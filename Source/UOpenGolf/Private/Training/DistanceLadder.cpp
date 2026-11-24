// SPDX-License-Identifier: MIT
#include "Training/DistanceLadder.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SBoxPanel.h"

using TrainingScoring::ELadderStep;
using TrainingScoring::LadderYards;

FText UDistanceLadder::Name() const
{
    return NSLOCTEXT("Training", "DistanceLadder", "Distance Ladder");
}

int32 UDistanceLadder::FirstYards() const
{
    return LadderYards()[0];
}

int32 UDistanceLadder::TargetYards() const
{
    return LadderYards()[State.Stage];
}

void UDistanceLadder::ResetScore()
{
    State = TrainingScoring::FLadderState();
}

void UDistanceLadder::Score(TrainingScoring::ERing Ring)
{
    const int32 Before = State.Stage;
    State = TrainingScoring::Next(State, Tries, Ring == TrainingScoring::ERing::Inside3Ft);
    if (State.IsComplete())
        ReportLater();
    else if (State.Stage != Before)
        MovePinLater(TargetYards());
}

FText UDistanceLadder::TargetText() const
{
    return FText::Format(NSLOCTEXT("Training", "LadderTarget", "Target  {0} yd  ·  {1} of {2}"), TargetYards(), State.Stage + 1, LadderYards().Num());
}

FText UDistanceLadder::TryText() const
{
    if (State.IsComplete())
        return FText::GetEmpty();
    FText Try = FText::Format(NSLOCTEXT("Training", "LadderTry", "Try  {0} of {1}"), State.TriesUsed + 1, Tries);
    if (State.BestStage == INDEX_NONE)
        return Try;
    return FText::Format(NSLOCTEXT("Training", "LadderTryBest", "{0}   ·   Best  {1} yd"), Try, LadderYards()[State.BestStage]);
}

FText UDistanceLadder::StepText() const
{
    switch (State.LastStep)
    {
    case ELadderStep::Advanced:
        return FText::Format(NSLOCTEXT("Training", "LadderAdvanced", "Inside 3 ft: on to {0} yd"), TargetYards());
    case ELadderStep::DroppedBack:
        return FText::Format(NSLOCTEXT("Training", "LadderDropped", "No ball inside 3 ft: back to {0} yd"), TargetYards());
    case ELadderStep::Repeated:
        return FText::Format(NSLOCTEXT("Training", "LadderRepeated", "No ball inside 3 ft: again at {0} yd"), TargetYards());
    case ELadderStep::Completed:
        return NSLOCTEXT("Training", "LadderDone", "Ladder complete  ·  Esc for Play Again or Exit");
    default:
        return FText::GetEmpty();
    }
}

FText UDistanceLadder::ResultText() const
{
    if (State.IsComplete())
        return FText::Format(NSLOCTEXT("Training", "LadderResult", "Cleared {0} yd in {1} shots"), TargetYards(), State.TotalShots);
    if (State.TotalShots == 0)
        return FText::GetEmpty();
    const FText Best = State.BestStage == INDEX_NONE ? NSLOCTEXT("Training", "LadderNoBest", "none yet")
                                                     : FText::Format(NSLOCTEXT("Training", "LadderBestYards", "{0} yd"), LadderYards()[State.BestStage]);
    return FText::Format(NSLOCTEXT("Training", "LadderProgress", "Target {0} yd  ·  best {1}  ·  {2} shots"), TargetYards(), Best, State.TotalShots);
}

TSharedRef<SWidget> UDistanceLadder::MakeSettings()
{
    const TWeakObjectPtr<UDistanceLadder> Drill(this);
    auto Line = [Drill](FText (UDistanceLadder::*Get)() const) {
        return TAttribute<FText>::CreateLambda([Drill, Get] { return Drill.IsValid() ? (Drill.Get()->*Get)() : FText::GetEmpty(); });
    };
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[ MakeLine(Line(&UDistanceLadder::TargetText), 11) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[ MakeLine(Line(&UDistanceLadder::TryText)) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[ MakeLine(Line(&UDistanceLadder::LastBallText)) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[ MakeLine(Line(&UDistanceLadder::StepText)) ];
    // clang-format on
}
