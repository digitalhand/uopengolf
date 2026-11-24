// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "OpenFairwayTypes.h"
#include "Widgets/SNullWidget.h"
#include "TrainingSession.generated.h"

class AUOpenGolfGameMode;
class SWidget;
class UStaticMeshComponent;
class ATrainingSession;

UCLASS(Abstract, Transient)
class UTrainingDrill : public UObject
{
    GENERATED_BODY()
public:
    virtual FText Name() const PURE_VIRTUAL(UTrainingDrill::Name, return FText::GetEmpty(););
    /** Tries is one of TrainingScoring::TryChoices, chosen with the game. */
    virtual bool Start(ATrainingSession& Session, int32 Tries) PURE_VIRTUAL(UTrainingDrill::Start, return false;);
    /** Called with the ball stopped at the tee. */
    virtual void Restart(ATrainingSession& Session) {}
    virtual void Finish(ATrainingSession& Session) {}
    virtual TSharedRef<SWidget> MakeSettings() PURE_VIRTUAL(UTrainingDrill::MakeSettings, return SNullWidget::NullWidget;);
    virtual bool AllowsPlacement() const { return false; }
    /** Every shot that comes to rest during the drill, including safety stops. */
    virtual void OnShotAtRest(ATrainingSession& Session, const FOpenFairwayShotResult& Result, FVector RestLocation) {}
    /** Finish any delayed change to the target now, before the next shot. */
    virtual void ApplyPending() {}
    /** The drill has ended; it takes no more shots until it is restarted. */
    virtual bool IsComplete() const { return false; }
    /** A one-line summary for the drill dialog; empty until there is something to report. */
    virtual FText ResultText() const { return FText::GetEmpty(); }
};

/** A game and its number of tries, as picked from the training games menu. */
struct FDrillChoice
{
    TSubclassOf<UTrainingDrill> Game;
    int32 Tries = 0;
};

UCLASS(Transient, NotPlaceable)
class ATrainingSession : public AActor
{
    GENERATED_BODY()
public:
    ATrainingSession();
    static TConstArrayView<TSubclassOf<UTrainingDrill>> Games();
    AUOpenGolfGameMode* Mode() const;
    /** Starts Game with Tries; refused while a game runs, in a course round or while the ball moves. */
    bool StartDrill(TSubclassOf<UTrainingDrill> Game, int32 Tries);
    /**
     * The training games menu's choice: starts Game, or with a game running opens the Exit Drill prompt, whose Exit
     * Drill starts Game (StartPendingDrill). The running game carries on until then.
     */
    bool ChooseDrill(TSubclassOf<UTrainingDrill> Game, int32 Tries);
    /** The game the Exit Drill prompt will start, if the player picked one while another ran. */
    const TOptional<FDrillChoice>& GetPendingDrill() const { return PendingDrill; }
    /** Exits the running game and starts the pending one. */
    bool StartPendingDrill();
    void CancelPendingDrill() { PendingDrill.Reset(); }
    void RestartDrill();
    void ExitDrill();
    UTrainingDrill* Active() const { return ActiveDrill; }
    void RememberRest(const FOpenFairwayShotResult& Result);
    void LeaveFinishedBall();
    int32 FinishedBallCount() const;
    /** Why a launch-monitor shot cannot be played now, or empty when it can. */
    FString RejectReason() const;
    /** A launch-monitor shot is about to be played. */
    void BeforeShot();
    /** Shows the drill dialog, for example with a finished drill's result. */
    void OpenDrillDialog();

private:
    bool CanStartDrill(TSubclassOf<UTrainingDrill> Game, int32 Tries) const;
    void TeeUpForDrill();
    TOptional<FDrillChoice> PendingDrill;
    UPROPERTY()
    TObjectPtr<UTrainingDrill> ActiveDrill;
    UPROPERTY()
    TArray<TObjectPtr<UStaticMeshComponent>> FinishedBalls;
    TOptional<FTransform> PendingBall;
    int32 RunStart = 0;
};
