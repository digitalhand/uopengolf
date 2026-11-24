// SPDX-License-Identifier: MIT
#include "Training/TrainingScoring.h"
#include "OpenFairway/GolfPhysics.h"

namespace TrainingScoring
{
    bool IsValidTries(int32 Tries)
    {
        return MakeArrayView(TryChoices).Contains(Tries);
    }

    bool IsValidRadius(int32 Feet)
    {
        return MakeArrayView(RadiusChoices).Contains(Feet);
    }

    FText RadiusText(int32 Feet)
    {
        return FText::Format(NSLOCTEXT("Training", "RadiusFeet", "{0} ft"), Feet);
    }

    bool IsHit(double DistanceCm, bool bHoled, bool bSafetyStop, double RadiusCm, double StripeWidthCm)
    {
        if (bSafetyStop)
            return false;
        const double BallRadiusCm = OpenFairway::GolfPhysics::Radius * 100.;
        // Allow for rounding when subtracting world positions at exact contact.
        return bHoled || DistanceCm <= RadiusCm + StripeWidthCm + BallRadiusCm + UE_DOUBLE_SMALL_NUMBER;
    }

    FHitTally Record(const FHitTally& Tally, bool bHit)
    {
        return {Tally.Shots + 1, Tally.Hits + (bHit ? 1 : 0)};
    }

    TConstArrayView<int32> LadderYards()
    {
        static const int32 Yards[] = {15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115, 125, 135, 145, 150};
        return MakeArrayView(Yards);
    }

    FLadderState Next(const FLadderState& State, int32 Tries, bool bHit)
    {
        if (State.IsComplete())
            return State;
        FLadderState Out = State;
        ++Out.TotalShots;
        if (bHit)
        {
            Out.BestStage = FMath::Max(Out.BestStage, Out.Stage);
            Out.TriesUsed = 0;
            if (Out.Stage == LadderYards().Num() - 1)
            {
                Out.LastStep = ELadderStep::Completed;
                return Out;
            }
            ++Out.Stage;
            Out.LastStep = ELadderStep::Advanced;
            return Out;
        }
        ++Out.TriesUsed;
        if (Out.TriesUsed < Tries)
        {
            Out.LastStep = ELadderStep::Missed;
            return Out;
        }
        Out.TriesUsed = 0;
        Out.LastStep = Out.Stage > 0 ? ELadderStep::DroppedBack : ELadderStep::Repeated;
        Out.Stage = FMath::Max(0, Out.Stage - 1);
        return Out;
    }
} // namespace TrainingScoring
