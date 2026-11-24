// SPDX-License-Identifier: MIT
#pragma once

#include "Training/TargetDrill.h"
#include "DistancePractice.generated.h"

/**
 * Open-ended practice at one yardage: the player hits as many balls as they like and the game counts how many finish
 * in the ring. The yardage can change whenever the ball is at rest, and each change starts a new count.
 */
UCLASS()
class UDistancePractice : public UTargetDrill
{
    GENERATED_BODY()
public:
    static constexpr int32 MinYards = 10;
    static constexpr int32 MaxYards = 250;
    /** The distance slider moves in whole steps of this many yards. */
    static constexpr int32 YardStep = 5;
    virtual FText Name() const override;
    virtual FText Hint() const override;
    virtual TSharedRef<SWidget> MakeSettings() override;
    virtual FText ResultText() const override;
    /** Moves the pin and starts a new count; refused while the ball moves. */
    bool SetDistance(int32 Yards);
    bool CanChangeDistance() const { return !IsBallMoving(); }
    int32 GetDistance() const { return Distance; }
    const TrainingScoring::FHitTally& GetTally() const { return Tally; }

protected:
    virtual int32 FirstYards() const override { return Distance; }
    virtual void ResetScore() override;
    virtual void Score(bool bHit) override;

private:
    FText ProgressText() const;
    TrainingScoring::FHitTally Tally;
    int32 Distance = 25;
};
