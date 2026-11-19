// SPDX-License-Identifier: MIT
// The ball actor and its cups: capture on landing or rolling, the crossing of an open cup too fast to drop, and the
// hole-out drop. The rules are OpenFairway::CupCapture's, in each cup's own frame (FCupFrame).
#include "OpenFairwayBallInternal.h"

#include "OpenFairway/CupCapture.h"
#include "OpenFairwayLog.h"

using namespace OpenFairway;
using namespace OpenFairwayBallDetail;

void AOpenFairwayBall::SetCupLocations(const TArray<FVector>& Locations)
{
    TArray<FOpenFairwayCup> Descriptors;
    for (const FVector& Location : Locations)
        Descriptors.Add({Location, FVector::UpVector});
    SetCups(Descriptors);
}

void AOpenFairwayBall::SetCups(const TArray<FOpenFairwayCup>& InCups)
{
    Cups.Reset();
    CupLocations.Reset();
    for (const FOpenFairwayCup& Cup : InCups)
    {
        if (Cup.Position.ContainsNaN())
            continue;
        Cups.Add({Cup.Position, Cup.GetNormal()});
        CupLocations.Add(Cup.Position);
    }
    CrossingCup.Reset();
    MeasuredCup.Reset();
}

bool AOpenFairwayBall::TryCaptureInCup(const FVector& Location, bool bLanding)
{
    for (const FOpenFairwayCup& Cup : Cups)
    {
        const FCupFrame Frame(Cup);
        const FVector3f Probe = Frame.ToLocal(Location);
        const FVector3f LocalVelocity = Frame.ToLocalDirection(Velocity);
        if (bLanding ? CupCapture::CapturesLanding(Probe, FVector3f::ZeroVector)
                     : CupCapture::CapturesRolling(Probe, LocalVelocity, FVector3f::ZeroVector))
        {
            StartHoleDrop(Cup, bLanding ? TEXT(" on the fly") : TEXT(""));
            return true;
        }
    }
    return false;
}

bool AOpenFairwayBall::TryEnterCup(const FVector& Start, const FVector& End, const FVector3f& PreviousVelocity,
    const FVector3f& PreviousSpin, float Dt)
{
    // Choose the first crossed aperture, independent of the order courses register their cups.
    const FOpenFairwayCup* First = nullptr;
    float EntryFraction = 2;
    for (const FOpenFairwayCup& Cup : Cups)
    {
        const FCupFrame Frame(Cup);
        const TOptional<float> Fraction = CupCapture::ApertureEntry(Frame.ToLocal(Start), Frame.ToLocalOffset(End - Start));
        if (Fraction && *Fraction < EntryFraction)
        {
            First = &Cup;
            EntryFraction = *Fraction;
        }
    }
    if (!First)
        return false;
    const FOpenFairwayCup Cup = *First;
    const FCupFrame Frame(Cup);
    SimLocation = FMath::Lerp(Start, End, EntryFraction);
    Velocity = FMath::Lerp(PreviousVelocity, Velocity, EntryFraction);
    Omega = FMath::Lerp(PreviousSpin, Omega, EntryFraction);
    const FVector3f Local = Frame.ToLocal(SimLocation);
    const FVector3f LocalVelocity = Frame.ToLocalDirection(Velocity);
    const float Speed = FVector2f(LocalVelocity.X, LocalVelocity.Z).Size();
    const float Offset = Speed > 1e-4f ? FMath::Abs(Local.X * LocalVelocity.Z - Local.Z * LocalVelocity.X) / Speed : 0;
    const float MaxSpeed = CupCapture::MaxCaptureSpeed(Offset);
    if (Speed <= MaxSpeed)
    {
        StartHoleDrop(Cup, TEXT(""));
        return true;
    }
    Result.bOverCup = true;
    Result.OverCupSpeedMps = Speed;
    Result.OverCupMaxSpeedMps = MaxSpeed;
    Result.CupEntrySpinRpm = Omega.Size() / GolfPhysics::RadPerRpm;
    Result.CupEntryOffsetM = Offset;
    Result.CupExitVelocityMps = FVector::ZeroVector;
    Result.CupMaxHeightM = 0;
    MeasuredCup = Cup;
    CrossingCup = Cup;
    CrossingEntrySpeed = Speed;
    bOnGround = false;
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d crossed the cup at %.3f m/s, offset %.4f m, spin %.1f rpm, normal %s (capture limit %.3f m/s)."),
        Result.ShotNumber, Speed, Offset, Result.CupEntrySpinRpm, *Cup.Normal.ToString(), MaxSpeed);
    AdvanceCupCrossing(Dt * (1 - EntryFraction));
    return true;
}

void AOpenFairwayBall::StartHoleDrop(const FOpenFairwayCup& Cup, const TCHAR* How)
{
    bHoled = true;
    Result.bHoled = true;
    // Even one that crossed too fast first: it dropped.
    Result.bOverCup = false;
    HoleDropFrom = SimLocation;
    const FCupFrame Frame(Cup);
    HoleDropTo = Frame.ToWorld(CupCapture::DropPoint(Frame.ToLocal(HoleDropFrom), FVector3f::ZeroVector, PoleRadiusCm / CmPerM));
    HoleDropElapsed = 0.0f;
    // Still moving until the drop finishes; off the ground so nothing rolls it on.
    BallState = EOpenFairwayBallState::Rollout;
    bOnGround = false;
    Velocity = FVector3f::ZeroVector;
    Omega = FVector3f::ZeroVector;
    UpdateResult();
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d holed%s after %.1f s."), Result.ShotNumber, How, Result.ElapsedS);
    PresentAt(SimLocation);
    OnBallHoled.Broadcast(Result);
}

bool AOpenFairwayBall::StepCupCrossing(float Dt)
{
    Result.ElapsedS += Dt;
    if (Result.ElapsedS > MaxShotSeconds)
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Shot %d still moving after %.1f s; stopping it."), Result.ShotNumber, MaxShotSeconds);
        Result.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
        CrossingCup.Reset();
        EnterRest();
        return false;
    }
    ++ShotStep;
    AdvanceCupCrossing(Dt);
    return !bHoled;
}

bool AOpenFairwayBall::AdvanceCupCrossing(float Dt)
{
    const FOpenFairwayCup Cup = *CrossingCup;
    const FCupFrame Frame(Cup);
    FBallState State{Frame.ToLocal(SimLocation), Frame.ToLocalDirection(Velocity), Frame.ToLocalDirection(Omega)};
    const auto Crossing = CupCapture::AdvanceCrossing(State, FVector3f::ZeroVector, Frame.ToLocalDirection(FVector3f(0, -GolfPhysics::Gravity, 0)), Dt);
    SimLocation = Frame.ToWorld(State.Position);
    Velocity = Frame.ToCoreDirection(State.Velocity);
    Omega = Frame.ToCoreDirection(State.Spin);
    UpdateCupHeight();
    if (Crossing.Outcome == CupCapture::ECupCrossing::OverCup)
        return true;

    CrossingCup.Reset();
    if (Crossing.Outcome == CupCapture::ECupCrossing::Holed)
    {
        StartHoleDrop(Cup, TEXT(" off the back of the cup"));
        return false;
    }
    // Above the surface is flight even for a small positive departure velocity. Do not erase a genuine hop.
    bOnGround = State.Position.Y <= GolfPhysics::Radius + 1e-6f && State.Velocity.Y <= 0;
    FloorNormal = ToCore(Cup.Normal);
    if (bOnGround)
        Velocity = RemoveVelocityAlongNormal(Velocity, FloorNormal, false);
    Result.CupExitVelocityMps = ToUnreal(Velocity);
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d left the cup: local velocity %s m/s (entered %.3f m/s)%s."),
        Result.ShotNumber, *State.Velocity.ToString(), CrossingEntrySpeed, bOnGround ? TEXT("") : TEXT(", airborne"));
    const float Remaining = Dt - Crossing.ConsumedSeconds;
    if (Remaining > 1e-7f)
    {
        // The caller already counted Dt; ordinary physics accounts for the remainder itself.
        Result.ElapsedS -= Remaining;
        StepPhysics(Remaining);
    }
    return false;
}

void AOpenFairwayBall::UpdateCupHeight()
{
    if (MeasuredCup)
    {
        const float Height = FVector::DotProduct(SimLocation - MeasuredCup->Position, MeasuredCup->Normal) / CmPerM - GolfPhysics::Radius;
        Result.CupMaxHeightM = FMath::Max(Result.CupMaxHeightM, Height);
    }
}

void AOpenFairwayBall::StepHoleDrop(float DeltaSeconds)
{
    // Slides over the cup in the first half while falling with an eased (t squared) drop.
    HoleDropElapsed += DeltaSeconds;
    const float T = FMath::Min(HoleDropElapsed / HoleDropSeconds, 1.0f);
    FVector Location = FMath::Lerp(HoleDropFrom, HoleDropTo, FMath::Min(T * 2.0f, 1.0f));
    Location.Z = FMath::Lerp(HoleDropFrom.Z, HoleDropTo.Z, T * T);
    // The drop runs per frame, not per step, so the ball is shown where it is.
    SimLocation = Location;
    PresentAt(Location);
    if (T >= 1.0f)
    {
        HoleDropElapsed = -1.0f;
        EnterRest();
    }
}
