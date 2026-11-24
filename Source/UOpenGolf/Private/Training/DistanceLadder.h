// SPDX-License-Identifier: MIT
#pragma once

#include "Training/TargetDrill.h"
#include "DistanceLadder.generated.h"

/**
 * From 15 yd out to 150 yd: one hit moves the pin to the next target, and missing every try
 * moves it back one.
 */
UCLASS()
class UDistanceLadder : public UTargetDrill
{
    GENERATED_BODY()
public:
    virtual FText Name() const override;
    virtual FText Hint() const override;
    virtual bool UsesTries() const override { return true; }
    virtual bool Start(ATrainingSession& Session, int32 InTries, int32 InRadiusFeet) override;
    virtual FDrillChoice GetChoice() const override;
    virtual TSharedRef<SWidget> MakeSettings() override;
    virtual bool IsComplete() const override { return State.IsComplete(); }
    virtual FText ResultText() const override;
    const TrainingScoring::FLadderState& GetState() const { return State; }
    int32 GetTries() const { return Tries; }
    /** The target the next ball is played at. */
    int32 TargetYards() const;

protected:
    virtual int32 FirstYards() const override;
    virtual void ResetScore() override;
    virtual void Score(bool bHit) override;

private:
    FText TargetText() const;
    FText TryText() const;
    FText StepText() const;
    TrainingScoring::FLadderState State;
    int32 Tries = TrainingScoring::DefaultTries;
};
