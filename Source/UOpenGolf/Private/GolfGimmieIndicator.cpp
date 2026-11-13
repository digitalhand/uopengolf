// SPDX-License-Identifier: MIT
#include "GolfGimmieIndicator.h"
#include "OpenGolfTerrain.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/Common/HudStyle.h"

UGolfGimmieIndicator::UGolfGimmieIndicator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    CastShadow = false;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/UOpenGolf/Art/M_ShotTrail.M_ShotTrail"));
    RingMaterial = Material.Object;
}

void UGolfGimmieIndicator::UpdateIndicator(AOpenGolfTerrainArea* Terrain, FVector Cup, float RadiusFeet, bool bEnabled)
{
    if (!bEnabled || !Terrain || RadiusFeet <= 0 || !RingMaterial) { SetVisibility(false); return; }
    FVector Center, Normal;
    if (!Terrain->GroundAt(Cup, Center, Normal)) { SetVisibility(false); return; }
    if (LastTerrain == Terrain && LastCup.Equals(Center, .1) && LastRadius == RadiusFeet && LastTerrainTransform.Equals(Terrain->GetActorTransform()))
    { SetVisibility(true); return; }
    constexpr int32 Segments = 128;
    constexpr int32 RadialSteps = 8;
    // The outline grows outward, so its inner edge stays at the exact scoring radius.
    constexpr float EdgeWidthCm = 6.f;
    const float RadiusCm = RadiusFeet * 30.48f;
    TArray<FVector> Vertices;
    TArray<int32> Fill, Edge;
    TArray<FVector> Normals;
    TArray<FVector2D> UV;
    // Concentric rings ensure the fill follows slopes as well as the outline.
    for (int32 R = 0; R <= RadialSteps + 1; ++R)
    {
        const float Radius = R <= RadialSteps ? RadiusCm * R / RadialSteps : RadiusCm + EdgeWidthCm;
        for (int32 I = 0; I < Segments; ++I)
        {
            const float Angle = 2 * PI * I / Segments;
            FVector Ground, GroundNormal;
            if (!Terrain->GroundAt(Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0) * Radius, Ground, GroundNormal))
            { ClearAllMeshSections(); SetVisibility(false); LastRadius = -1; return; }
            Vertices.Add(Ground + GroundNormal * .6f); Normals.Add(GroundNormal); UV.Add(FVector2D(.5f, .5f));
        }
    }
    for (int32 R = 0; R <= RadialSteps; ++R)
        for (int32 I = 0; I < Segments; ++I)
        {
            const int32 A = R * Segments + I, B = R * Segments + (I+1)%Segments;
            TArray<int32>& Triangles = R == RadialSteps ? Edge : Fill;
            Triangles.Append({A, A+Segments, B, B, A+Segments, B+Segments});
        }
    SetWorldTransform(FTransform::Identity);
    for (int32 Section = 0; Section < 2; ++Section)
    {
        CreateMeshSection(Section, Vertices, Section == 0 ? Fill : Edge, Normals, UV, TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        auto* Material = CreateDynamicMaterialInstance(Section, RingMaterial);
        Material->SetVectorParameterValue(TEXT("Color"), HudStyle::AccentColor());
        Material->SetScalarParameterValue(TEXT("Opacity"), Section == 0 ? .08f : .9f);
    }
    LastTerrain = Terrain; LastCup = Center; LastRadius = RadiusFeet; LastTerrainTransform = Terrain->GetActorTransform();
    SetVisibility(true);
}
