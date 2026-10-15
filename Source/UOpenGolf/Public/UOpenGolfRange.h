// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "OpenFairwayTypes.h"
#include "UOpenGolfRange.generated.h"

class UStaticMesh;
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

private:
    void UpdateGrass();

    UPROPERTY()
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Grass;

    bool bGrassEnabled = false;

    UStaticMeshComponent* AddBox(FName Name, const FVector& MinM, const FVector& MaxM, const FLinearColor& Color, bool bCollides,
        TOptional<EOpenFairwaySurface> Surface = TOptional<EOpenFairwaySurface>());

    void AddYardageMarker(int32 Yards);

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    /** Each colored piece with its color, applied as a dynamic material at BeginPlay. */
    TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>> ColoredPieces;
};
