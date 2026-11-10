// SPDX-License-Identifier: MIT
#pragma once
#include "ProceduralMeshComponent.h"
#include "GolfGimmieIndicator.generated.h"
class AOpenGolfTerrainArea;
class UMaterialInstanceDynamic;

/** Terrain-conforming, collision-free visualization of the session scoring radius. */
UCLASS()
class UOPENGOLF_API UGolfGimmieIndicator : public UProceduralMeshComponent
{
    GENERATED_BODY()
public:
    UGolfGimmieIndicator(const FObjectInitializer& ObjectInitializer);
    void UpdateIndicator(AOpenGolfTerrainArea* Terrain, FVector Cup, float RadiusFeet, bool bEnabled);
private:
    FVector LastCup = FVector::ZeroVector;
    FTransform LastTerrainTransform;
    float LastRadius = -1;
    TWeakObjectPtr<AOpenGolfTerrainArea> LastTerrain;
    UPROPERTY() TObjectPtr<UMaterialInterface> RingMaterial;
};
