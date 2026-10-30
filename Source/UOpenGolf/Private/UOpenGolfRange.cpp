// SPDX-License-Identifier: MIT
#include "UOpenGolfRange.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "Hole/OpenGolfCupComponent.h"
#include "HAL/IConsoleManager.h"
#include "OpenFairwayBall.h"
#include "OpenGolfFlagstickComponent.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfWindSubsystem.h"
#include "UOpenGolfGameMode.h"

namespace
{
    TAutoConsoleVariable<int32> CVarGrass3D(TEXT("uopengolf.Grass3D"), 1,
        TEXT("3D grass and ground cover on the terrain: 0 hides it, 1 shows it."), ECVF_Default);
    TAutoConsoleVariable<float> CVarWindMph(TEXT("uopengolf.WindMph"), 0.0f,
        TEXT("Wind speed in mph at 10 m for the range ball, flag and vegetation. Applied when it changes."), ECVF_Default);
    TAutoConsoleVariable<float> CVarWindFromDeg(TEXT("uopengolf.WindFromDeg"), 0.0f,
        TEXT("Direction the wind blows from, degrees clockwise from the target line: 0 headwind, 90 from the right, 180 tailwind."), ECVF_Default);
    TAutoConsoleVariable<float> CVarGreenStimp(TEXT("uopengolf.GreenStimp"), 10.0f,
        TEXT("Green speed for the range ball as a stimp reading in feet (8-15). Applied when it changes, from the next shot."), ECVF_Default);
    constexpr float CmPerM = 100.0f;
} // namespace

AUOpenGolfRange::AUOpenGolfRange()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Tee"));
    RootComponent->SetMobility(EComponentMobility::Static);
    Cup = CreateDefaultSubobject<UOpenGolfCupComponent>(TEXT("Cup"));
    Cup->SetupAttachment(RootComponent);
}

template <typename T>
T* AUOpenGolfRange::AddLayoutComponent(FName Name, const TCHAR* Folder)
{
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.ObjectFlags |= RF_Transactional;
    AActor* Actor = GetWorld()->SpawnActor<AActor>(Params);
    auto* Component = NewObject<T>(Actor, Name, RF_Transactional);
    Actor->SetRootComponent(Component);
    Actor->AddInstanceComponent(Component);
    Component->SetMobility(EComponentMobility::Static);
    Component->SetupAttachment(RootComponent);
    // Register after the layout has its final transforms, including in runtime fallback maps.
#if WITH_EDITOR
    Actor->SetActorLabel(Name.ToString());
    Actor->SetFolderPath(FName(Folder));
#endif
    LayoutActors.Add(Actor);
    return Component;
}

void AUOpenGolfRange::BuildDefaultLayout()
{
    LayoutActors.RemoveAll([](const TObjectPtr<AActor>& Actor)
        { return !IsValid(Actor); });
    if (!GetWorld() || IsTemplate() || !LayoutActors.IsEmpty())
        return;
    Modify();
    PostProcess = AddLayoutComponent<UPostProcessComponent>(TEXT("RangeView"), TEXT("Range/Lighting"));
    PostProcess->bUnbound = true;

    // The terrain area supplies the ground; AlignLayoutToTerrain puts the flag on it.
    Flagstick = AddLayoutComponent<UOpenGolfFlagstickComponent>(TEXT("Flagstick"), TEXT("Range/Targets"));
    Flagstick->SetRelativeLocation(FVector(HoleDistanceM * CmPerM, 0, 0));
    // Like the rest of the range, it is movable; its cloth child moves every frame.
    Flagstick->SetMobility(EComponentMobility::Movable);

    UDirectionalLightComponent* Sun = AddLayoutComponent<UDirectionalLightComponent>(TEXT("Sun"), TEXT("Range/Lighting"));
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->SetRelativeRotation(FRotator(-40.0f, 35.0f, 0.0f));
    Sun->SetAtmosphereSunLight(true);

    AddLayoutComponent<USkyAtmosphereComponent>(TEXT("Sky"), TEXT("Range/Lighting"));
    USkyLightComponent* SkyLight = AddLayoutComponent<USkyLightComponent>(TEXT("SkyLight"), TEXT("Range/Lighting"));
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->SetRealTimeCaptureEnabled(false);
    for (const auto& Actor : LayoutActors)
        Actor->RegisterAllComponents();
    RefreshPresentation();
}

void AUOpenGolfRange::BeginPlay()
{
    Super::BeginPlay();
    AppliedWindVarMph = CVarWindMph.GetValueOnGameThread();
    AppliedWindVarFromDeg = CVarWindFromDeg.GetValueOnGameThread();
    AppliedWindBall.Reset();
    ApplyWindSettings();
    BuildDefaultLayout();
    RefreshPresentation();
    if (IsValid(Flagstick))
    {
        FVector Ground, Normal = FVector::UpVector;
        if (IsValid(Terrain) && Terrain->GroundAt(Flagstick->GetComponentLocation(), Ground, Normal))
            Flagstick->SetWorldLocation(Ground);
        Cup->PlaceAtFlagstick(Flagstick, Normal);
    }
}

FVector AUOpenGolfRange::GetCupLocation() const
{
    if (Cup && Cup->GetFlagstick())
        return Cup->GetCupLocation();
    return IsValid(Flagstick) ? Flagstick->GetComponentLocation() : GetTeeLocation();
}

FVector AUOpenGolfRange::GetTeeLocation() const
{
    const FVector Origin = GetActorLocation();
    if (!IsValid(Terrain))
        return Origin;
    // The player controller asks every frame and the ground is fixed during Play; editing can rebuild it.
    const bool bCache = GetWorld() && GetWorld()->IsGameWorld();
    if (bCache && CachedTee.IsSet() && CachedTeeOrigin.Equals(Origin) && CachedTeeTerrain.Get() == Terrain)
        return CachedTee.GetValue();
    FVector Ground, Normal;
    if (!Terrain->GroundAt(Origin, Ground, Normal))
        return Origin;
    if (bCache)
    {
        CachedTee = Ground;
        CachedTeeOrigin = Origin;
        CachedTeeTerrain = Terrain;
    }
    return Ground;
}

void AUOpenGolfRange::AlignLayoutToTerrain()
{
    if (!IsValid(Terrain))
        return;
    for (const auto& Actor : LayoutActors)
    {
        if (!IsValid(Actor))
            continue;
        TArray<USceneComponent*> Components;
        Actor->GetComponents(Components);
        for (auto* Component : Components)
        {
            FVector Ground, Normal;
            if (Component == Flagstick && Terrain->GroundAt(Component->GetComponentLocation(), Ground, Normal))
            {
                Component->Modify();
                Component->SetWorldLocation(Ground);
            }
        }
    }
}

void AUOpenGolfRange::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    ApplyWindSettings();
    if (!LayoutActors.IsEmpty())
        RefreshPresentation();
}

void AUOpenGolfRange::RefreshPresentation()
{
    if (!IsValid(PostProcess))
        return;
    bGrassEnabled = false;
    bTerrainGrassApplied = false;
    // A fast follow camera smears the ground with motion blur; the fixed exposure keeps
    // the sunlit grass from washing out.
    FPostProcessSettings& View = PostProcess->Settings;
    View.bOverride_MotionBlurAmount = true;
    View.MotionBlurAmount = 0.0f;
    View.bOverride_AutoExposureBias = true;
    View.AutoExposureBias = ExposureBiasEV;
    UpdateGrass();
    TArray<USkyLightComponent*> Lights;
    GetLayoutComponents(Lights);
    for (auto* Light : Lights)
        Light->RecaptureSky();
    bFlagWindApplied = false;
    SyncWindToScene();
}

void AUOpenGolfRange::ApplyWindSettings()
{
    UWorld* World = GetWorld();
    if (!World)
        return;

    if (World->IsGameWorld() && HasActorBegunPlay())
    {
        const float Mph = CVarWindMph.GetValueOnGameThread();
        const float FromDeg = CVarWindFromDeg.GetValueOnGameThread();
        if (Mph != AppliedWindVarMph || FromDeg != AppliedWindVarFromDeg)
        {
            WindSpeedMph = Mph;
            WindFromDeg = FromDeg;
            AppliedWindVarMph = Mph;
            AppliedWindVarFromDeg = FromDeg;
        }
    }
    WindSpeedMph = FMath::IsFinite(WindSpeedMph) ? FMath::Max(0.0f, WindSpeedMph) : 0.0f;
    WindFromDeg = FMath::IsFinite(WindFromDeg) ? FRotator::ClampAxis(WindFromDeg) : 0.0f;
    const AUOpenGolfGameMode* Mode = World->GetAuthGameMode<AUOpenGolfGameMode>();
    AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Ball)
        return;
    if (AppliedWindBall.Get() == Ball && WindSpeedMph == AppliedRangeWindMph && WindFromDeg == AppliedRangeWindFromDeg)
        return;
    Ball->WindSpeedMph = WindSpeedMph;
    Ball->WindFromDeg = WindFromDeg;
    AppliedRangeWindMph = WindSpeedMph;
    AppliedRangeWindFromDeg = WindFromDeg;
    AppliedWindBall = Ball;
}

void AUOpenGolfRange::ApplyGreenConsoleVariable()
{
    // Only a change applies, so the default never overwrites a stimp set on the ball another way.
    const float StimpFeet = CVarGreenStimp.GetValueOnGameThread();
    if (StimpFeet == AppliedGreenStimpVar)
        return;
    const AUOpenGolfGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Ball)
        return;
    Ball->GreenStimpFeet = OpenFairway::GreenSpeed::Clamp(StimpFeet);
    AppliedGreenStimpVar = StimpFeet;
}

void AUOpenGolfRange::SyncWindToScene()
{
    UWorld* World = GetWorld();
    if (!World)
        return;
    OpenFairway::FAirConditions PreviewAir;
    PreviewAir.WindSpeedMph = WindSpeedMph;
    PreviewAir.WindFromDeg = WindFromDeg;
    const FVector3f PreviewWind = PreviewAir.WindVelocity();
    const AUOpenGolfGameMode* Mode = World->GetAuthGameMode<AUOpenGolfGameMode>();
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    // The ball's wind and height profile, which a moving shot keeps from its launch; before there is a ball,
    // the saved wind. The flag and vegetation then meet the wind the ball flies through.
    const FVector Wind = Ball ? Ball->GetWindVelocityMps() : FVector(PreviewWind.X, PreviewWind.Z, PreviewWind.Y);
    const float AirDensity = Ball ? Ball->GetAirDensityKgM3() : PreviewAir.Density();
    const float ReferenceHeightM = Ball ? Ball->GetWindReferenceHeightM() : PreviewAir.WindReferenceHeightM;
    const float RoughnessM = Ball ? Ball->GetWindRoughnessM() : PreviewAir.WindRoughnessM;
    // Vegetation animates in any Realtime viewport, so outside Play it sees wind only while previewing.
    if (UOpenGolfWindSubsystem* WorldWind = World->GetSubsystem<UOpenGolfWindSubsystem>())
        WorldWind->SetWind(World->IsGameWorld() || bPreviewWindInEditor ? Wind : FVector::ZeroVector, AirDensity, ReferenceHeightM, RoughnessM);

    if (!IsValid(Flagstick))
        return;
    Flagstick->bTickInEditor = World->WorldType == EWorldType::Editor && bPreviewWindInEditor;
    // The flag meets the wind at its own height, below the 10 m reading the controls author.
    constexpr float FlagHeightM = UOpenGolfFlagstickComponent::FlagCentreHeightCm / 100.0f;
    const FVector FlagWind = Wind * OpenFairway::WindProfile::Factor(FlagHeightM, ReferenceHeightM, RoughnessM);
    if (bFlagWindApplied && FlagWind.Equals(AppliedWindMps, 0.01) && FMath::IsNearlyEqual(AirDensity, AppliedAirDensityKgM3, 0.001f))
        return;
    Flagstick->SetWind(FlagWind, AirDensity);
    bFlagWindApplied = true;
    AppliedWindMps = FlagWind;
    AppliedAirDensityKgM3 = AirDensity;
}

void AUOpenGolfRange::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetWorld() && !GetWorld()->IsGameWorld())
    {
        ApplyWindSettings();
        SyncWindToScene();
        return;
    }
    UpdateGrass();
    ApplyWindSettings();
    ApplyGreenConsoleVariable();
    SyncWindToScene();
}

int32 AUOpenGolfRange::GetGrassInstanceCount() const
{
    int32 Count = 0;
    if (!IsValid(Terrain))
        return Count;
    for (AActor* Actor : Terrain->GeneratedActors)
    {
        if (!IsValid(Actor) || !Actor->ActorHasTag(AOpenGolfTerrainArea::VegetationTag))
            continue;
        TArray<UInstancedStaticMeshComponent*> Components;
        Actor->GetComponents(Components);
        for (auto* Component : Components)
            if (Component->IsVisible())
                Count += Component->GetInstanceCount();
    }
    return Count;
}

int32 AUOpenGolfRange::GetTreeInstanceCount() const
{
    int32 Count = 0;
    if (!IsValid(Terrain))
        return Count;
    for (AActor* Actor : Terrain->GeneratedActors)
    {
        if (!IsValid(Actor) || !Actor->ActorHasTag(AOpenGolfTerrainArea::TreesTag))
            continue;
        TArray<UInstancedStaticMeshComponent*> Components;
        Actor->GetComponents(Components);
        for (auto* Component : Components)
            Count += Component->GetInstanceCount();
    }
    return Count;
}

void AUOpenGolfRange::UpdateGrass()
{
    if (!IsValid(Terrain))
        return;
    const bool bWanted = CVarGrass3D.GetValueOnGameThread() != 0;
    // Applying touches every vegetation component, so it runs again only when the switch changes.
    if (bTerrainGrassApplied && bWanted == bGrassEnabled)
        return;
    for (const auto& Actor : Terrain->GeneratedActors)
    {
        if (!IsValid(Actor) || !Actor->ActorHasTag(AOpenGolfTerrainArea::VegetationTag))
            continue;
        TArray<UInstancedStaticMeshComponent*> Instances;
        Actor->GetComponents(Instances);
        for (auto* Component : Instances)
            Component->SetVisibility(bWanted);
    }
    bGrassEnabled = bWanted;
    bTerrainGrassApplied = true;
}
