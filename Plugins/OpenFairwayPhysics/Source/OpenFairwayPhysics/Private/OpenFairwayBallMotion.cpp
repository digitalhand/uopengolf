// SPDX-License-Identifier: MIT
// One physics step of the ball actor: the OpenFairway flight or ground step, then a Chaos sphere sweep along the move,
// the contact response (landing, bounce, roll, obstacle, wall) and canopy drag.
#include "OpenFairwayBallInternal.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "OpenFairway/Obstacles.h"
#include "OpenFairway/ShotStep.h"
#include "OpenFairwayLog.h"

using namespace OpenFairway;
using namespace OpenFairwayBallDetail;

bool AOpenFairwayBall::StepPhysics(float Dt)
{
    if (CrossingCup)
    {
        return StepCupCrossing(Dt);
    }
    if (ShotStep == 0 && BallState == EOpenFairwayBallState::Flight)
    {
        FGroundHit Ground;
        if (ProbeGround(Ground, GroundProbeUpM, GolfPhysics::Radius + GroundProbeDistanceM) &&
            FVector3f::DotProduct(Velocity, Ground.Normal) <= 0.0f)
        {
            FloorNormal = Ground.Normal;
            SetSurface(ResolveSurface(Ground.Component.Get(), Ground.PhysMaterial.Get()));
            RecordLanding(FloorNormal);
            const FBallState Initial{CorePosition(), Velocity, Omega};
            TracePhysics(DiagnosticTrace, 0, Dt, TEXT("ImpactIn"), ToCoreSurface(CurrentSurface), Initial, FloorNormal);
            StruckWeight = ShotStep::StruckWeight(Velocity, FloorNormal);
            const ShotStep::FImpactOutcome Contact = ShotStep::Impact(Velocity, Omega, FloorNormal, SurfaceSettings, ShotParams, true);
            Velocity = Contact.Velocity;
            Omega = Contact.Spin;
            BallState = EOpenFairwayBallState::Rollout;
            bOnGround = !Contact.bRebounds;
            if (bOnGround) Velocity = ShotStep::Settle(Velocity, FloorNormal);
            TracePhysics(DiagnosticTrace, 0, Dt, TEXT("ImpactOut"), ToCoreSurface(CurrentSurface), {Initial.Position, Velocity, Omega}, FloorNormal);
            BroadcastLanded();
        }
    }
    const bool bWasOnGround = bOnGround;
    const FVector3f PreviousVelocity = Velocity;
    const FVector3f PreviousSpin = Omega;
    // Flight reads the height above the launch ground: ShotStart is the ball centre, one radius up.
    const FBallState State{CorePosition(), Velocity, Omega};
    // A full shot struck into or along the ground keeps its struck grass drag on every surface it rolls over.
    const FBallState Next = bWasOnGround ? GolfPhysics::StepGround(State, FloorNormal, GolfPhysics::StruckAlongGround(SurfaceSettings, StruckWeight),
                                               ShotParams.Rollout, RolloutImpactSpinRpm, Dt, ShotParams.Environment)
                                         : GolfPhysics::StepFlight(State, ShotParams.Environment, ShotParams.Flight, Dt);
    Velocity = Next.Velocity;
    Omega = Next.Spin;
    Result.ElapsedS += Dt;
    if (Result.ElapsedS > MaxShotSeconds)
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Shot %d still moving after %.1f s; stopping it."), Result.ShotNumber, MaxShotSeconds);
        Result.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
        EnterRest();
        return false;
    }

    const FVector Start = SimLocation;
    const FVector Offset = Start - ShotStart;
    if (FMath::Abs(Offset.X) > OutOfBoundsM * CmPerM || FMath::Abs(Offset.Y) > OutOfBoundsM * CmPerM || Offset.Z < -FallThroughM * CmPerM)
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Ball left the playable area at %s; stopping it."), *Start.ToString());
        Result.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
        EnterRest();
        return false;
    }

    // Intersect the actual predicted segment before sweeping against the uncut green. This also catches
    // a complete crossing between two samples and keeps entry height/normal velocity on sloped greens.
    if (bWasOnGround && TryEnterCup(Start, Start + ToUnreal(Velocity * Dt) * CmPerM, PreviousVelocity, PreviousSpin, Dt))
    {
        UpdateCupHeight();
        return !bHoled;
    }

    auto MoveWithSweep = [this](const FVector& Delta, FHitResult& Hit)
    {
        if (Delta.IsNearlyZero(1e-4))
        {
            return false;
        }
        const FVector MoveStart = SimLocation;
        const FVector End = MoveStart + Delta;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(OpenFairwayBallSweep), false, this);
        Params.bReturnPhysicalMaterial = true;
        // Airborne sweeps must detect the true sphere's first contact. Shrinking it can
        // postpone a grazing touchdown by a full step, past the RK4 contact solver's interval.
        // Supported movement retains clearance so tangential sweeps do not stick to turf.
        const bool bDescending = !bOnGround && FVector3f::DotProduct(Velocity, FloorNormal) < -0.01f;
        float SweepMarginCm = bDescending ? 0.0f : SafeMarginCm;
        bool bHit = GetWorld()->SweepSingleByChannel(
            Hit, MoveStart, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(GolfPhysics::Radius * CmPerM - SweepMarginCm), Params);
        if (bHit && Hit.bStartPenetrating && bDescending)
        {
            // Terrain/float tolerances can leave a supported sphere barely overlapping.
            // This is recovery, not a new airborne impact; preserve the usual clearance.
            SweepMarginCm = SafeMarginCm;
            Hit = FHitResult();
            bHit = GetWorld()->SweepSingleByChannel(Hit, MoveStart, End, FQuat::Identity, TraceChannel,
                FCollisionShape::MakeSphere(GolfPhysics::Radius * CmPerM - SweepMarginCm), Params);
        }
        if (bHit && Hit.bBlockingHit)
        {
            // Rest the true sphere tangent to the contact (the sweep sphere was a margin smaller).
            SimLocation = Hit.bStartPenetrating ? MoveStart + Hit.Normal * (Hit.PenetrationDepth + SafeMarginCm) : Hit.Location + Hit.ImpactNormal * SweepMarginCm;
            return true;
        }
        SimLocation = End;
        return false;
    };
    FHitResult Hit;
    const FVector3f Displacement = bWasOnGround ? Velocity * Dt : Next.Position - State.Position;
    const bool bHit = MoveWithSweep(ToUnreal(Displacement) * CmPerM, Hit);
    if (bHit && !bWasOnGround && IsGroundNormal(ToCore(Hit.ImpactNormal)) && !IsObstacle(Hit.GetComponent()))
    {
        const FVector3f Normal = ToCore(Hit.ImpactNormal).GetSafeNormal();
        const FVector3f SurfacePoint = ToCore(Hit.ImpactPoint - ShotStart) / CmPerM + CoreUp * GolfPhysics::Radius;
        const float PlaneHeight = FVector3f::DotProduct(SurfacePoint, Normal) + GolfPhysics::Radius;
        // The wind reads the height above the launch ground, as the step itself does.
        const ShotStep::FContactFlight Contact = ShotStep::FlyToContact(State, ShotParams, Dt, Normal, PlaneHeight, CoreUp);
        Velocity = Contact.State.Velocity;
        Omega = Contact.State.Spin;
        Hit.Time = Contact.TimeS / Dt;
        SimLocation = ShotStart + ToUnreal(Contact.State.Position - CoreUp * GolfPhysics::Radius) * CmPerM;
    }
    HandleCollision(bHit ? &Hit : nullptr, bWasOnGround, PreviousVelocity);
    if (bHoled)
    {
        return false;
    }

    // Match the headless contact step: move with the rebound for the unspent portion
    // of this timestep. Dropping it changes the next bounce speed enough to cross
    // the restitution cutoff, creating several yards of rollout disagreement.
    if (bHit && !bWasOnGround && !Hit.bStartPenetrating && IsGroundNormal(ToCore(Hit.ImpactNormal)) && !IsObstacle(Hit.GetComponent()))
    {
        const bool bSupportedAfterImpact = bOnGround;
        const FVector3f ReboundVelocity = Velocity;
        FHitResult RemainderHit;
        const bool bRemainderHit = MoveWithSweep(ToUnreal(Velocity * Dt * (1.0f - Hit.Time)) * CmPerM, RemainderHit);
        HandleCollision(bRemainderHit ? &RemainderHit : nullptr, bSupportedAfterImpact, ReboundVelocity);
        if (bHoled)
        {
            return false;
        }
    }
    TryRecoverToGround();

    ++ShotStep;
    if (!bOnGround)
    {
        ApplyCanopy(Dt);
    }

    UpdateCupHeight();
    Result.ApexM = FMath::Max(Result.ApexM, static_cast<float>(SimLocation.Z - ShotStart.Z) / CmPerM);
    TracePhysics(DiagnosticTrace, Result.ElapsedS, Dt, bOnGround ? TEXT("Ground") : TEXT("Flight"),
        ToCoreSurface(CurrentSurface), {CorePosition(), Velocity, Omega}, FloorNormal);
    // Before the rest check: a ball that stops over the cup drops, it never rests there.
    if (BallState == EOpenFairwayBallState::Rollout && bOnGround && TryCaptureInCup(SimLocation, false))
    {
        return false;
    }
    if (BallState != EOpenFairwayBallState::Rest && GolfPhysics::CanRest(FBallState{FVector3f::ZeroVector, Velocity, Omega}, bOnGround, FloorNormal, SurfaceSettings, ShotParams.Rollout))
    {
        EnterRest();
    }
    return true;
}

void AOpenFairwayBall::ApplyCanopy(float Dt)
{
    // Canopy only overlaps the contact channel, so the move sweep and ground probes never see it.
    CanopyOverlaps.Reset();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OpenFairwayBallCanopy), false, this);
    GetWorld()->OverlapMultiByChannel(CanopyOverlaps, SimLocation, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(GolfPhysics::Radius * CmPerM), Params);
    for (const FOverlapResult& Overlap : CanopyOverlaps)
    {
        if (IsCanopy(Overlap.GetComponent()))
        {
            const float PathM = Velocity.Size() * Dt;
            Omega *= CanopySpeedFactor(PathM);
            Velocity = ResolveCanopyDrag(Velocity, PathM, HashCombineFast(GetTypeHash(Overlap.ItemIndex), ShotStep));
            return;
        }
    }
}

void AOpenFairwayBall::HandleCollision(const FHitResult* Hit, bool bWasOnGround, const FVector3f& PreviousVelocity)
{
    if (Hit)
    {
        const FVector3f Normal = SafeNormal(ToCore(Hit->bStartPenetrating ? Hit->Normal : Hit->ImpactNormal));
        if (IsObstacle(Hit->GetComponent()))
        {
            // Trees and other upright obstacles deflect the ball; it never lands or rests on them.
            FVector Axis = Hit->GetComponent()->GetComponentLocation();
            FTransform Instance;
            if (const auto* Instances = Cast<UInstancedStaticMeshComponent>(Hit->GetComponent());
                Instances && Instances->GetInstanceTransform(Hit->Item, Instance, true))
            {
                Axis = Instance.GetLocation();
            }
            bOnGround = false;
            FloorNormal = CoreUp;
            Velocity = ResolveObstacleHit(Velocity, Normal, ToCore(SimLocation - Axis));
        }
        else if (IsGroundNormal(Normal))
        {
            FloorNormal = Normal;
            SetSurface(ResolveSurface(Hit->GetComponent(), Hit->PhysMaterial.Get()));
            const bool bLandedFromFlight = BallState == EOpenFairwayBallState::Flight;
            const bool bIsLanding = bLandedFromFlight || (!bWasOnGround && FVector3f::DotProduct(Velocity, Normal) < 0.0f) ||
                                    FVector3f::DotProduct(PreviousVelocity, Normal) < -0.5f;
            if (bIsLanding)
            {
                if (bLandedFromFlight)
                {
                    RecordLanding(Normal);
                }
                // Descending into the cup, from flight or a bounce: a dunk instead of a bounce.
                if (FVector3f::DotProduct(Velocity, Normal) < 0.0f && TryCaptureInCup(Hit->ImpactPoint, true))
                {
                    if (bLandedFromFlight)
                    {
                        BroadcastLanded();
                    }
                    return;
                }

                TracePhysics(DiagnosticTrace, Result.ElapsedS, GolfPhysics::SimulationDt, TEXT("ImpactIn"),
                    ToCoreSurface(CurrentSurface), {CorePosition(), Velocity, Omega}, Normal);
                const ShotStep::FImpactOutcome Impact = ShotStep::Impact(Velocity, Omega, Normal, SurfaceSettings, ShotParams, bLandedFromFlight);
                if (FVector3f::DotProduct(Velocity, Normal) < 0.0f && bLandedFromFlight)
                {
                    BallState = EOpenFairwayBallState::Rollout;
                }
                Velocity = Impact.Velocity;
                Omega = Impact.Spin;
                TracePhysics(DiagnosticTrace, Result.ElapsedS, GolfPhysics::SimulationDt, TEXT("ImpactOut"),
                    ToCoreSurface(CurrentSurface), {CorePosition(), Velocity, Omega}, Normal);

                if (!Impact.bRebounds && BallState == EOpenFairwayBallState::Rollout)
                {
                    bOnGround = true;
                    Velocity = ShotStep::Settle(Velocity, Normal);
                }
                else
                {
                    bOnGround = false;
                }

                if (bLandedFromFlight && BallState == EOpenFairwayBallState::Rollout)
                {
                    UpdateResult();
                    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d landed on %s: carry %.1f yd, %.1f m/s at %.1f deg, hang time %.2f s."), Result.ShotNumber,
                        SurfaceCatalog::GetName(ToCoreSurface(CurrentSurface)), Result.CarryM * YardsPerMeter, Result.LandingSpeedMps, Result.LandingAngleDeg,
                        Result.HangTimeS);
                    BroadcastLanded();
                }
            }
            else
            {
                bOnGround = true;
                Velocity = RemoveVelocityAlongNormal(Velocity, Normal, false);
            }
        }
        else
        {
            // Wall: damped reflection.
            bOnGround = false;
            FloorNormal = CoreUp;
            Velocity = Obstacles::ResolveWallHit(Velocity, Normal);
        }
        return;
    }

    // No contact: stay grounded only while ground is still directly beneath the ball.
    FGroundHit Ground;
    if (BallState != EOpenFairwayBallState::Flight && bWasOnGround && ProbeGround(Ground, GroundProbeUpM, GolfPhysics::Radius + GroundProbeDistanceM))
    {
        bOnGround = true;
        FloorNormal = Ground.Normal;
        SetSurface(ResolveSurface(Ground.Component.Get(), Ground.PhysMaterial.Get()));
    }
    else
    {
        bOnGround = false;
        FloorNormal = CoreUp;
    }
}

void AOpenFairwayBall::RecordLanding(const FVector3f& Normal)
{
    // The impact spin keeps its "bite" on roll-out friction as the spin decays.
    RolloutImpactSpinRpm = GolfPhysics::RolloutSpinRpm(Omega, Normal);
    Result.bLanded = true;
    Result.LandingSpeedMps = Velocity.Size();
    Result.LandingAngleDeg = ShotStep::ImpactAngleDeg(Velocity, Normal);
    Result.HangTimeS = Result.ElapsedS;
    Result.LandingSurface = CurrentSurface;
    Result.CarryM = FMath::Max(FVector3f::DotProduct(CoreOffset(), ShotDirection), 0.0f);
}
