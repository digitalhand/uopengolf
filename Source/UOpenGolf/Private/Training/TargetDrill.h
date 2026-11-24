// SPDX-License-Identifier: MIT
#pragma once

#include "Training/TrainingScoring.h"
#include "Training/TrainingSession.h"
#include "TimerManager.h"
#include "TargetDrill.generated.h"

class UTargetRings;

/**
 * A pin on the line from the tee to the range's cup, ringed at 3 ft and 6 ft, scored where each ball stops.
 * Subclasses choose the yardage and keep the score.
 */
UCLASS(Abstract)
class UTargetDrill : public UTrainingDrill
{
    GENERATED_BODY()
public:
    virtual bool Start(ATrainingSession& Session, int32 Tries) override;
    virtual void Restart(ATrainingSession& Session) override;
    virtual void Finish(ATrainingSession& Session) override;
    virtual void OnShotAtRest(ATrainingSession& Session, const FOpenFairwayShotResult& Result, FVector RestLocation) override;
    virtual void ApplyPending() override;
    int32 GetTries() const { return Tries; }
    int32 GetPinYards() const { return PinYards; }
    UTargetRings* GetRings() const { return Rings; }
    /** Set while a new target waits to replace the one the last ball was hit at. */
    TOptional<int32> GetPendingYards() const { return PendingYards; }

protected:
    virtual int32 FirstYards() const PURE_VIRTUAL(UTargetDrill::FirstYards, return 0;);
    /** Back to an empty score for a new run at the same settings. */
    virtual void ResetScore() PURE_VIRTUAL(UTargetDrill::ResetScore, );
    virtual void Score(TrainingScoring::ERing Ring) PURE_VIRTUAL(UTargetDrill::Score, );
    /** Moves the pin, rings and physics cup; refused while the ball moves or if the ground cannot be found. */
    bool PlacePin(int32 Yards);
    /** Moves the pin once the player has seen where the last ball stopped, or before the next shot. */
    void MovePinLater(int32 Yards);
    /** Shows the drill dialog with the result once the player has seen where the last ball stopped. */
    void ReportLater();
    /** The last scored ball's distance from the pin, such as "Last ball  2.6 ft  ·  inside 3 ft". */
    FText LastBallText() const;
    TSharedRef<SWidget> MakeLine(TAttribute<FText> Text, int32 Size = 10) const;
    bool IsBallMoving() const;
    int32 Tries = 3;

private:
    void ClearTimers();
    TWeakObjectPtr<ATrainingSession> Training;
    UPROPERTY()
    TObjectPtr<UTargetRings> Rings;
    int32 PinYards = 0;
    /** Results up to this shot number came before the current run. */
    int32 RunStartShot = 0;
    int32 LastScoredShot = INDEX_NONE;
    TOptional<double> LastDistanceCm;
    TOptional<TrainingScoring::ERing> LastRing;
    bool bLastHoled = false;
    TOptional<int32> PendingYards;
    FTimerHandle PendingTimer;
    FTimerHandle ReportTimer;
};
