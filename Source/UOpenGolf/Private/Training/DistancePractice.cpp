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

FText UDistancePractice::Hint() const
{
    return NSLOCTEXT("Training", "PracticeHint", "Hit as many balls as you like. Changing the distance starts a new count.");
}

bool UDistancePractice::SetDistance(int32 Yards)
{
    if (!CanChangeDistance())
        return false;
    const int32 Clamped = FMath::Clamp(Yards, MinYards, MaxYards);
    // The slider reports every drag step; the same yardage keeps the count.
    if (Clamped == Distance && GetPinYards() == Distance)
        return true;
    if (!PlacePin(Clamped))
        return false;
    Distance = Clamped;
    ResetScore();
    ClearLastBall();
    return true;
}

void UDistancePractice::ResetScore()
{
    Tally = TrainingScoring::FHitTally();
}

void UDistancePractice::Score(bool bHit)
{
    Tally = TrainingScoring::Record(Tally, bHit);
}

FText UDistancePractice::ProgressText() const
{
    const FText Radius = TrainingScoring::RadiusText(GetRadiusFeet());
    if (Tally.Shots == 0)
        return FText::Format(NSLOCTEXT("Training", "PracticeRing", "{0} ring"), Radius);
    return FText::Format(NSLOCTEXT("Training", "PracticeHits", "{0} of {1} shots in the {2} ring"), Tally.Hits, Tally.Shots, Radius);
}

FText UDistancePractice::ResultText() const
{
    if (Tally.Shots == 0)
        return FText::GetEmpty();
    return FText::Format(NSLOCTEXT("Training", "PracticeSetResult", "{0} yd  ·  {1}"), Distance, ProgressText());
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
            .IsEnabled_Lambda([Drill] { return Drill.IsValid() && Drill->CanChangeDistance(); })
            .Value_Lambda([Drill] { return Drill.IsValid() ? float(Drill->Distance) : 0.f; })
            .OnValueChanged_Lambda([Drill](float Value) { if (Drill.IsValid()) Drill->SetDistance(FMath::RoundToInt(Value / YardStep) * YardStep); })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
        [ MakeLine(TAttribute<FText>::CreateLambda([Drill] { return Drill.IsValid() ? Drill->ProgressText() : FText::GetEmpty(); }), 11) ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)
        [ MakeLine(TAttribute<FText>::CreateLambda([Drill] { return Drill.IsValid() ? Drill->LastBallText() : FText::GetEmpty(); })) ];
    // clang-format on
}
