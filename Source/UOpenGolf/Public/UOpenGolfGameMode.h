// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/GameModeBase.h"
#include "UOpenGolfGameMode.generated.h"

class AOpenFairwayBall;
class AUOpenGolfRange;
struct FLaunchMonitorShot;
struct FOpenFairwayLaunch;

/**
 * Range mode: builds the whitebox range, tees up an OpenFairway ball and hits every
 * launch-monitor shot (device or replay) from the tee.
 */
UCLASS()
class UOPENGOLF_API AUOpenGolfGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AUOpenGolfGameMode();

    virtual void StartPlay() override;

    UFUNCTION(BlueprintPure, Category = "UOpenGolf")
    AOpenFairwayBall* GetBall() const { return Ball; }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf")
    AUOpenGolfRange* GetRange() const { return Range; }

    /** Puts the ball back on the tee. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf")
    void ResetBallToTee();

    /** Converts a launch-monitor shot; false when speed or launch angles were not measured. */
    static bool ToLaunch(const FLaunchMonitorShot& Shot, FOpenFairwayLaunch& OutLaunch);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleShot(const FLaunchMonitorShot& Shot);

    UPROPERTY()
    TObjectPtr<AUOpenGolfRange> Range;

    UPROPERTY()
    TObjectPtr<AOpenFairwayBall> Ball;

    FDelegateHandle ShotHandle;
};
