// SPDX-License-Identifier: MIT
#include "UOpenGolfRange.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "HAL/IConsoleManager.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OpenFairwayBall.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    TAutoConsoleVariable<int32> CVarGrass3D(TEXT("uopengolf.Grass3D"), 1,
        TEXT("Decorative grass near the tee: 0 releases instances, 1 enables them."), ECVF_Default);
    constexpr float CmPerM = 100.0f;
    constexpr float MetersPerYard = 0.9144f;
    constexpr float GroundThicknessM = 1.0f;

    // Range layout in meters: X downrange from the tee, Y to the right.
    constexpr float RangeStartM = -10.0f;
    constexpr float RangeEndM = 320.0f;
    constexpr float FairwayHalfWidthM = 20.0f;
    constexpr float RoughHalfWidthM = 70.0f;
    constexpr float GreenHalfSizeM = 15.0f;
    constexpr float FringeWidthM = 2.0f;

    const FLinearColor FairwayColor(0.10f, 0.42f, 0.10f);
    const FLinearColor TeeColor = FLinearColor::FromSRGBColor(FColor(0x40, 0x6B, 0x1F));
    const FLinearColor MarkerColor(0.95f, 0.95f, 0.95f);
} // namespace

AUOpenGolfRange::AUOpenGolfRange()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = 0.25f;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Tee"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeMesh = Cube.Object;

    Grass = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Grass3D"));
    Grass->SetupAttachment(RootComponent);
    Grass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Grass->SetGenerateOverlapEvents(false);
    Grass->SetCanEverAffectNavigation(false);
    Grass->SetMobility(EComponentMobility::Movable);
    Grass->CastShadow = false;
    Grass->SetCullDistances(1200, 2200);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> GrassMesh(TEXT("/Game/UOpenGolf/Art/SM_GrassClump.SM_GrassClump"));
    Grass->SetStaticMesh(GrassMesh.Object);

    const float GreenStart = GreenDistanceM - GreenHalfSizeM;
    const float GreenEnd = GreenDistanceM + GreenHalfSizeM;
    const EOpenFairwaySurface Fairway = EOpenFairwaySurface::Fairway;

    // Ground pieces tile without overlapping, with their tops at the tee's height.
    // Assign fairway to the scene ground for calibration shots, including the target
    // and side strips. The ball still resolves each contacted mesh's surface tag.
    AddBox(TEXT("FairwayFront"), FVector(RangeStartM, -FairwayHalfWidthM, 0), FVector(GreenStart - FringeWidthM, FairwayHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("FairwayBack"), FVector(GreenEnd + FringeWidthM, -FairwayHalfWidthM, 0), FVector(RangeEndM, FairwayHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("RoughLeft"), FVector(RangeStartM, -RoughHalfWidthM, 0), FVector(RangeEndM, -FairwayHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("RoughRight"), FVector(RangeStartM, FairwayHalfWidthM, 0), FVector(RangeEndM, RoughHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("Green"), FVector(GreenStart, -GreenHalfSizeM, 0), FVector(GreenEnd, GreenHalfSizeM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("FringeFront"), FVector(GreenStart - FringeWidthM, -FairwayHalfWidthM, 0), FVector(GreenStart, FairwayHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("FringeBack"), FVector(GreenEnd, -FairwayHalfWidthM, 0), FVector(GreenEnd + FringeWidthM, FairwayHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("FringeLeft"), FVector(GreenStart, -FairwayHalfWidthM, 0), FVector(GreenEnd, -GreenHalfSizeM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("FringeRight"), FVector(GreenStart, GreenHalfSizeM, 0), FVector(GreenEnd, FairwayHalfWidthM, 0), FairwayColor, true, Fairway);

    // Presentation only (no collision): tee box paint, flag and yardage markers.
    AddBox(TEXT("TeeBox"), FVector(-2.5f, -2.5f, 0.0f), FVector(2.5f, 2.5f, 0.005f), TeeColor, false);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> FlagMesh(TEXT("/Game/UOpenGolf/Art/SM_Flagstick.SM_Flagstick"));
    UStaticMeshComponent* Flag = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Flagstick"));
    Flag->SetupAttachment(RootComponent);
    Flag->SetStaticMesh(FlagMesh.Object);
    Flag->SetRelativeLocation(FVector(GreenDistanceM * CmPerM, 0, 0));
    Flag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Flag->SetCanEverAffectNavigation(false);
    for (int32 Yards = 50; Yards <= 300; Yards += 50)
    {
        AddYardageMarker(Yards);
    }

    UDirectionalLightComponent* Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
    Sun->SetupAttachment(RootComponent);
    Sun->SetMobility(EComponentMobility::Movable);
    Sun->SetRelativeRotation(FRotator(-40.0f, 35.0f, 0.0f));
    Sun->SetAtmosphereSunLight(true);

    USkyAtmosphereComponent* Sky = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Sky"));
    Sky->SetupAttachment(RootComponent);

    USkyLightComponent* SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
    SkyLight->SetupAttachment(RootComponent);
    SkyLight->SetMobility(EComponentMobility::Movable);
    SkyLight->bRealTimeCapture = true;
}

UStaticMeshComponent* AUOpenGolfRange::AddBox(FName Name, const FVector& MinM, const FVector& MaxM, const FLinearColor& Color, bool bCollides,
    TOptional<EOpenFairwaySurface> Surface)
{
    // Collision boxes are a meter thick below the ground plane; decorations use their own bounds.
    const FVector Min = bCollides ? FVector(MinM.X, MinM.Y, -GroundThicknessM) : MinM;
    const FVector Max = bCollides ? FVector(MaxM.X, MaxM.Y, 0.0f) : MaxM;

    UStaticMeshComponent* Box = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Box->SetupAttachment(RootComponent);
    Box->SetStaticMesh(CubeMesh);
    // The engine cube is 100 cm (1 m) on a side, centered on its origin.
    Box->SetRelativeLocation((Min + Max) * 0.5f * CmPerM);
    Box->SetRelativeScale3D(Max - Min);
    Box->SetMobility(EComponentMobility::Movable);
    if (bCollides)
    {
        Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    }
    else
    {
        Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Box->CastShadow = Name != TEXT("TeeBox");
    }
    if (Surface.IsSet())
    {
        Box->ComponentTags.Add(AOpenFairwayBall::GetSurfaceTag(Surface.GetValue()));
        static ConstructorHelpers::FObjectFinder<UMaterialInterface> FairwayMaterial(TEXT("/Game/UOpenGolf/Art/M_Fairway.M_Fairway"));
        Box->SetMaterial(0, FairwayMaterial.Object);
    }
    else
    {
        ColoredPieces.Emplace(Box, Color);
    }
    return Box;
}

void AUOpenGolfRange::AddYardageMarker(int32 Yards)
{
    const float X = Yards * MetersPerYard;
    for (const float Side : {-1.0f, 1.0f})
    {
        const float Y = Side * (FairwayHalfWidthM + 1.0f);
        AddBox(*FString::Printf(TEXT("Marker%d_%s"), Yards, Side < 0 ? TEXT("L") : TEXT("R")), FVector(X - 0.15f, Y - 0.15f, 0.0f),
            FVector(X + 0.15f, Y + 0.15f, 1.2f), MarkerColor, false);

        UTextRenderComponent* Label = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("Label%d_%s"), Yards, Side < 0 ? TEXT("L") : TEXT("R")));
        Label->SetupAttachment(RootComponent);
        Label->SetMobility(EComponentMobility::Movable);
        // Text faces back toward the tee.
        Label->SetRelativeLocation(FVector(X, Y, 1.4f) * CmPerM);
        Label->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
        Label->SetHorizontalAlignment(EHTA_Center);
        Label->SetWorldSize(90.0f);
        Label->SetTextRenderColor(FColor::White);
        Label->SetText(FText::AsNumber(Yards));
    }
}

void AUOpenGolfRange::BeginPlay()
{
    Super::BeginPlay();
    UpdateGrass();
    for (const TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>& Piece : ColoredPieces)
    {
        // The engine's basic shape material exposes a "Color" parameter.
        if (UMaterialInstanceDynamic* Material = Piece.Key->CreateDynamicMaterialInstance(0))
        {
            Material->SetVectorParameterValue(TEXT("Color"), Piece.Value);
        }
    }
}

void AUOpenGolfRange::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    UpdateGrass();
}

int32 AUOpenGolfRange::GetGrassInstanceCount() const
{
    return Grass->GetInstanceCount();
}

void AUOpenGolfRange::UpdateGrass()
{
    const bool bWanted = CVarGrass3D.GetValueOnGameThread() != 0;
    if (bWanted == bGrassEnabled)
        return;
    bGrassEnabled = bWanted;
    if (!bWanted)
    {
        Grass->ClearInstances();
        return;
    }

    FRandomStream Random(20261015);
    TArray<FTransform> Transforms;
    // 25 clumps/m2, in a 20 m radius clipped to the range. Leave tee paint clear.
    for (float X = -990; X < 2000; X += 20)
    {
        for (float Y = -1990; Y < 2000; Y += 20)
        {
            const float PX = X + Random.FRandRange(-8, 8);
            const float PY = Y + Random.FRandRange(-8, 8);
            if (PX * PX + PY * PY > 2000 * 2000 || (FMath::Abs(PX) < 255 && FMath::Abs(PY) < 255))
                continue;
            const float Scale = Random.FRandRange(0.75f, 1.15f);
            Transforms.Emplace(FRotator(0, Random.FRandRange(0, 360), 0), FVector(PX, PY, 0.0f), FVector(Scale));
        }
    }
    Grass->AddInstances(Transforms, false, false, false);
}
