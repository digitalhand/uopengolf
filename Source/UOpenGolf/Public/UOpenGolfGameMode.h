// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/GameModeBase.h"
#include "OpenFairwayTypes.h"
#include "UOpenGolfGameMode.generated.h"

class AOpenFairwayBall;
class AUOpenGolfRange;
struct FLaunchMonitorClubData;
struct FLaunchMonitorShot;

namespace ShotRecording
{
    class FShotRecorder;
}

/**
 * Range mode: builds the whitebox range, tees up an OpenFairway ball and hits every
 * launch-monitor shot (device or replay) from the tee. A stopped ball goes back to the
 * tee after ResetDelayS; its shot result stays available until the next launch. While shot
 * recording is on, device shots are saved as JSON under Documents/UOpenGolf/Saved/ShotRecordings.
 */
UCLASS(Config = Game)
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

    /** Puts the ball back on the tee and cancels a pending reset. The last shot result is kept. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf")
    void ResetBallToTee();

    /** Records real launch-monitor shots (never replays) while connected; off at the start of play. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Recording")
    void SetRecordShots(bool bRecord);

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Recording")
    bool IsRecordingShots() const;

    /** Shots saved since recording was last switched on. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Recording")
    int32 GetRecordedShotCount() const;

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Recording")
    int32 GetFailedRecordingCount() const;

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Recording")
    FString GetRecordingError() const;

    /** Folder of the current recording session; empty until its first shot is saved. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Recording")
    FString GetRecordingFolder() const;

    /** Seconds a stopped ball stays put, with its result shown, before it goes back to the tee. */
    static constexpr float ResetDelayS = 3.0f;

    /** Converts a launch-monitor shot; false when speed or launch angles were not measured. */
    static bool ToLaunch(const FLaunchMonitorShot& Shot, FOpenFairwayLaunch& OutLaunch);

protected:
    /** Optional map-owned presentation variant; public maps use the native range. */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Range")
    TSubclassOf<AUOpenGolfRange> RangeClass;

    /**
     * Optional per-machine range subclass, set in the ignored Config/UserGame.ini. Used only
     * when the map keeps the native range and not during automation; if it fails to load,
     * the native range is used.
     */
    UPROPERTY(Config)
    FSoftClassPath LocalRangeClass;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleShot(const FLaunchMonitorShot& Shot);
    void HandleClubData(const FLaunchMonitorClubData& ClubData);

    UFUNCTION()
    void HandleBallAtRest(const FOpenFairwayShotResult& Result);

    void ResetAfterShot();

    UPROPERTY()
    TObjectPtr<AUOpenGolfRange> Range;

    UPROPERTY()
    TObjectPtr<AOpenFairwayBall> Ball;

    // Shared, not unique: generated code destroys the mode where the recorder type is incomplete.
    TSharedPtr<ShotRecording::FShotRecorder> Recorder;
    FDelegateHandle ShotHandle;
    FDelegateHandle ClubDataHandle;
    FTimerHandle ResetTimer;
    FTimerHandle RecordingTimer;
};
