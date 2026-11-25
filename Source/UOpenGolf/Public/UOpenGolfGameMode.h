// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/GameModeBase.h"
#include "Play/OpenGolfHoleRound.h"
#include "Play/OpenGolfLieRules.h"
#include "Play/OpenGolfPlayRules.h"
#include "Play/OpenGolfRoundSetup.h"
#include "LaunchMonitorTypes.h"
#include "OpenFairwayTypes.h"
#include "UOpenGolfGameMode.generated.h"

class AOpenFairwayBall;
class ATrainingSession;
class UGolfGimmieIndicator;
class AUOpenGolfRange;
class UGolfCourseDefinition;
enum class EGolfPenalty : uint8;
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
    /**
     * A course round whose last hole is on the scorecard. Shots stop when the last ball drops; the round completes
     * after the hole-out pause, once that hole's score is recorded, so the final card always has every hole.
     */
    bool IsRoundComplete() const;
    int32 GetTeeIndex() const { return TeeIndex; }
    float GetOpeningAimDeg() const { return OpeningAimDeg; }
    float GetHoleLengthM() const;
    const TArray<FOpenGolfHoleRound>& GetCompletedHoles() const { return CompletedHoles; }
    /** The hole number of each completed hole, in the order of GetCompletedHoles. */
    const TArray<int32>& GetCompletedHoleNumbers() const { return CompletedHoleNumbers; }
    /** The setup the course round plays (its conditions and holes); "Play again" starts another round with it. */
    const OpenGolfPlay::FOpenGolfRoundSetup& GetRoundSetup() const { return RoundSetup; }
    /** The number of the round's first hole. */
    int32 GetFirstHoleNumber() const { return HoleOrder.IsEmpty() ? 1 : HoleOrder[0] + 1; }
    /**
     * The wind a course round plays, fixed for the round: its speed and the world direction it comes from (as
     * AOpenFairwayBall::WindFromDeg). False in Training, which keeps the player's own wind.
     */
    bool GetRoundWind(float& OutMph, float& OutWorldFromDeg) const;
    /** The penalty taken in the last PenaltyNoticeS seconds, for the HUD to explain the replay; None otherwise. */
    EGolfPenalty GetRecentPenalty() const;
    static constexpr float PenaltyNoticeS = 4.0f;
    /** Par for the round: the course's in a course round, the range's in Training. */
    int32 GetPar() const;
    /** The hole being played, from 1: its number on the course's scorecard. */
    int32 GetHoleNumber() const { return HoleIndex + 1; }

    DECLARE_MULTICAST_DELEGATE(FOnRoundComplete);
    /** A course round has ended and its hole-out banner has been shown for HoleOutDelayS. */
    FOnRoundComplete OnRoundComplete;

    ATrainingSession* GetTrainingSession() const { return Training; }
    bool IsDrillActive() const;
    bool CanPlaceTrainingBall() const;

    bool IsPractice() const { return Round.bPractice; }
    FVector GetPracticeOrigin() const { return PracticeOrigin; }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    EOpenGolfPlayMode GetPlayMode() const { return PlayMode; }

#if WITH_DEV_AUTOMATION_TESTS
    /**
     * Tests only: switches the running world between Training and a round of InCourse without level travel, as a fresh
     * session; players enter courses through travel (?Course=). The round plays Setup, or the course's defaults with a
     * rolled wind. Hole mode without a course counts strokes but keeps Training's wind. Ignored mid-shot.
     */
    bool SetPlayMode(EOpenGolfPlayMode Mode, const UGolfCourseDefinition* InCourse = nullptr, const OpenGolfPlay::FOpenGolfRoundSetup* Setup = nullptr);
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

    /** The ball is on the tee box, within OpenGolfPlay::TeeBoxRadiusM of the tee. */
    bool IsBallOnTee() const;

    /**
     * What the ball's lie takes off the next shot with the selected club, from the course's lie rules. Nothing on the tee,
     * in the cup or in Training. The hole map shows it, and a launch-monitor shot plays it (the recorders keep the shot as
     * measured).
     */
    FOpenGolfLieCut GetLieCut() const;

    /** Course rounds with Auto club on: selects the club for the ball's lie and distance to the cup (CourseClubs::Recommend). */
    void SelectClubForLie();

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
    friend class ATrainingSession;
    void CreateTrainingSession();
    void ResetToTeeForPlay();
    void ResetForPlay();
    UPROPERTY(Transient) TObjectPtr<ATrainingSession> Training;

    void HandleShot(const FLaunchMonitorShot& Shot);
    void HandleClubData(const FLaunchMonitorClubData& ClubData);

    UFUNCTION()
    void HandleBallAtRest(const FOpenFairwayShotResult& Result);

    void ResetAfterShot();
    void ReadyForNextShot();
    /** Hole mode, once a stopped shot has been seen: turns to the cup and picks the club for the next shot. */
    void AddressNextShot();
    /** The round's lie rules: the course's, or the standard. */
    const UOpenGolfLieRules& GetLieRules() const;
    /** After the hole-out banner: tells listeners (the player's menu) that the round is over. */
    void FinishRound();
    /** Turns the ball (and its aim line) toward the cup. */
    void AimAtCup();
    void PlaceOnTee();
    void StartSessionDefaults();
    /** A course round: takes its setup (Setup, or the URL's over the course's defaults with a rolled wind) and orders its holes. */
    void ReadRoundSetup(const OpenGolfPlay::FOpenGolfRoundSetup* Setup);

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
    void ActivateCourseHole();
    /** The layout index of the hole being played. */
    int32 HoleIndex = 0;
    /** Layout indices in play order, from the round's setup; empty for a course without a layout. */
    TArray<int32> HoleOrder;
    /** How far through HoleOrder the round is. */
    int32 OrderPosition = 0;
    /** The travel URL's options, read once the player's preferences are loaded. */
    FString RoundOptions;
    /** The round's setup, with its random wind fixed in the world. */
    OpenGolfPlay::FOpenGolfRoundSetup RoundSetup;
    TArray<int32> CompletedHoleNumbers;
    int32 TeeIndex = 0;
    FName TeeSet;
    FVector PreviousLie = FVector::ZeroVector;
    EGolfPenalty LastPenalty{};  // None
    double LastPenaltyTime = -1;
    float OpeningAimDeg = 0;
    TArray<FOpenGolfHoleRound> CompletedHoles;
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
    /** Hole mode: the club in hand when the last shot stopped; a different club at address is the player's pick. */
    ELaunchMonitorClub ClubAtRest = ELaunchMonitorClub::Driver;
    UPROPERTY()
    TObjectPtr<UGolfGimmieIndicator> GimmieIndicator;
};
