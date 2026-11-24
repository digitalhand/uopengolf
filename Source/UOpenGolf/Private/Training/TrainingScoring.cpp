// SPDX-License-Identifier: MIT
#include "Training/TrainingScoring.h"

namespace TrainingScoring
{
    bool IsValidTries(int32 Tries)
    {
        for (const int32 Choice : TryChoices)
            if (Choice == Tries)
                return true;
        return false;
    }

    ERing Classify(double DistanceCm, bool bHoled, bool bSafetyStop)
    {
        if (bSafetyStop)
            return ERing::Miss;
        if (bHoled || DistanceCm <= InnerRadiusCm)
            return ERing::Inside3Ft;
        return DistanceCm <= OuterRadiusCm ? ERing::Inside6Ft : ERing::Miss;
    }

    FTargetSet Record(const FTargetSet& Set, ERing Ring)
    {
        if (Set.IsComplete())
            return Set;
        FTargetSet Out = Set;
        ++Out.Shots;
        Out.Inside3 += Ring == ERing::Inside3Ft ? 1 : 0;
        Out.Inside6 += Ring != ERing::Miss ? 1 : 0;
        return Out;
    }

    TConstArrayView<int32> LadderYards()
    {
        static const int32 Yards[] = {15, 25, 35, 45, 55, 65, 75, 85, 95, 105, 115, 125, 135, 145, 150};
        return MakeArrayView(Yards);
    }

    FLadderState Next(const FLadderState& State, int32 Tries, bool bInside3)
    {
        if (State.IsComplete())
            return State;
        FLadderState Out = State;
        ++Out.TotalShots;
        if (bInside3)
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
