// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/GameModeBase.h"
#include "Play/OpenGolfHoleRound.h"
#include "Play/OpenGolfPlayRules.h"
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
 * Uses the map's range controller (or spawns one with daylight and a flag), tees up an OpenFairway ball on its terrain
 * and hits every launch-monitor shot (device or replay). In Range mode every shot is hit from the tee and a stopped ball
 * goes back to it after ResetDelayS. In Hole mode the ball is played as it lies, strokes and putts are counted, and the
 * ball can be placed anywhere for practice; after it drops into the cup it goes back to the tee for a new hole. A shot
 * result stays available until the next launch. While shot recording is on, device shots are saved as JSON under
 * Documents/UOpenGolf/Saved/ShotRecordings.
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

    /** Puts the ball back on the tee and cancels a pending reset. The last shot result is kept. In Hole mode a new hole starts. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf")
    void ResetBallToTee();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf")
    void ResetBall();

    bool IsPractice() const { return Round.bPractice; }
    FVector GetPracticeOrigin() const { return PracticeOrigin; }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    EOpenGolfPlayMode GetPlayMode() const { return PlayMode; }

    /** Switches between the range and playing the hole; either way the ball goes back to the tee. Ignored mid-shot. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    bool SetPlayMode(EOpenGolfPlayMode Mode);

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    bool TogglePlayMode();

    /** The hole being played; strokes are only counted in Hole mode. */
    const FOpenGolfHoleRound& GetHoleRound() const { return Round; }

    /**
     * Places the ball at rest on the ground at Location to practise from there (Hole mode only), aimed at the cup. It
     * adds no stroke, but the hole then counts as practice. False while the ball is moving or too close to the cup.
     */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    bool PlaceBallAt(const FVector& Location);

    /** The cup at the flag, on the green's surface; the range origin when there is no flag. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    FVector GetCupLocation() const;

    /** Horizontal distance from the ball to the cup in metres. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    float GetDistanceToCupM() const;

    /** At rest and waiting for the next shot: on the tee in Range mode, at its lie (not in the cup) in Hole mode. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    bool IsBallAtAddress() const;

    /** Seconds since the ball dropped into the cup (Hole mode), or a negative value. */
    float GetHoledSeconds() const;

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

    /** Hole mode: seconds a stopped ball's result shows before the view turns to the next shot. */
    static constexpr float AddressDelayS = 2.0f;

    /** Hole mode: seconds after a hole-out before the ball goes back to the tee for a new hole. */
    static constexpr float HoleOutDelayS = 5.0f;



    /** Converts a launch-monitor shot; false when speed or launch angles were not measured. */
    static bool ToLaunch(const FLaunchMonitorShot& Shot, FOpenFairwayLaunch& OutLaunch);

protected:
    /** Optional map-owned presentation variant; public maps use the native range. */
    UPROPERTY(EditDefaultsOnly, Category = "UOpenGolf|Range")
    TSubclassOf<AUOpenGolfRange> RangeClass;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void HandleShot(const FLaunchMonitorShot& Shot);
    void HandleClubData(const FLaunchMonitorClubData& ClubData);

    UFUNCTION()
    void HandleBallAtRest(const FOpenFairwayShotResult& Result);

    void ResetAfterShot();
    void ReadyForNextShot();
    /** Turns the ball (and its aim line) toward the cup. */
    void AimAtCup();
    void PlaceOnTee();

    UPROPERTY()
    TObjectPtr<AUOpenGolfRange> Range;

    UPROPERTY()
    TObjectPtr<AOpenFairwayBall> Ball;

    // Shared, not unique: generated code destroys the mode where the recorder type is incomplete.
    TSharedPtr<ShotRecording::FShotRecorder> Recorder;
    FDelegateHandle ShotHandle;
    FDelegateHandle ClubDataHandle;
    FTimerHandle ResetTimer;
    FTimerHandle AddressTimer;
    EOpenGolfPlayMode PlayMode = EOpenGolfPlayMode::Range;
    FOpenGolfHoleRound Round;
    FVector PracticeOrigin = FVector::ZeroVector;
    bool bAddressReady = true;
    bool bShotFromGreen = false;
    double HoledTime = -1.0;
    FTimerHandle RecordingTimer;
};
