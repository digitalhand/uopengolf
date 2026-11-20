// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/GameModeBase.h"
#include "Play/OpenGolfHoleRound.h"
#include "Play/OpenGolfPlayRules.h"
#include "OpenFairwayTypes.h"
#include "UOpenGolfGameMode.generated.h"

class AOpenFairwayBall;
class UGolfGimmieIndicator;
class AUOpenGolfRange;
class UGolfCourseDefinition;
struct FLaunchMonitorClubData;
struct FLaunchMonitorShot;

namespace OpenGolfOnline { class FCloudShotRecorder; }

namespace ShotRecording
{
    class FShotRecorder;
}

/**
 * Runs Training (Range mode) and course rounds (Hole mode), receives launch-monitor shots and records device data.
 * Range mode tees up each shot. A course round, entered by level travel with ?Course=<definition>, plays from the
 * current lie, counts strokes and putts, and announces OnRoundComplete once the hole-out banner has been shown.
 * Shot results remain available until the next launch.
 */
UCLASS(Config = Game)
class UOPENGOLF_API AUOpenGolfGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    AUOpenGolfGameMode();

    /** Reads the URL options: ?Course=<definition> starts a course round, anything else is Training. */
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void StartPlay() override;
    float GetGimmieRadiusFeet() const { return SessionGimmieFeet; }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf")
    AOpenFairwayBall* GetBall() const { return Ball; }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf")
    AUOpenGolfRange* GetRange() const { return Range; }

    /** Puts the ball back on the tee and cancels a pending reset. The last shot result is kept. In Hole mode a new hole starts. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf")
    void ResetBallToTee();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf")
    void ResetBall();

    /** The course being played; null in Training. */
    const UGolfCourseDefinition* GetCourse() const { return Course; }
    /** A scored course round (Hole mode), as opposed to Training. */
    bool IsCourseRound() const { return PlayMode == EOpenGolfPlayMode::Hole; }
    /** A course round whose ball has dropped: no more shots are taken and the scorecard follows. */
    bool IsRoundComplete() const { return IsCourseRound() && Round.bHoled; }
    /** Par for the round: the course's in a course round, the range's in Training. */
    int32 GetPar() const;
    /** The hole being played, from 1. Every course is a single hole for now. */
    int32 GetHoleNumber() const { return 1; }

    DECLARE_MULTICAST_DELEGATE(FOnRoundComplete);
    /** A course round has ended and its hole-out banner has been shown for HoleOutDelayS. */
    FOnRoundComplete OnRoundComplete;

    bool IsPractice() const { return Round.bPractice; }
    FVector GetPracticeOrigin() const { return PracticeOrigin; }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    EOpenGolfPlayMode GetPlayMode() const { return PlayMode; }

#if WITH_DEV_AUTOMATION_TESTS
    /**
     * Tests only: switches the running world between Training and a round of InCourse without level travel, as a fresh
     * session; players enter courses through travel (?Course=). Hole mode without a course counts strokes but keeps
     * Training's wind. Ignored mid-shot.
     */
    bool SetPlayMode(EOpenGolfPlayMode Mode, const UGolfCourseDefinition* InCourse = nullptr);
#endif

    /** The hole being played; strokes are only counted in Hole mode. */
    const FOpenGolfHoleRound& GetHoleRound() const { return Round; }

    /**
     * Places the ball at rest on the ground at Location to practise from there (Training only), aimed at the cup. It
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

    /** Records real launch-monitor shots on the range (never replays or hole shots) while connected; off at the start of play. */
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

    /** Hole mode: seconds to show the hole-out banner before OnRoundComplete opens the final scorecard. */
    static constexpr float HoleOutDelayS = 5.0f;

    /** Practice lie: how far (cm) the ball may sit from where it was placed and still be at address. */
    static constexpr float PracticeAddressToleranceCm = 2.0f;

    /** Range mode: how far (cm) the ball may sit from the tee and still be at address. */
    static constexpr float TeeAddressToleranceCm = 200.0f;

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
    /** After the hole-out banner: tells listeners (the player's menu) that the round is over. */
    void FinishRound();
    /** Turns the ball (and its aim line) toward the cup. */
    void AimAtCup();
    void PlaceOnTee();
    void StartSessionDefaults();

    UPROPERTY()
    TObjectPtr<AUOpenGolfRange> Range;

    UPROPERTY()
    TObjectPtr<AOpenFairwayBall> Ball;

    /** Set by InitGame for a course round; kept loaded for the round. */
    UPROPERTY(Transient)
    TObjectPtr<const UGolfCourseDefinition> Course;

    // Shared, not unique: generated code destroys the mode where the recorder type is incomplete.
    TSharedPtr<ShotRecording::FShotRecorder> Recorder;
    TSharedPtr<OpenGolfOnline::FCloudShotRecorder> CloudRecorder;
    FDelegateHandle CloudReadHandle, CloudAccountHandle;
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
    float SessionGimmieFeet = 0;
    /** The wind saved with the map, which Training starts from when the starting-wind preference is "Course default". */
    float MapWindMph = 0, MapWindFromDeg = 0;
    int32 LastScoredShot = INDEX_NONE;
    UPROPERTY()
    TObjectPtr<UGolfGimmieIndicator> GimmieIndicator;
};
