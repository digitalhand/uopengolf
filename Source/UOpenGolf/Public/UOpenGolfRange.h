// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "OpenFairwayTypes.h"
#include "UOpenGolfRange.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;

/**
 * Driving range with distributable grass and golf assets: a tee,
 * fairway ground, a target flag, yardage markers every 50 yards and simple daylight.
 * All ground pieces have the Fairway surface tag and appearance for consistent test shots.
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

protected:
    virtual void BeginPlay() override;

    /** Empty uses the public clump. Private subclasses own optional content references. */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    TArray<TObjectPtr<UStaticMesh>> GrassMeshes;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    TObjectPtr<UMaterialInterface> GroundMaterialOverride;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation", meta = (ClampMin = "5", ClampMax = "200"))
    float GrassSpacingCm = 20.0f;

    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Presentation")
    FVector GrassScale = FVector::OneVector;

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

    UPROPERTY()
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassComponents;

    bool bGrassEnabled = false;

    UStaticMeshComponent* AddBox(FName Name, const FVector& MinM, const FVector& MaxM, const FLinearColor& Color, bool bCollides,
        TOptional<EOpenFairwaySurface> Surface = TOptional<EOpenFairwaySurface>());

    void AddYardageMarker(int32 Yards);

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    /** Each colored piece with its color, applied as a dynamic material at BeginPlay. */
    TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>> ColoredPieces;
};
