// SPDX-License-Identifier: MIT
#include "UOpenGolfRange.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "HAL/IConsoleManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OpenFairwayBall.h"
#include "OpenFairwayPhysicalMaterial.h"
#include "OpenGolfCourse.h"
#include "OpenGolfFlagstickComponent.h"
#include "OpenGolfTerrain.h"
#include "UObject/ConstructorHelpers.h"
#include "UOpenGolfGameMode.h"
#include "Components/InstancedStaticMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogUOpenGolfRange, Log, All);

namespace
{
    TAutoConsoleVariable<int32> CVarGrass3D(TEXT("uopengolf.Grass3D"), 1,
        TEXT("Decorative grass near the tee: 0 releases instances, 1 enables them."), ECVF_Default);
    TAutoConsoleVariable<float> CVarWindMph(TEXT("uopengolf.WindMph"), 0.0f,
        TEXT("Wind speed in mph for the range ball and flag. Applied when it changes."), ECVF_Default);
    TAutoConsoleVariable<float> CVarWindFromDeg(TEXT("uopengolf.WindFromDeg"), 0.0f,
        TEXT("Direction the wind blows from, degrees clockwise from the target line: 0 headwind, 90 from the right, 180 tailwind."), ECVF_Default);
    TAutoConsoleVariable<float> CVarGreenStimp(TEXT("uopengolf.GreenStimp"), 10.0f,
        TEXT("Green speed for the range ball as a stimp reading in feet (8-15). Applied when it changes, from the next shot."), ECVF_Default);
    constexpr float CmPerM = 100.0f;
    constexpr float GroundThicknessM = 1.0f;
    // The public clump's ~1 cm blades drop below two pixels from the tee camera past about
    // 10 m and shimmer under temporal AA, so public grass ends closer than the class defaults.
    constexpr int32 PublicGrassCullStartCm = 700;
    constexpr int32 PublicGrassCullEndCm = 1000;

    // Range layout in meters: X downrange from the tee, Y to the right.
    constexpr float RangeStartM = -10.0f;
    constexpr float RangeEndM = 320.0f;
    constexpr float RoughHalfWidthM = 70.0f;
    constexpr float GreenHalfSizeM = 15.0f;
    constexpr float FringeWidthM = 2.0f;

    const FLinearColor TeeColor = FLinearColor::FromSRGBColor(FColor(0x40, 0x6B, 0x1F));
} // namespace

const FName AUOpenGolfRange::TurfTag(TEXT("UOpenGolf.Turf"));
const FName AUOpenGolfRange::RoughTag(TEXT("UOpenGolf.Rough"));
const FName AUOpenGolfRange::GreenTag(TEXT("UOpenGolf.Green"));

AUOpenGolfRange::AUOpenGolfRange()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Tee"));
    RootComponent->SetMobility(EComponentMobility::Static);

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeMesh = Cube.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> FairwayMaterialFinder(OpenGolfCourse::FairwayMaterialPath);
    FairwayMaterial = FairwayMaterialFinder.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> RoughMaterialFinder(OpenGolfCourse::RoughMaterialPath);
    RoughMaterial = RoughMaterialFinder.Object;

    Grass = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Grass3D"));
    Grass->SetupAttachment(RootComponent);
    Grass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Grass->SetGenerateOverlapEvents(false);
    Grass->SetCanEverAffectNavigation(false);
    Grass->SetMobility(EComponentMobility::Movable);
    Grass->CastShadow = false;
    Grass->SetCullDistances(1200, 2200);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassMesh(OpenGolfCourse::GrassClumpPath);
    DefaultGrassMesh = GrassMesh.Object;
    Grass->SetStaticMesh(DefaultGrassMesh);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Shape(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    ShapeMaterial = Shape.Object;
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

    const float GreenStart = GreenDistanceM - GreenHalfSizeM;
    const float GreenEnd = GreenDistanceM + GreenHalfSizeM;
    static_assert(FairwayHalfWidthM > GreenHalfSizeM, "The fringe sides need room between the green and the rough");
    static_assert(GreenHalfSizeM * CmPerM > OpenGolfCourse::GreenDiscRadiusCm, "The putting green sits inside the square under it");

    // Ground pieces tile without overlapping, with their tops at the tee's height. The
    // former green and fringe keep fairway turf and physics for calibration shots; the
    // side strips are rough. The ball resolves each contacted mesh's physical material.
    UMaterialInterface* Fairway = FairwayMaterial;
    UMaterialInterface* Rough = RoughMaterial;
    AddBox(TEXT("FairwayFront"), FVector(RangeStartM, -FairwayHalfWidthM, 0), FVector(GreenStart - FringeWidthM, FairwayHalfWidthM, 0), true, Fairway);
    AddBox(TEXT("FairwayBack"), FVector(GreenEnd + FringeWidthM, -FairwayHalfWidthM, 0), FVector(RangeEndM, FairwayHalfWidthM, 0), true, Fairway);
    AddBox(TEXT("RoughLeft"), FVector(RangeStartM, -RoughHalfWidthM, 0), FVector(RangeEndM, -FairwayHalfWidthM, 0), true, Rough);
    AddBox(TEXT("RoughRight"), FVector(RangeStartM, FairwayHalfWidthM, 0), FVector(RangeEndM, RoughHalfWidthM, 0), true, Rough);
    auto* Surround = AddLayoutComponent<UStaticMeshComponent>(TEXT("Green"), TEXT("Range/Ground"));
    Surround->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/OpenGolfCourse/Surfaces/SM_GreenSurround.SM_GreenSurround")));
    Surround->SetRelativeLocation(FVector(GreenDistanceM * CmPerM, 0, 0));
    Surround->SetMaterial(0, Fairway);
    Surround->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    Surround->ComponentTags.Add(TurfTag);
    // Surround the original layout with rough, leaving no overlapping collision tops.
    AddBox(TEXT("RoughBeyondLeft"), FVector(-1100, -1100, 0), FVector(1100, -RoughHalfWidthM, 0), true, Rough);
    AddBox(TEXT("RoughBeyondRight"), FVector(-1100, RoughHalfWidthM, 0), FVector(1100, 1100, 0), true, Rough);
    AddBox(TEXT("RoughBeyondBack"), FVector(-1100, -RoughHalfWidthM, 0), FVector(RangeStartM, RoughHalfWidthM, 0), true, Rough);
    AddBox(TEXT("RoughBeyondFront"), FVector(RangeEndM, -RoughHalfWidthM, 0), FVector(1100, RoughHalfWidthM, 0), true, Rough);
    AddBox(TEXT("FringeFront"), FVector(GreenStart - FringeWidthM, -FairwayHalfWidthM, 0), FVector(GreenStart, FairwayHalfWidthM, 0), true, Fairway);
    AddBox(TEXT("FringeBack"), FVector(GreenEnd, -FairwayHalfWidthM, 0), FVector(GreenEnd + FringeWidthM, FairwayHalfWidthM, 0), true, Fairway);
    AddBox(TEXT("FringeLeft"), FVector(GreenStart, -FairwayHalfWidthM, 0), FVector(GreenEnd, -GreenHalfSizeM, 0), true, Fairway);
    AddBox(TEXT("FringeRight"), FVector(GreenStart, GreenHalfSizeM, 0), FVector(GreenEnd, FairwayHalfWidthM, 0), true, Fairway);

    // Presentation only (no collision): tee box paint and flag.
    AddBox(TEXT("TeeBox"), FVector(-2.5f, -2.5f, 0.0f), FVector(2.5f, 2.5f, 0.005f), false);
    Flagstick = AddLayoutComponent<UOpenGolfFlagstickComponent>(TEXT("Flagstick"), TEXT("Range/Targets"));
    Flagstick->SetRelativeLocation(FVector(GreenDistanceM * CmPerM, 0, 0));
    // Like the rest of the range, it is movable; its cloth child moves every frame.
    Flagstick->SetMobility(EComponentMobility::Movable);

    // The putting green: a disc 1 mm above the square under it, whose convex hull bevels down to
    // the ground at its edge. MI_Green declares PM_Green, so the ball plays it as Green; the
    // square stays fairway around it. It casts no shadow onto the turf a millimeter below.
    PuttingGreen = AddLayoutComponent<UStaticMeshComponent>(TEXT("PuttingGreen"), TEXT("Range/Ground"));
    PuttingGreen->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, OpenGolfCourse::GreenDiscMeshPath));
    PuttingGreen->SetRelativeLocation(FVector(GreenDistanceM * CmPerM, 0, 0));

    PuttingGreen->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    PuttingGreen->CastShadow = false;
    PuttingGreen->ComponentTags.Add(GreenTag);

    // Presentation only (no collision): ground out to the horizon so the void past the
    // range is not visible. It sits just below the ground tops, hidden under the boxes.
    UStaticMeshComponent* Backdrop = AddLayoutComponent<UStaticMeshComponent>(TEXT("Backdrop"), TEXT("Range/Scenery"));
    Backdrop->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")));
    Backdrop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Backdrop->SetCanEverAffectNavigation(false);
    Backdrop->CastShadow = false;
    // The engine plane is 100 cm square; this one is 4 km square, centered downrange.
    Backdrop->SetRelativeLocation(FVector(1500.0f * CmPerM, 0.0f, -2.0f));
    Backdrop->SetRelativeScale3D(FVector(4000.0f, 4000.0f, 1.0f));
    Backdrop->SetMaterial(0, RoughMaterial);
    Backdrop->ComponentTags.Add(RoughTag);

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

UStaticMeshComponent* AUOpenGolfRange::AddBox(FName Name, const FVector& MinM, const FVector& MaxM, bool bCollides,
    UMaterialInterface* SurfaceMaterial)
{
    // Collision boxes are a meter thick below the ground plane; decorations use their own bounds.
    const FVector Min = bCollides ? FVector(MinM.X, MinM.Y, -GroundThicknessM) : MinM;
    const FVector Max = bCollides ? FVector(MaxM.X, MaxM.Y, 0.0f) : MaxM;

    UStaticMeshComponent* Box = AddLayoutComponent<UStaticMeshComponent>(Name, bCollides ? TEXT("Range/Ground") : TEXT("Range/Tee"));
    Box->SetStaticMesh(CubeMesh);
    // The engine cube is 100 cm (1 m) on a side, centered on its origin.
    Box->SetRelativeLocation((Min + Max) * 0.5f * CmPerM);
    Box->SetRelativeScale3D(Max - Min);

    if (bCollides)
    {
        Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    }
    else
    {
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Box->CastShadow = Name != TEXT("TeeBox");
    }
    if (SurfaceMaterial)
    {
        // Optional turf covers the fairway strip; optional rough covers the side strips.
        Box->ComponentTags.Add(SurfaceMaterial == RoughMaterial ? RoughTag : TurfTag);
        Box->SetMaterial(0, SurfaceMaterial);
    }
    else if (Name == TEXT("TeeBox"))
    {
        // World-aligned turf continues the fairway texture; BeginPlay tints it the tee color.
        Box->SetMaterial(0, FairwayMaterial);
        TeeBox = Box;
    }
    else
    {
        Box->SetMaterial(0, ShapeMaterial);
    }
    return Box;
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
            const bool Flag = Component == Flagstick;
            if (!Flag && Component != TeeBox)
                continue;
            FVector Ground, Normal;
            if (Terrain->GroundAt(Component->GetComponentLocation(), Ground, Normal))
            {
                // The painted tee box sits just above the ground.
                const double Offset = Flag ? 0.0 : 0.5;
                Component->Modify();
                Component->SetWorldLocation(Ground + FVector::UpVector * Offset);
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

void AUOpenGolfRange::ApplyPresentationDefaults(const AUOpenGolfRange& Defaults)
{
    GrassMeshes = Defaults.GrassMeshes;
    GroundMaterialOverride = Defaults.GroundMaterialOverride;
    RoughMaterialOverride = Defaults.RoughMaterialOverride;
    GrassSpacingCm = Defaults.GrassSpacingCm;
    GrassRadiusCm = Defaults.GrassRadiusCm;
    GrassScale = Defaults.GrassScale;
    GrassCullStartCm = Defaults.GrassCullStartCm;
    GrassCullEndCm = Defaults.GrassCullEndCm;
    ExposureBiasEV = Defaults.ExposureBiasEV;
    RefreshPresentation();
}

void AUOpenGolfRange::RefreshPresentation()
{
    if (!IsValid(PostProcess))
        return;
    for (const auto& Component : GrassComponents)
        if (IsValid(Component) && Component != Grass)
            Component->DestroyComponent();
    GrassComponents.Reset();
    Grass->ClearInstances();
    bGrassEnabled = false;
    bTerrainGrassApplied = false;
    Grass->SetStaticMesh(DefaultGrassMesh);
    GrassComponents.Add(Grass);
    if (!GrassMeshes.IsEmpty())
    {
        Grass->SetStaticMesh(GrassMeshes[0]);
        for (int32 Index = 1; Index < GrassMeshes.Num(); ++Index)
        {
            auto* Component = NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
            Component->SetupAttachment(RootComponent);
            Component->SetMobility(EComponentMobility::Movable);
            Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Component->SetGenerateOverlapEvents(false);
            Component->SetCanEverAffectNavigation(false);
            Component->CastShadow = false;
            Component->SetStaticMesh(GrassMeshes[Index]);
            Component->RegisterComponent();
            GrassComponents.Add(Component);
        }
    }
    const bool bPublicGrass = GrassMeshes.IsEmpty();
    const int32 CullStartCm = bPublicGrass ? PublicGrassCullStartCm : FMath::Max(0, GrassCullStartCm);
    const int32 CullEndCm = bPublicGrass ? PublicGrassCullEndCm : FMath::Max(GrassCullStartCm + 1, GrassCullEndCm);
    for (const auto& Component : GrassComponents)
    {
        // Calm practice turf: block material wind and player-interaction displacement.
        Component->SetEvaluateWorldPositionOffset(false);
        Component->SetCullDistances(CullStartCm, CullEndCm);
    }
    TArray<UStaticMeshComponent*> Components;
    GetLayoutComponents(Components);
    for (auto* Component : Components)
    {
        // Physics follows the kit's surface material even when an override replaces its look.
        UMaterialInterface* SurfaceMaterial = Component->GetMaterial(0);
        if (SurfaceMaterial && Component->GetCollisionEnabled() != ECollisionEnabled::NoCollision && (Component->ComponentHasTag(TurfTag) || Component->ComponentHasTag(RoughTag) || Component->ComponentHasTag(GreenTag)))
        {
            UPhysicalMaterial* Physical = Component->ComponentHasTag(GreenTag) ? LoadObject<UOpenFairwayPhysicalMaterial>(nullptr, *OpenGolfCourse::SurfacePhysicalMaterialPath(TEXT("Green")))
                                                                               : (Component->ComponentHasTag(RoughTag) ? RoughMaterial : FairwayMaterial)->GetPhysicalMaterial();
            // Without a declared surface the ball silently plays its default surface here.
            if (!Cast<UOpenFairwayPhysicalMaterial>(Physical))
                UE_LOG(LogUOpenGolfRange, Warning, TEXT("%s: %s has no OpenFairway physical material (%s), so it plays as the ball's default surface. Rerun BuildCourseAssets.py."),
                    *Component->GetName(), *SurfaceMaterial->GetName(), Physical ? *Physical->GetName() : TEXT("none"));
            Component->SetPhysMaterialOverride(Physical);
        }
        if (GroundMaterialOverride && Component->ComponentHasTag(TurfTag))
            Component->SetMaterial(0, GroundMaterialOverride);
        else if (RoughMaterialOverride && Component->ComponentHasTag(RoughTag))
            Component->SetMaterial(0, RoughMaterialOverride);
    }
    // A fast follow camera smears the ground with motion blur; the fixed exposure keeps
    // the sunlit grass from washing out.
    FPostProcessSettings& View = PostProcess->Settings;
    View.bOverride_MotionBlurAmount = true;
    View.MotionBlurAmount = 0.0f;
    View.bOverride_AutoExposureBias = true;
    View.AutoExposureBias = ExposureBiasEV;
    UpdateGrass();
    if (TeeBox)
    {
        auto* Material = Cast<UMaterialInstanceDynamic>(TeeBox->GetMaterial(0));
        if (!Material)
            Material = TeeBox->CreateDynamicMaterialInstance(0);
        if (Material)
        {
            Material->SetVectorParameterValue(TEXT("GrassDarkColor"), TeeColor);
            Material->SetVectorParameterValue(TEXT("GrassLightColor"), TeeColor);
        }
    }
    TArray<USkyLightComponent*> Lights;
    GetLayoutComponents(Lights);
    for (auto* Light : Lights)
        Light->RecaptureSky();
    bFlagWindApplied = false;
    SyncFlagToWind();
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

void AUOpenGolfRange::SyncFlagToWind()
{
    if (!IsValid(Flagstick))
        return;
    Flagstick->bTickInEditor = GetWorld() && GetWorld()->WorldType == EWorldType::Editor && bPreviewWindInEditor;
    OpenFairway::FAirConditions PreviewAir;
    PreviewAir.WindSpeedMph = WindSpeedMph;
    PreviewAir.WindFromDeg = WindFromDeg;
    const FVector3f PreviewWind = PreviewAir.WindVelocity();
    const AUOpenGolfGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    const FVector Wind = Ball ? Ball->GetWindVelocityMps() : FVector(PreviewWind.X, PreviewWind.Z, PreviewWind.Y);
    const float AirDensity = Ball ? Ball->GetAirDensityKgM3() : PreviewAir.Density();
    if (bFlagWindApplied && Wind.Equals(AppliedWindMps, 0.01) && FMath::IsNearlyEqual(AirDensity, AppliedAirDensityKgM3, 0.001f))
        return;
    Flagstick->SetWind(Wind, AirDensity);
    bFlagWindApplied = true;
    AppliedWindMps = Wind;
    AppliedAirDensityKgM3 = AirDensity;
}

void AUOpenGolfRange::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetWorld() && !GetWorld()->IsGameWorld())
    {
        ApplyWindSettings();
        SyncFlagToWind();
        return;
    }
    UpdateGrass();
    ApplyWindSettings();
    ApplyGreenConsoleVariable();
    SyncFlagToWind();
}

int32 AUOpenGolfRange::GetGrassInstanceCount() const
{
    int32 Count = 0;
    if (IsValid(Terrain))
    {
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
    for (const auto& Component : GrassComponents)
        Count += Component->GetInstanceCount();
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
    const bool bWanted = CVarGrass3D.GetValueOnGameThread() != 0;
    if (IsValid(Terrain))
    {
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
        for (const auto& Component : GrassComponents)
            if (Component->GetInstanceCount() > 0)
                Component->ClearInstances();
        bGrassEnabled = bWanted;
        bTerrainGrassApplied = true;
        return;
    }
    if (bWanted == bGrassEnabled)
        return;
    bGrassEnabled = bWanted;
    if (!bWanted)
    {
        for (const auto& Component : GrassComponents)
            Component->ClearInstances();
        return;
    }

    FRandomStream Random(20261015);
    TArray<TArray<FTransform>> Transforms;
    Transforms.SetNum(GrassComponents.Num());
    const float Spacing = FMath::Clamp(GrassSpacingCm, 5.0f, 200.0f);
    const float Radius = FMath::Clamp(GrassRadiusCm, 100.0f, 5000.0f);
    // Deterministic coverage clipped to the fairway strip. Leave tee paint clear.
    const float MinX = RangeStartM * CmPerM;
    const float MaxAbsY = FairwayHalfWidthM * CmPerM;
    int32 InstanceIndex = 0;
    for (float X = FMath::Max(MinX, -Radius) + Spacing * 0.5f; X < Radius; X += Spacing)
    {
        for (float Y = FMath::Max(-MaxAbsY, -Radius) + Spacing * 0.5f; Y < FMath::Min(MaxAbsY, Radius); Y += Spacing)
        {
            const float PX = X + Random.FRandRange(-0.4f, 0.4f) * Spacing;
            const float PY = Y + Random.FRandRange(-0.4f, 0.4f) * Spacing;
            if (PX * PX + PY * PY > Radius * Radius || PX < MinX || FMath::Abs(PY) > MaxAbsY || (FMath::Abs(PX) < 255 && FMath::Abs(PY) < 255))
                continue;
            const float Scale = Random.FRandRange(0.75f, 1.15f);
            Transforms[InstanceIndex++ % GrassComponents.Num()].Emplace(FRotator(0, Random.FRandRange(0, 360), 0), FVector(PX, PY, 0.0f), GrassScale * Scale);
        }
    }
    for (int32 Index = 0; Index < GrassComponents.Num(); ++Index)
        GrassComponents[Index]->AddInstances(Transforms[Index], false, false, false);
}
