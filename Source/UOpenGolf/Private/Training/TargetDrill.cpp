// SPDX-License-Identifier: MIT
#include "Training/TargetDrill.h"
#include "Training/TargetRings.h"
#include "Engine/World.h"
#include "Hole/OpenGolfCupComponent.h"
#include "OpenFairwayBall.h"
#include "OpenGolfSettings.h"
#include "OpenGolfTerrain.h"
#include "UI/Common/HudStyle.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"
#include "Widgets/Text/STextBlock.h"

DEFINE_LOG_CATEGORY_STATIC(LogTrainingTarget, Log, All);

namespace
{
    /** A delayed pin move waits this long for a ball that is still rolling. */
    constexpr float RetrySeconds = .5f;
} // namespace

bool UTargetDrill::Start(ATrainingSession& Session, int32 /*Tries*/, int32 InRadiusFeet)
{
    const AUOpenGolfGameMode* Mode = Session.Mode();
    if (!TrainingScoring::IsValidRadius(InRadiusFeet) || !Mode || !Mode->GetBall())
        return false;
    Training = &Session;
    RadiusFeet = InRadiusFeet;
    Rings = UTargetRings::Create(Session, TrainingScoring::RadiusText(RadiusFeet));
    if (!Rings)
        return false;
    ResetScore();
    RunStartShot = Mode->GetBall()->GetShotNumber();
    if (PlacePin(FirstYards()))
        return true;
    Rings->Destroy();
    Rings = nullptr;
    return false;
}

FDrillChoice UTargetDrill::GetChoice() const
{
    FDrillChoice Choice = Super::GetChoice();
    Choice.RadiusFeet = RadiusFeet;
    return Choice;
}

void UTargetDrill::Restart(ATrainingSession& Session)
{
    ClearTimers();
    PendingYards.Reset();
    ClearLastBall();
    LastScoredShot = INDEX_NONE;
    RunStartShot = Session.Mode()->GetBall()->GetShotNumber();
    ResetScore();
    if (PinYards != FirstYards() && !PlacePin(FirstYards()))
        UE_LOG(LogTrainingTarget, Warning, TEXT("Could not move the target back to %d yd."), FirstYards());
}

void UTargetDrill::Finish(ATrainingSession& Session, bool bRestorePin)
{
    ClearTimers();
    PendingYards.Reset();
    AUOpenGolfGameMode* Mode = Session.Mode();
    if (AUOpenGolfRange* Range = (bRestorePin && Mode) ? Mode->GetRange() : nullptr)
    {
        Range->GetCup()->PlaceOnSurface(Range->GetOriginalCup(), Range->GetFlagstick());
        Mode->GetBall()->SetCups({Range->GetOriginalCup()});
    }
    if (Rings)
        Rings->Destroy();
    Rings = nullptr;
    Training.Reset();
}

void UTargetDrill::OnShotAtRest(ATrainingSession& Session, const FOpenFairwayShotResult& Result, FVector RestLocation)
{
    // A ball from before this run, or a second report of the same ball, is not a try.
    if (IsComplete() || Result.ShotNumber <= RunStartShot || Result.ShotNumber <= LastScoredShot)
        return;
    LastScoredShot = Result.ShotNumber;
    // Scored against the pin the ball was hit at: a pending move has not happened yet.
    const double DistanceCm = FVector::Dist2D(RestLocation, Session.Mode()->GetCupLocation());
    const bool bSafetyStop = Result.TerminationReason == EOpenFairwayShotTermination::SafetyStop;
    const double StripeWidthCm = UTargetRings::EdgeWidthCm(PinYards * TrainingScoring::CmPerYard);
    bLastHit = TrainingScoring::IsHit(DistanceCm, Result.bHoled, bSafetyStop, GetRadiusCm(), StripeWidthCm);
    LastDistanceCm = Result.bHoled ? 0.0 : DistanceCm;
    bLastHoled = Result.bHoled;
    Score(bLastHit);
}

bool UTargetDrill::PlacePin(int32 Yards)
{
    AUOpenGolfGameMode* Mode = Training.IsValid() ? Training->Mode() : nullptr;
    if (!Mode || !Mode->GetBall() || Mode->GetBall()->IsMoving() || !IsValid(Rings))
        return false;
    AUOpenGolfRange* Range = Mode->GetRange();
    if (!Range || !Range->Terrain || !Range->GetFlagstick() || !Range->GetCup())
        return false;
    const FVector Tee = Range->GetTeeLocation();
    const FVector Line = (Range->GetOriginalCup().Position - Tee).GetSafeNormal2D();
    if (Line.IsNearlyZero())
        return false;
    const double DistanceCm = Yards * TrainingScoring::CmPerYard;
    FVector Ground, Normal;
    if (!Range->Terrain->GroundAt(Tee + Line * DistanceCm, Ground, Normal, AUOpenGolfBall::ContactChannel))
        return false;
    if (!Rings->Place(*Range->Terrain, Ground, Line, DistanceCm, GetRadiusCm(), AUOpenGolfBall::ContactChannel))
        return false;
    Range->GetCup()->PlaceOnSurface({Ground, Normal}, Range->GetFlagstick());
    Mode->GetBall()->SetCups({Range->GetCup()->GetCupDescriptor()});
    PinYards = Yards;
    return true;
}

void UTargetDrill::MovePinLater(int32 Yards)
{
    PendingYards = Yards;
    if (Training.IsValid())
        Training->GetWorldTimerManager().SetTimer(
            PendingTimer, FTimerDelegate::CreateUObject(this, &UTargetDrill::ApplyPending), AUOpenGolfGameMode::AddressDelayS, false);
}

void UTargetDrill::ApplyPending()
{
    if (!PendingYards || !Training.IsValid())
        return;
    FTimerManager& Timers = Training->GetWorldTimerManager();
    Timers.ClearTimer(PendingTimer);
    if (PlacePin(PendingYards.GetValue()))
    {
        PendingYards.Reset();
        return;
    }
    const AOpenFairwayBall* Ball = Training->Mode() ? Training->Mode()->GetBall() : nullptr;
    if (Ball && Ball->IsMoving())
    {
        Timers.SetTimer(PendingTimer, FTimerDelegate::CreateUObject(this, &UTargetDrill::ApplyPending), RetrySeconds, false);
        return;
    }
    UE_LOG(LogTrainingTarget, Warning, TEXT("Could not move the target to %d yd; it stays at %d yd."), PendingYards.GetValue(), PinYards);
    PendingYards.Reset();
}

void UTargetDrill::ReportLater()
{
    if (!Training.IsValid())
        return;
    Training->GetWorldTimerManager().SetTimer(ReportTimer, FTimerDelegate::CreateWeakLambda(this, [this] {
        if (Training.IsValid() && Training->Active() == this && IsComplete())
            Training->OpenDrillDialog();
    }), AUOpenGolfGameMode::AddressDelayS, false);
}

void UTargetDrill::ClearTimers()
{
    if (!Training.IsValid())
        return;
    Training->GetWorldTimerManager().ClearTimer(PendingTimer);
    Training->GetWorldTimerManager().ClearTimer(ReportTimer);
}

FText UTargetDrill::LastBallText() const
{
    if (!LastDistanceCm)
        return FText::GetEmpty();
    const double DistanceCm = LastDistanceCm.GetValue();
    const FText Radius = TrainingScoring::RadiusText(RadiusFeet);
    FText Verdict = NSLOCTEXT("Training", "Miss", "miss");
    if (bLastHoled)
        Verdict = NSLOCTEXT("Training", "Holed", "holed");
    else if (bLastHit)
        Verdict = DistanceCm >= GetRadiusCm()
            ? FText::Format(NSLOCTEXT("Training", "OnRadiusLine", "on the {0} line"), Radius)
            : FText::Format(NSLOCTEXT("Training", "InsideRadius", "inside {0}"), Radius);
    return FText::Format(NSLOCTEXT("Training", "LastBall", "Last ball  {0}  ·  {1}"),
        FText::FromString(GolfSettings::Distance(DistanceCm / 100.0, true)), Verdict);
}

void UTargetDrill::ClearLastBall()
{
    LastDistanceCm.Reset();
    bLastHit = false;
    bLastHoled = false;
}

bool UTargetDrill::IsBallMoving() const
{
    const AUOpenGolfGameMode* Mode = Training.IsValid() ? Training->Mode() : nullptr;
    return Mode && Mode->GetBall() && Mode->GetBall()->IsMoving();
}

TSharedRef<SWidget> UTargetDrill::MakeLine(TAttribute<FText> Text, int32 Size) const
{
    return SNew(STextBlock).Font(HudStyle::Font("Regular", Size)).ColorAndOpacity(HudStyle::TextColor()).Text(Text)
        .Visibility_Lambda([Text] { return Text.Get().IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; });
}
