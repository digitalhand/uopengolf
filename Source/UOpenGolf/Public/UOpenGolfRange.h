// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "OpenFairwayTypes.h"
#include "UOpenGolfRange.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * Whitebox driving range built from engine basic shapes, so it needs no binary assets: a tee,
 * a fairway between rough strips, a green ringed by fringe with a flag, yardage markers every
 * 50 yards and simple daylight. Ground pieces block the ball and carry OpenFairway surface
 * tags. The tee is on the ground at the actor's origin and the target line is world +X.
 */
UCLASS()
class UOPENGOLF_API AUOpenGolfRange : public AActor
{
    GENERATED_BODY()

public:
    AUOpenGolfRange();

    /** Where the ball is teed up. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Range")
    FVector GetTeeLocation() const { return GetActorLocation(); }

    /** Distance from the tee to the green's center (150 yards). */
    static constexpr float GreenDistanceM = 137.16f;

protected:
    virtual void BeginPlay() override;

private:
    UStaticMeshComponent* AddBox(FName Name, const FVector& MinM, const FVector& MaxM, const FLinearColor& Color, bool bCollides,
        TOptional<EOpenFairwaySurface> Surface = TOptional<EOpenFairwaySurface>());

    void AddYardageMarker(int32 Yards);

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    /** Each colored piece with its color, applied as a dynamic material at BeginPlay. */
    TArray<TPair<TObjectPtr<UStaticMeshComponent>, FLinearColor>> ColoredPieces;
};
