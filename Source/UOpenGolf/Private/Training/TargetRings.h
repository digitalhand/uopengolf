// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"
#include "UObject/Object.h"
#include "TargetRings.generated.h"

class AActor;
class AOpenGolfTerrainArea;
class UMaterialInterface;
class UTextRenderComponent;

/**
 * The rings' mesh. M_TargetRingLine moves edge vertices off the flat mesh, up to MinPixels of screen space, so its bounds
 * reach past them. Bounds as flat as the ring sit level with the ground and are culled as hidden on some frames, which
 * blinks a far target on and off.
 */
UCLASS()
class UTargetRingMesh : public UProceduralMeshComponent
{
    GENERATED_BODY()
public:
    /** 2 px of growth from the tee camera at 250 yd is about 50 cm. */
    static constexpr float LineGrowthMarginCm = 100.f;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override
    {
        return Super::CalcBounds(LocalToWorld).ExpandBy(LineGrowthMarginCm);
    }
};

/**
 * One white ring around a training pin, labelled flat on the ground and reading from the tee.
 * The edges use M_TargetRingLine (Assets/Scripts/UpdateTrainingRings.py), which keeps them at least two pixels wide on
 * screen: from the tee their far arcs are otherwise thinner than a pixel and flicker into dashes while the camera is still.
 * The components belong to the owning actor; Destroy removes them.
 */
UCLASS(Transient)
class UTargetRings : public UObject
{
    GENERATED_BODY()
public:
    /** Cut-turf blades stand up to 55 mm (the rough); the rings sit above them, or the grass covers them. */
    static constexpr float TurfClearanceCm = 6.f;
    static constexpr float MinEdgeWidthCm = 5.f;
    /** Wider lines further from the tee; M_TargetRingLine keeps them at least two pixels wide on screen. */
    static constexpr float EdgeWidthPerDistance = .0015f;
    static constexpr float LabelSizeCm = 25.f;
    static constexpr int32 Segments = 128;

    static UTargetRings* Create(AActor& Owner, const FText& RadiusText);
    static float EdgeWidthCm(double TeeToPinCm);
    /** Draws around Center, with a label on the tee side of LineDir. Failed ground traces leave the previous target in place. */
    bool Place(AOpenGolfTerrainArea& Terrain, FVector Center, FVector LineDir, double TeeToPinCm, double RadiusCm, ECollisionChannel Channel);
    void Destroy();
    UProceduralMeshComponent* GetMesh() const { return Mesh; }
    UTextRenderComponent* GetLabel() const { return RingLabel; }

private:
    UTextRenderComponent* MakeLabel(AActor& Owner, const FText& Text);
    bool PlaceLabel(UTextRenderComponent& Label, AOpenGolfTerrainArea& Terrain, FVector Center, FVector LineDir, double OffsetCm,
        ECollisionChannel Channel) const;
    UPROPERTY()
    TObjectPtr<UProceduralMeshComponent> Mesh;
    UPROPERTY()
    TObjectPtr<UTextRenderComponent> RingLabel;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> Material;
    UPROPERTY()
    TObjectPtr<UMaterialInterface> LineMaterial;
};
