// SPDX-License-Identifier: MIT
#pragma once

#include "Training/TargetDrill.h"
#include "DistancePractice.generated.h"

/** One set of 3 or 5 tries at a yardage the player picks before the first shot. */
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
    virtual TSharedRef<SWidget> MakeSettings() override;
    virtual bool IsComplete() const override { return Set.IsComplete(); }
    virtual FText ResultText() const override;
    /** Only before the first shot of a set, and never in flight. */
    bool SetDistance(int32 Yards);
    bool CanChangeDistance() const { return Set.Shots == 0; }
    int32 GetDistance() const { return Distance; }
    const TrainingScoring::FTargetSet& GetSet() const { return Set; }

protected:
    virtual int32 FirstYards() const override { return Distance; }
    virtual void ResetScore() override;
    virtual void Score(TrainingScoring::ERing Ring) override;

private:
    FText ProgressText() const;
    TrainingScoring::FTargetSet Set;
    int32 Distance = 25;
};
