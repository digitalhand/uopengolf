// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "OpenFairwayTypes.h"
#include "UOpenGolfRange.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UPostProcessComponent;

/**
 * Driving range with distributable grass and golf assets: a tee,
 * fairway ground, a target flag, yardage markers every 50 yards and simple daylight.
 * All ground pieces have the Fairway surface tag for consistent test shots; optional turf and
 * grass are limited to the fairway strip.
 * Component names preserve the original green/fringe/rough layout. The tee is on the
 * ground at the actor's origin and the target line is world +X.
 */
UCLASS()
class UOPENGOLF_API AUOpenGolfRange : public AActor
{
    GENERATED_BODY()

public:
    AUOpenGolfRange();
    virtual void Tick(float DeltaSeconds) override;

    /** Number of decorative grass clumps, zero when disabled. */
    int32 GetGrassInstanceCount() const;

    /** Where the ball is teed up. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Range")
    FVector GetTeeLocation() const { return GetActorLocation(); }

    /** Distance from the tee to the green's center (150 yards). */
    static constexpr float GreenDistanceM = 137.16f;

    /** Half width of the fairway strip; turf and grass stay inside it. */
    static constexpr float FairwayHalfWidthM = 20.0f;

    /** Tag on the fairway, fringe and green pieces that receive GroundMaterialOverride. */
    static const FName TurfTag;

    /** Tag on the rough strips and the backdrop that receive RoughMaterialOverride. */
    static const FName RoughTag;

protected:
    virtual void BeginPlay() override;

    /** Empty uses the public clump. Grass is clipped to the fairway strip. Private subclasses own optional content references. */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    TArray<TObjectPtr<UStaticMesh>> GrassMeshes;

    /** Applied only to TurfTag pieces (fairway, fringe and green). */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    TObjectPtr<UMaterialInterface> GroundMaterialOverride;

    /** Applied only to RoughTag pieces (rough strips and backdrop). Null keeps the public material. */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    TObjectPtr<UMaterialInterface> RoughMaterialOverride;

    /** Fixed exposure compensation in EV for the whole range (auto exposure is off project-wide). */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    float ExposureBiasEV = -0.5f;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation", meta = (ClampMin = "5", ClampMax = "200"))
    float GrassSpacingCm = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    FVector GrassScale = FVector::OneVector;

    /** Grass covers this radius around the tee, intersected with the fairway strip. */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation", meta = (ClampMin = "100", ClampMax = "5000"))
    float GrassRadiusCm = 2000.0f;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation", meta = (ClampMin = "0"))
    int32 GrassCullStartCm = 1200;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation", meta = (ClampMin = "100"))
    int32 GrassCullEndCm = 2200;

private:
    void UpdateGrass();

    UPROPERTY()
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Grass;

    /** Unbound range-wide view settings: fixed exposure and no motion blur. */
    UPROPERTY()
    TObjectPtr<UPostProcessComponent> PostProcess;

    UPROPERTY()
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassComponents;

    bool bGrassEnabled = false;

    UStaticMeshComponent* AddBox(FName Name, const FVector& MinM, const FVector& MaxM, const FLinearColor& Color, bool bCollides,
        TOptional<EOpenFairwaySurface> Surface = TOptional<EOpenFairwaySurface>());

    void AddYardageMarker(int32 Yards);

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> FairwayMaterial;

    /** Each colored piece with its color, applied as a dynamic material at BeginPlay. */
    TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>> ColoredPieces;
};
