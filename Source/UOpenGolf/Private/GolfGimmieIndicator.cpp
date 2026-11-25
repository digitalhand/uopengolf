// SPDX-License-Identifier: MIT
#include "GolfGimmieIndicator.h"
#include "UOpenGolfAssetPaths.h"
#include "OpenGolfTerrain.h"
#include "Training/GroundBands.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"
#include "UI/Common/HudStyle.h"

UGolfGimmieIndicator::UGolfGimmieIndicator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    CastShadow = false;
    SetHiddenInSceneCapture(true);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(UOpenGolfAssetPaths::ShotTrailMaterial);
    RingMaterial = Material.Object;
}

void UGolfGimmieIndicator::UpdateIndicator(AOpenGolfTerrainArea* Terrain, FVector Cup, float RadiusFeet, bool bEnabled)
{
    if (!bEnabled || !Terrain || RadiusFeet <= 0 || !RingMaterial) { SetVisibility(false); return; }
    FVector Center, Normal;
    if (!Terrain->GroundAt(Cup, Center, Normal)) { SetVisibility(false); return; }
    if (LastTerrain == Terrain && LastCup.Equals(Center, .1) && LastRadius == RadiusFeet && LastTerrainTransform.Equals(Terrain->GetActorTransform()))
    { SetVisibility(true); return; }
    const float RadiusCm = RadiusFeet * 30.48f;
    const GroundBands::FBand Bands[] = {
        {RadiusCm, 8, HudStyle::AccentColor(), .08f}, {RadiusCm + 6.f, 1, HudStyle::AccentColor(), .9f}
    };
    if (!GroundBands::Draw(*this, *Terrain, Center, *RingMaterial, Bands, 128))
    {
        SetVisibility(false);
        LastRadius = -1;
        return;
    }
    LastTerrain = Terrain; LastCup = Center; LastRadius = RadiusFeet; LastTerrainTransform = Terrain->GetActorTransform();
    SetVisibility(true);
}
