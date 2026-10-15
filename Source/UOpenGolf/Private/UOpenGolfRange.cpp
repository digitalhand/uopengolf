// SPDX-License-Identifier: MIT
#include "UOpenGolfRange.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OpenFairwayBall.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
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
    const FLinearColor RoughColor(0.07f, 0.25f, 0.06f);
    const FLinearColor GreenColor(0.16f, 0.62f, 0.18f);
    const FLinearColor FringeColor(0.12f, 0.50f, 0.13f);
    const FLinearColor TeeColor(0.13f, 0.48f, 0.13f);
    const FLinearColor MarkerColor(0.95f, 0.95f, 0.95f);
} // namespace

AUOpenGolfRange::AUOpenGolfRange()
{
    PrimaryActorTick.bCanEverTick = false;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Tee"));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    CubeMesh = Cube.Object;

    const float GreenStart = GreenDistanceM - GreenHalfSizeM;
    const float GreenEnd = GreenDistanceM + GreenHalfSizeM;
    const EOpenFairwaySurface Fairway = EOpenFairwaySurface::Fairway;

    // Ground pieces tile without overlapping, with their tops at the tee's height.
    AddBox(TEXT("FairwayFront"), FVector(RangeStartM, -FairwayHalfWidthM, 0), FVector(GreenStart - FringeWidthM, FairwayHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("FairwayBack"), FVector(GreenEnd + FringeWidthM, -FairwayHalfWidthM, 0), FVector(RangeEndM, FairwayHalfWidthM, 0), FairwayColor, true, Fairway);
    AddBox(TEXT("RoughLeft"), FVector(RangeStartM, -RoughHalfWidthM, 0), FVector(RangeEndM, -FairwayHalfWidthM, 0), RoughColor, true, EOpenFairwaySurface::Rough);
    AddBox(TEXT("RoughRight"), FVector(RangeStartM, FairwayHalfWidthM, 0), FVector(RangeEndM, RoughHalfWidthM, 0), RoughColor, true, EOpenFairwaySurface::Rough);
    AddBox(TEXT("Green"), FVector(GreenStart, -GreenHalfSizeM, 0), FVector(GreenEnd, GreenHalfSizeM, 0), GreenColor, true, EOpenFairwaySurface::Green);
    const EOpenFairwaySurface Fringe = EOpenFairwaySurface::Fringe;
    AddBox(TEXT("FringeFront"), FVector(GreenStart - FringeWidthM, -FairwayHalfWidthM, 0), FVector(GreenStart, FairwayHalfWidthM, 0), FringeColor, true, Fringe);
    AddBox(TEXT("FringeBack"), FVector(GreenEnd, -FairwayHalfWidthM, 0), FVector(GreenEnd + FringeWidthM, FairwayHalfWidthM, 0), FringeColor, true, Fringe);
    AddBox(TEXT("FringeLeft"), FVector(GreenStart, -FairwayHalfWidthM, 0), FVector(GreenEnd, -GreenHalfSizeM, 0), FringeColor, true, Fringe);
    AddBox(TEXT("FringeRight"), FVector(GreenStart, GreenHalfSizeM, 0), FVector(GreenEnd, FairwayHalfWidthM, 0), FringeColor, true, Fringe);

    // Presentation only (no collision): tee box paint, flag and yardage markers.
    AddBox(TEXT("TeeBox"), FVector(-2.5f, -2.5f, 0.0f), FVector(2.5f, 2.5f, 0.005f), TeeColor, false);
    AddBox(TEXT("FlagPole"), FVector(GreenDistanceM - 0.015f, -0.015f, 0.0f), FVector(GreenDistanceM + 0.015f, 0.015f, 2.4f), MarkerColor, false);
    AddBox(TEXT("Flag"), FVector(GreenDistanceM - 0.01f, 0.015f, 1.9f), FVector(GreenDistanceM + 0.01f, 0.55f, 2.35f), FLinearColor(0.85f, 0.08f, 0.06f), false);
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
    }
    ColoredPieces.Emplace(Box, Color);
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
    for (const TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>& Piece : ColoredPieces)
    {
        // The engine's basic shape material exposes a "Color" parameter.
        if (UMaterialInstanceDynamic* Material = Piece.Key->CreateDynamicMaterialInstance(0))
        {
            Material->SetVectorParameterValue(TEXT("Color"), Piece.Value);
        }
    }
}
