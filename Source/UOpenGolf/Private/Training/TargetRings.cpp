// SPDX-License-Identifier: MIT
#include "Training/TargetRings.h"
#include "Training/GroundBands.h"
#include "Training/TrainingScoring.h"
#include "Components/TextRenderComponent.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "OpenGolfTerrain.h"
#include "ProceduralMeshComponent.h"

namespace
{
    constexpr float FillOpacity = .1f;
    constexpr float EdgeOpacity = .9f;

    template <typename T> T* MakeComponent(AActor& Owner)
    {
        T* Component = NewObject<T>(&Owner);
        Component->SetupAttachment(Owner.GetRootComponent());
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(false);
        Component->SetCanEverAffectNavigation(false);
        // The hole map is a scene capture of the course; the drill target is not part of it.
        Component->SetHiddenInSceneCapture(true);
        Component->SetCastShadow(false);
        return Component;
    }
} // namespace

UTargetRings* UTargetRings::Create(AActor& Owner)
{
    auto* Rings = NewObject<UTargetRings>(&Owner);
    Rings->Material = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UOpenGolf/Art/M_ShotTrail.M_ShotTrail"));
    Rings->LineMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/UOpenGolf/Art/M_TargetRingLine.M_TargetRingLine"));
    if (!Rings->Material || !Rings->LineMaterial)
        return nullptr;
    Rings->Mesh = MakeComponent<UTargetRingMesh>(Owner);
    Rings->Mesh->RegisterComponent();
    Rings->Labels = {Rings->MakeLabel(Owner, NSLOCTEXT("Training", "ThreeFeet", "3 ft")),
        Rings->MakeLabel(Owner, NSLOCTEXT("Training", "SixFeet", "6 ft"))};
    return Rings;
}

UTextRenderComponent* UTargetRings::MakeLabel(AActor& Owner, const FText& Text)
{
    auto* Label = MakeComponent<UTextRenderComponent>(Owner);
    Label->SetText(Text);
    Label->SetTextRenderColor(FColor::White);
    Label->SetWorldSize(LabelSizeCm);
    Label->SetHorizontalAlignment(EHTA_Center);
    Label->SetVerticalAlignment(EVRTA_TextCenter);
    // Shown once the rings are placed, so a failed first trace never leaves labels at the origin.
    Label->SetVisibility(false);
    Label->RegisterComponent();
    return Label;
}

float UTargetRings::EdgeWidthCm(double TeeToPinCm)
{
    return FMath::Max(MinEdgeWidthCm, float(TeeToPinCm * EdgeWidthPerDistance));
}

bool UTargetRings::Place(AOpenGolfTerrainArea& Terrain, FVector Center, FVector LineDir, double TeeToPinCm, ECollisionChannel Channel)
{
    if (!IsValid(Mesh) || !Material || !LineMaterial || Labels.Num() != 2)
        return false;
    const float Inner = float(TrainingScoring::InnerRadiusCm), Outer = float(TrainingScoring::OuterRadiusCm);
    const float Edge = EdgeWidthCm(TeeToPinCm);
    // Each white edge grows outward, so its inner side is the exact scoring radius. Between the rings is a gap.
    const GroundBands::FBand Bands[] = {
        {Inner, 6, FLinearColor::White, FillOpacity},
        {Inner + Edge, 1, FLinearColor::White, EdgeOpacity, LineMaterial},
        {Outer, 1, FLinearColor::White, 0.f},
        {Outer + Edge, 1, FLinearColor::White, EdgeOpacity, LineMaterial}};
    if (!GroundBands::Draw(*Mesh, Terrain, Center, *Material, Bands, Segments, Channel, TurfClearanceCm))
        return false;
    // Each label sits halfway across its own ring, on the side the player looks from.
    PlaceLabel(*Labels[0], Terrain, Center, LineDir, Inner * .5, Channel);
    PlaceLabel(*Labels[1], Terrain, Center, LineDir, (Inner + Edge + Outer) * .5, Channel);
    return true;
}

bool UTargetRings::PlaceLabel(UTextRenderComponent& Label, AOpenGolfTerrainArea& Terrain, FVector Center, FVector LineDir, double OffsetCm,
    ECollisionChannel Channel) const
{
    FVector Ground, Normal;
    if (!Terrain.GroundAt(Center - LineDir * OffsetCm, Ground, Normal, Channel))
    {
        Label.SetVisibility(false);
        return false;
    }
    // Text faces its +X and stands along +Z: lay it on the ground with its top away from the tee.
    Label.SetWorldLocationAndRotation(Ground + Normal * TurfClearanceCm, FRotationMatrix::MakeFromXZ(Normal, LineDir).Rotator());
    Label.SetVisibility(true);
    return true;
}

void UTargetRings::Destroy()
{
    if (IsValid(Mesh))
        Mesh->DestroyComponent();
    for (UTextRenderComponent* Label : Labels)
        if (IsValid(Label))
            Label->DestroyComponent();
    Mesh = nullptr;
    Labels.Reset();
}
