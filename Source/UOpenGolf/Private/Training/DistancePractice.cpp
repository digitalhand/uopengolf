// SPDX-License-Identifier: MIT
#include "Training/DistancePractice.h"
#include "UI/Common/HudSlider.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

FText UDistancePractice::Name() const
{
    return NSLOCTEXT("Training", "DistancePractice", "Distance Practice");
}

bool UDistancePractice::SetDistance(int32 Yards)
{
    if (!CanChangeDistance())
        return false;
    const int32 Clamped = FMath::Clamp(Yards, MinYards, MaxYards);
    if (!PlacePin(Clamped))
        return false;
    Distance = Clamped;
    return true;
}

void UDistancePractice::ResetScore()
{
    Set = TrainingScoring::FTargetSet{Tries};
}

void UDistancePractice::Score(TrainingScoring::ERing Ring)
{
    Set = TrainingScoring::Record(Set, Ring);
    if (Set.IsComplete())
        ReportLater();
}

FText UDistancePractice::ProgressText() const
{
    return FText::Format(NSLOCTEXT("Training", "PracticeProgress", "Inside 3 ft  {0}   ·   Inside 6 ft  {1}"), Set.Inside3, Set.Inside6);
}

FText UDistancePractice::ResultText() const
{
    if (Set.Shots == 0)
        return FText::GetEmpty();
    return FText::Format(NSLOCTEXT("Training", "PracticeResult", "{0} yd  ·  {1} of {2} shots  ·  inside 3 ft: {3}  ·  inside 6 ft: {4}"), Distance,
        Set.Shots, Set.Tries, Set.Inside3, Set.Inside6);
}

TSharedRef<SWidget> UDistancePractice::MakeSettings()
{
    const TWeakObjectPtr<UDistancePractice> Drill(this);
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).Text_Lambda([Drill] {
                return Drill.IsValid() ? FText::Format(NSLOCTEXT("Training", "TargetDistance", "Target distance  {0} yd"), Drill->Distance) : FText::GetEmpty();
            })
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHudSlider).Tag(TEXT("TrainingDistance")).MinValue(MinYards).MaxValue(MaxYards).Step(YardStep)
#if WITH_ACCESSIBILITY
            .AccessibleText(NSLOCTEXT("Training", "TargetYards", "Target distance in yards"))
#endif
            .IsEnabled_Lambda([Drill] { return Drill.IsValid() && Drill->CanChangeDistance() && !Drill->IsBallMoving(); })
            .Value_Lambda([Drill] { return Drill.IsValid() ? float(Drill->Distance) : 0.f; })
            .OnValueChanged_Lambda([Drill](float Value) { if (Drill.IsValid()) Drill->SetDistance(FMath::RoundToInt(Value / YardStep) * YardStep); })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
        [
            MakeLine(TAttribute<FText>::CreateLambda([Drill] {
                return Drill.IsValid() ? FText::Format(NSLOCTEXT("Training", "PracticeShots", "Shot  {0} of {1}"),
                    FMath::Min(Drill->Set.Shots + 1, Drill->Set.Tries), Drill->Set.Tries) : FText::GetEmpty();
            }), 11)
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
        [ MakeLine(TAttribute<FText>::CreateLambda([Drill] { return Drill.IsValid() ? Drill->ProgressText() : FText::GetEmpty(); })) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
        [ MakeLine(TAttribute<FText>::CreateLambda([Drill] { return Drill.IsValid() ? Drill->LastBallText() : FText::GetEmpty(); })) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
        [
            MakeLine(TAttribute<FText>::CreateLambda([Drill] {
                return Drill.IsValid() && Drill->IsComplete()
                    ? NSLOCTEXT("Training", "PracticeDone", "Set complete  ·  Esc for Play Again or Exit") : FText::GetEmpty();
            }))
        ];
    // clang-format on
}
