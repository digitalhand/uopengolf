// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "OpenFairwayTypes.generated.h"

/** Ground surfaces with their own contact tuning. Tag a component or actor "OpenFairway.Surface.<Name>" to assign one. */
UENUM(BlueprintType)
enum class EOpenFairwaySurface : uint8
{
    Fairway,
    FairwaySoft UMETA(DisplayName = "Soft Fairway"),
    Rough,
    Firm,
    Green,
    Sand,
    Fringe
};

UENUM(BlueprintType)
enum class EOpenFairwayBallState : uint8
{
    Rest,
    Flight,
    /** On or bouncing along the ground after the first landing. */
    Rollout
};

/**
 * Ball launch from a launch monitor: mph, degrees and rpm with GSPro signs (positive HLA,
 * spin axis and side spin are to the right). Leave a spin flag false when it was not
 * measured; it is derived from the others.
 */
USTRUCT(BlueprintType)
struct OPENFAIRWAYPHYSICS_API FOpenFairwayLaunch
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    float BallSpeedMph = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    float VerticalLaunchDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    float HorizontalLaunchDeg = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    bool bHasBackSpin = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway", meta = (EditCondition = "bHasBackSpin"))
    float BackSpinRpm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    bool bHasSideSpin = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway", meta = (EditCondition = "bHasSideSpin"))
    float SideSpinRpm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    bool bHasTotalSpin = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway", meta = (EditCondition = "bHasTotalSpin"))
    float TotalSpinRpm = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    bool bHasSpinAxis = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway", meta = (EditCondition = "bHasSpinAxis"))
    float SpinAxisDeg = 0.0f;
};

/**
 * Live and final shot numbers. Distances are meters from the launch point: downrange along
 * the shot's horizontal direction, lateral from the target line (positive right).
 */
USTRUCT(BlueprintType)
struct OPENFAIRWAYPHYSICS_API FOpenFairwayShotResult
{
    GENERATED_BODY()

    /** Increases for every launch of this ball. */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    int32 ShotNumber = 0;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    FOpenFairwayLaunch Launch;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    bool bLanded = false;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    bool bAtRest = false;

    /** Downrange distance at the first landing. */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float CarryM = 0.0f;

    /** Current downrange distance; the total once the ball is at rest. */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float TotalM = 0.0f;

    /** Total minus carry (negative when the ball spins back). */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float RollM = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float LateralM = 0.0f;

    /** Highest point above the launch point. */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float ApexM = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float HangTimeS = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float LandingAngleDeg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float LandingSpeedMps = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    EOpenFairwaySurface LandingSurface = EOpenFairwaySurface::Fairway;

    /** Surface under the ball now, or where it came to rest. */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    EOpenFairwaySurface Surface = EOpenFairwaySurface::Fairway;

    /** Seconds since launch. */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    float ElapsedS = 0.0f;

    /** "Run", "Release", "Check" or "Spinback": the landing response chosen at launch. */
    UPROPERTY(BlueprintReadOnly, Category = "OpenFairway")
    FString LandingBucket;
};
