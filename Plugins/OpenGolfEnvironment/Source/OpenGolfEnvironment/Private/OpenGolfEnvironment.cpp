// SPDX-License-Identifier: MIT
#include "OpenGolfEnvironment.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, OpenGolfEnvironment)
DEFINE_LOG_CATEGORY_STATIC(LogOpenGolfEnvironment, Log, All);

namespace
{
    // Ray-march samples relative to Unreal's 96 per 15 km (about 156 m per step). One such step is
    // nearly opaque in the cumulus material, so the per-frame jittered start made clouds shimmer.
    // Four times (about 39 m) settles them; eight cost twice as much without measurably steadier clouds.
    constexpr float CloudViewSampleCountScale = 4;
    float Bounded(float Value, float Default, float Min, float Max)
    {
        return FMath::IsFinite(Value) ? FMath::Clamp(Value, Min, Max) : Default;
    }
    void Curve(FRuntimeFloatCurve& Target, float Morning, float Day, float Sunset)
    {
        auto* Data = Target.GetRichCurve();
        for (const auto Pair : {TPair<float, float>(0, Morning), TPair<float, float>(.5f, Day), TPair<float, float>(1, Sunset)})
        {
            auto Key = Data->AddKey(Pair.Key, Pair.Value);
            Data->SetKeyInterpMode(Key, RCIM_Cubic);
        }
        Data->AutoSetTangents();
    }
} // namespace
void FOpenGolfEnvironmentSettings::Sanitize()
{
    Daylight = Bounded(Daylight, .5f, 0, 1);
    CloudCoverage = Bounded(CloudCoverage, .35f, 0, 1);
    CloudSpeed = Bounded(CloudSpeed, 1, 0, 5);
    DurationMinutes = Bounded(DurationMinutes, 60, 1, 1440);
}
void FOpenGolfEnvironmentSettings::Advance(float Seconds)
{
    Sanitize();
    if (bAutomaticTime && FMath::IsFinite(Seconds) && Seconds > 0)
        Daylight = FMath::Min(1.f, Daylight + Seconds / (DurationMinutes * 60));
}
UOpenGolfEnvironmentProfile::UOpenGolfEnvironmentProfile()
{
    Curve(SunElevation, 15, 40, 7);
    Curve(SunIntensity, 11, 10, 18);
    Curve(SunTemperature, 5800, 6500, 5000);
    Curve(FogDensity, .012f, .0002f, .001f);
    CloudMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/OpenGolfEnvironment/Materials/M_Clouds.M_Clouds")));
}
AOpenGolfEnvironment::AOpenGolfEnvironment()
{
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Environment"));
    Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    Sky = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Sky"));
    SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
    Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("MorningMist"));
    for (USceneComponent* Part : TArray<USceneComponent*>{Sun, Sky, SkyLight, Clouds, Fog})
    {
        Part->SetupAttachment(RootComponent);
        Part->SetMobility(EComponentMobility::Movable);
    }
    Sun->SetAtmosphereSunLight(true);
    Sun->bUseTemperature = true;
    Sun->bCastCloudShadows = false;
    SkyLight->SetRealTimeCaptureEnabled(true);
    Clouds->SetLayerBottomAltitude(1.5f);
    Clouds->SetLayerHeight(2);
    Clouds->SetViewSampleCountScale(CloudViewSampleCountScale);
    Clouds->SetVisibleInRealTimeSkyCaptures(false);
    Fog->SetVolumetricFog(false);
    Fog->SetFogHeightFalloff(.8f);
}
AOpenGolfEnvironment* AOpenGolfEnvironment::Find(const UWorld* World)
{
    if (World)
    {
        TActorIterator<AOpenGolfEnvironment> It(World);
        if (It)
            return *It;
    }
    return nullptr;
}
const UOpenGolfEnvironmentProfile& AOpenGolfEnvironment::ActiveProfile() const
{
    return Profile ? *Profile : *GetDefault<UOpenGolfEnvironmentProfile>();
}
void AOpenGolfEnvironment::PostRegisterAllComponents()
{
    Super::PostRegisterAllComponents();
    // Dynamic materials are transient, so restore them when a saved map registers its components.
    if (!IsTemplate() && GetWorld())
    {
        if (HasActorBegunPlay())
            UpdateAtmosphere();
        else
            ResetToCourseDefaults();
    }
}
void AOpenGolfEnvironment::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ResetToCourseDefaults();
}
void AOpenGolfEnvironment::BeginPlay()
{
    Super::BeginPlay();
    ResetToCourseDefaults();
}
#if WITH_EDITOR
void AOpenGolfEnvironment::PostEditChangeProperty(FPropertyChangedEvent& Event)
{
    Super::PostEditChangeProperty(Event);
    ResetToCourseDefaults();
}
#endif
void AOpenGolfEnvironment::ApplySettings(FOpenGolfEnvironmentSettings Settings, bool bNotify)
{
    Settings.Sanitize();
    Current = Settings;
    UpdateAtmosphere();
    if (bNotify)
        OnSettingsChanged.Broadcast();
}
void AOpenGolfEnvironment::ResetToCourseDefaults()
{
    CloudOffset = 0;
    ApplySettings(bOverrideStartingSettings ? StartingSettings : ActiveProfile().StartingSettings);
}
void AOpenGolfEnvironment::SelectPreset(EOpenGolfDaylightPreset Preset)
{
    auto Settings = Current;
    Settings.Daylight = Preset == EOpenGolfDaylightPreset::Morning ? 0 : Preset == EOpenGolfDaylightPreset::Sunset ? 1
                                                                                                                   : .5f;
    ApplySettings(Settings);
}
void AOpenGolfEnvironment::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!GetWorld() || GetWorld()->IsPaused() || (!GetWorld()->IsGameWorld() && !bPreviewAnimationInEditor))
        return;
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0)
        return;
    const float Previous = Current.Daylight;
    Current.Advance(DeltaSeconds);
    // Offset is in kilometres. Keep drift gentle and independent of golf wind.
    CloudOffset += DeltaSeconds * Current.CloudSpeed * .004f;
    if (Previous != Current.Daylight)
        UpdateAtmosphere();
    else if (CloudInstance && Current.CloudSpeed > 0)
        CloudInstance->SetScalarParameterValue(TEXT("CloudOffset"), CloudOffset);
}
void AOpenGolfEnvironment::UpdateAtmosphere()
{
    const auto& Data = ActiveProfile();
    const float T = Current.Daylight;
    Sun->SetWorldRotation(FRotator(-Data.SunElevation.GetRichCurveConst()->Eval(T), Data.SunOrientationDegrees + (T - .5f) * 160, 0));
    Sun->SetIntensity(FMath::Max(0.f, Data.SunIntensity.GetRichCurveConst()->Eval(T)));
    Sun->SetTemperature(Data.SunTemperature.GetRichCurveConst()->Eval(T));
    Fog->SetFogDensity(FMath::Max(0.f, Data.FogDensity.GetRichCurveConst()->Eval(T)));
    if (CloudMaterialPath != Data.CloudMaterial.ToSoftObjectPath() || (!CloudInstance && Data.CloudMaterial.IsValid()))
    {
        CloudMaterialPath = Data.CloudMaterial.ToSoftObjectPath();
        auto* LoadedCloudMaterial = Data.CloudMaterial.LoadSynchronous();
        CloudInstance = LoadedCloudMaterial ? UMaterialInstanceDynamic::Create(LoadedCloudMaterial, this) : nullptr;
        if (CloudInstance)
            CloudInstance->SetFlags(RF_Transient | RF_DuplicateTransient);
        if (!LoadedCloudMaterial && !CloudMaterialPath.IsNull())
            UE_LOG(LogOpenGolfEnvironment, Warning, TEXT("Could not load cloud material %s."), *CloudMaterialPath.ToString());
    }
    if (Clouds->GetMaterial() != CloudInstance)
        Clouds->SetMaterial(CloudInstance);
    // A cloud shader that is still compiling can leave the 5.8 skylight capture with an unwritten render target.
    Clouds->SetVisibleInRealTimeSkyCaptures(false);
    Clouds->SetVisibility(CloudInstance != nullptr && Current.CloudCoverage > 0);
    if (CloudInstance)
    {
        CloudInstance->SetScalarParameterValue(TEXT("Coverage"), Current.CloudCoverage);
        CloudInstance->SetScalarParameterValue(TEXT("CloudOffset"), CloudOffset);
    }
}
