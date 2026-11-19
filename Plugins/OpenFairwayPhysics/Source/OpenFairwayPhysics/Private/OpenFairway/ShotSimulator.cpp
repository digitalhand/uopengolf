// SPDX-License-Identifier: MIT
// Launch setup and the headless shot loop, originally ported from ogolf-physics (ShotSetup.cs and
// PhysicsAdapter.SimulateShotInternal).
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairway/ShotStep.h"
#include "OpenFairway/WindProfile.h"

namespace OpenFairway
{
    namespace
    {
        constexpr float DegToRad = UE_PI / 180.0f;
        constexpr float RadToDeg = 180.0f / UE_PI;
        const FVector3f UpAxis(0.0f, 1.0f, 0.0f);
        /** The axis the launch angle pitches the ball's velocity about: a positive angle turns +X toward +Y. */
        const FVector3f LaunchPitchAxis(0.0f, 0.0f, -1.0f);

        bool IsFiniteOptional(const TOptional<float>& Value)
        {
            return !Value.IsSet() || FMath::IsFinite(Value.GetValue());
        }
    } // namespace

    bool FLaunchData::IsValid() const
    {
        return FMath::IsFinite(SpeedMph) && SpeedMph >= 0.0f && FMath::IsFinite(VerticalLaunchDeg) && VerticalLaunchDeg >= -90.0f &&
               VerticalLaunchDeg <= 90.0f && FMath::IsFinite(HorizontalLaunchDeg) && IsFiniteOptional(BackSpinRpm) && IsFiniteOptional(SideSpinRpm) &&
               IsFiniteOptional(TotalSpinRpm) && IsFiniteOptional(SpinAxisDeg) && ShotSetup::ParseSpin(*this).TotalSpinRpm >= 0.0f;
    }

    FShotSpin ShotSetup::ParseSpin(const FLaunchData& Launch)
    {
        const bool bHasBack = Launch.BackSpinRpm.IsSet();
        const bool bHasSide = Launch.SideSpinRpm.IsSet();
        const bool bHasTotal = Launch.TotalSpinRpm.IsSet();
        const bool bHasAxis = Launch.SpinAxisDeg.IsSet();

        FShotSpin Spin;
        Spin.BackSpinRpm = Launch.BackSpinRpm.Get(0.0f);
        Spin.SideSpinRpm = Launch.SideSpinRpm.Get(0.0f);
        Spin.TotalSpinRpm = Launch.TotalSpinRpm.Get(0.0f);
        Spin.SpinAxisDeg = Launch.SpinAxisDeg.Get(0.0f);

        if (Spin.TotalSpinRpm == 0.0f && (bHasBack || bHasSide))
        {
            Spin.TotalSpinRpm = FMath::Sqrt(Spin.BackSpinRpm * Spin.BackSpinRpm + Spin.SideSpinRpm * Spin.SideSpinRpm);
        }
        if (!bHasAxis && (bHasBack || bHasSide))
        {
            Spin.SpinAxisDeg = FMath::Atan2(Spin.SideSpinRpm, Spin.BackSpinRpm) * RadToDeg;
        }
        if (bHasTotal && bHasAxis)
        {
            if (!bHasBack)
            {
                Spin.BackSpinRpm = Spin.TotalSpinRpm * FMath::Cos(Spin.SpinAxisDeg * DegToRad);
            }
            if (!bHasSide)
            {
                Spin.SideSpinRpm = Spin.TotalSpinRpm * FMath::Sin(Spin.SpinAxisDeg * DegToRad);
            }
        }
        // Launch monitors measure back and side spin; total spin is derived, so components win.
        if (bHasBack && bHasSide && bHasTotal)
        {
            const float Computed = FMath::Sqrt(Spin.BackSpinRpm * Spin.BackSpinRpm + Spin.SideSpinRpm * Spin.SideSpinRpm);
            if (FMath::Abs(Computed - Spin.TotalSpinRpm) > 1.0f)
            {
                Spin.TotalSpinRpm = Computed;
                Spin.SpinAxisDeg = FMath::Atan2(Spin.SideSpinRpm, Spin.BackSpinRpm) * RadToDeg;
            }
        }
        return Spin;
    }

    FVector3f ShotSetup::Rotate(const FVector3f& V, const FVector3f& Axis, float Angle)
    {
        const float C = FMath::Cos(Angle);
        const float S = FMath::Sin(Angle);
        const float T = 1.0f - C;
        const float R00 = Axis.X * Axis.X + C * (1.0f - Axis.X * Axis.X);
        const float R11 = Axis.Y * Axis.Y + C * (1.0f - Axis.Y * Axis.Y);
        const float R22 = Axis.Z * Axis.Z + C * (1.0f - Axis.Z * Axis.Z);
        const float R01 = Axis.X * Axis.Y * T - Axis.Z * S;
        const float R10 = Axis.X * Axis.Y * T + Axis.Z * S;
        const float R02 = Axis.X * Axis.Z * T + Axis.Y * S;
        const float R20 = Axis.X * Axis.Z * T - Axis.Y * S;
        const float R12 = Axis.Y * Axis.Z * T - Axis.X * S;
        const float R21 = Axis.Y * Axis.Z * T + Axis.X * S;
        return FVector3f(R00 * V.X + R01 * V.Y + R02 * V.Z, R10 * V.X + R11 * V.Y + R12 * V.Z, R20 * V.X + R21 * V.Y + R22 * V.Z);
    }

    FLaunchVectors ShotSetup::BuildLaunchVectors(float SpeedMph, float VerticalLaunchDeg, float HorizontalLaunchDeg, float BackSpinRpm, float SideSpinRpm)
    {
        // The spin vector is built in shot-local space and rotated with the launch frame so
        // backspin and sidespin stay aligned with the shot plane.
        FLaunchVectors Vectors;
        const float SpeedMps = SpeedMph * MpsPerMph;
        Vectors.Velocity = Rotate(Rotate(FVector3f(SpeedMps, 0.0f, 0.0f), LaunchPitchAxis, -VerticalLaunchDeg * DegToRad), UpAxis, -HorizontalLaunchDeg * DegToRad);
        const FVector3f Omega = Rotate(FVector3f(0.0f, -SideSpinRpm * GolfPhysics::RadPerRpm, BackSpinRpm * GolfPhysics::RadPerRpm), UpAxis,
            -HorizontalLaunchDeg * DegToRad);
        Vectors.Omega = GolfPhysics::LaunchSpin(Vectors.Velocity, Omega, UpAxis, FBounceProfile());
        const FVector3f Flat(Vectors.Velocity.X, 0.0f, Vectors.Velocity.Z);
        Vectors.ShotDirection = Flat.Size() > 0.001f ? Flat / Flat.Size() : FVector3f(1.0f, 0.0f, 0.0f);
        return Vectors;
    }

    FShotSimulationResult ShotSimulator::Simulate(const FLaunchData& Launch, ESurface SurfaceType, const FBallPhysicsProfile& Profile,
        const FAirConditions& Air, float MaxTimeSeconds, const FVector3f& GroundNormal, const FShotSimulationOptions& Options)
    {
        FShotSimulationResult Result;
        if (!Launch.IsValid() || !FMath::IsFinite(Options.Dt) || Options.Dt <= 0.0f || Options.Dt > 0.1f ||
            !FMath::IsFinite(MaxTimeSeconds) || MaxTimeSeconds <= 0 || MaxTimeSeconds / Options.Dt > MAX_int32)
        {
            return Result;
        }

        Result.Spin = ShotSetup::ParseSpin(Launch);
        const FResolvedShotParams Params =
            FResolvedShotParams::Resolve(Profile, Air, Launch.VerticalLaunchDeg, Result.Spin.TotalSpinRpm, Result.Spin.BackSpinRpm);
        const FSurfaceSettings Surface = Options.SurfaceOverride.Get(Params.ResolveSurface(SurfaceType, Profile));
        const FLaunchVectors Vectors =
            ShotSetup::BuildLaunchVectors(Launch.SpeedMph, Launch.VerticalLaunchDeg, Launch.HorizontalLaunchDeg, Result.Spin.BackSpinRpm, Result.Spin.SideSpinRpm);
        Result.bValid = true;
        Result.LandingBucket = Params.LandingBucket;

        const float Dt = Options.Dt;
        constexpr float Radius = GolfPhysics::Radius;
        const FVector3f Normal = GroundNormal.SizeSquared() > 0.000001f ? GroundNormal.GetSafeNormal() : UpAxis;
        FVector3f Velocity = Vectors.Velocity;
        FVector3f Omega = Vectors.Omega;
        FVector3f Position = Normal * Radius;
        bool bFlight = true;
        bool bOnGround = false;
        bool bCarryRecorded = false;
        float RolloutImpactSpinRpm = 0.0f;
        float LandingSpinRpm = 0.0f;
        // Share of the struck-along-the-ground grass drag this shot rolls with (0 unless it starts on the ground).
        float StruckWeight = 0.0f;
        Result.ApexM = Position.Y;

        // A supported launch into (or along) the plane contacts at t=0. Do not first
        // add a timestep of gravity/flight and manufacture a landing velocity or carry.
        if (FVector3f::DotProduct(Velocity, Normal) <= 0.0f)
        {
            const FVector3f Tangent = ShotStep::Settle(Velocity, Normal);
            Result.LandingSpeedMps = Velocity.Size();
            Result.LandingSpinRpm = Omega.Size() / GolfPhysics::RadPerRpm;
            Result.LandingAngleDeg = ShotStep::ImpactAngleDeg(Velocity, Normal);
            Result.FirstImpactTangentInMps = Tangent.Size();
            RolloutImpactSpinRpm = GolfPhysics::RolloutSpinRpm(Omega, Normal);
            StruckWeight = ShotStep::StruckWeight(Velocity, Normal);
            TracePhysics(Options.Trace, 0, Dt, TEXT("ImpactIn"), SurfaceType, {Position, Velocity, Omega}, Normal);
            const ShotStep::FImpactOutcome Contact = ShotStep::Impact(Velocity, Omega, Normal, Surface, Params, true);
            Velocity = Contact.Velocity;
            Omega = Contact.Spin;
            bFlight = false;
            bCarryRecorded = true;
            bOnGround = !Contact.bRebounds;
            Result.BounceCount = bOnGround ? 0 : 1;
            if (bOnGround) Velocity = ShotStep::Settle(Velocity, Normal);
            Result.FirstImpactTangentOutMps = Tangent.IsNearlyZero() ? 0 : FVector3f::DotProduct(Velocity, Tangent.GetSafeNormal());
            Result.bFirstImpactSpinback = Result.FirstImpactTangentOutMps < 0;
            TracePhysics(Options.Trace, 0, Dt, TEXT("ImpactOut"), SurfaceType, {Position, Velocity, Omega}, Normal);
        }

        // A full shot struck into or along the ground keeps its struck grass drag for the whole roll.
        const FSurfaceSettings GroundSurface = GolfPhysics::StruckAlongGround(Surface, StruckWeight);
        const int32 Steps = static_cast<int32>(MaxTimeSeconds / Dt);
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            if (!bOnGround)
            {
                // The ball centre's height above the ground plane.
                const FVector3f Wind = WindProfile::At(Params.Environment, FVector3f::DotProduct(Position, Normal));
                const FFlightAerodynamicsSample Sample =
                    FlightAerodynamics::Sample(Velocity - Wind, Omega, Params.Environment.AirDensity, Params.Environment.AirViscosity, Params.Flight);
                if (bFlight)
                {
                    Result.MinFlightRe = FMath::Min(Result.MinFlightRe, Sample.Reynolds);
                    Result.MaxFlightRe = FMath::Max(Result.MaxFlightRe, Sample.Reynolds);
                    Result.MinFlightSpinRatio = FMath::Min(Result.MinFlightSpinRatio, Sample.SpinRatio);
                    Result.MaxFlightSpinRatio = FMath::Max(Result.MaxFlightSpinRatio, Sample.SpinRatio);
                    if (Sample.bOutsideMeasuredCoverage) Result.OutsideMeasuredCoverageS += Dt;
                }
                const FBallState PreviousState{Position, Velocity, Omega};
                const FBallState Next = GolfPhysics::StepFlight(PreviousState, Params.Environment, Params.Flight, Dt, Normal);
                Velocity = Next.Velocity;
                Omega = Next.Spin;
                Position = Next.Position;
                Result.ApexM = FMath::Max(Result.ApexM, Position.Y);

                const float ContactDistance = FVector3f::DotProduct(Position, Normal) - Radius;
                if (ContactDistance <= 0.0f && FVector3f::DotProduct(Velocity, Normal) < -0.01f)
                {
                    // Back up to the moment of contact, bounce, then use the rest of the step. The contact time comes from
                    // the flight itself: a very low launch can complete its entire flight inside one step, and a chord
                    // from the ground would invent zero carry.
                    const ShotStep::FContactFlight Contact = ShotStep::FlyToContact(PreviousState, Params, Dt, Normal, Radius, Normal);
                    const float ImpactFraction = Contact.TimeS / Dt;
                    Position = Contact.State.Position;
                    Velocity = Contact.State.Velocity;
                    Omega = Contact.State.Spin;
                    Position += Normal * (Radius - FVector3f::DotProduct(Position, Normal));
                    const float PreImpactSpeed = Velocity.Size();
                    const float PreImpactAngleDeg = ShotStep::ImpactAngleDeg(Velocity, Normal);
                    const FVector3f PreImpactTangent = ShotStep::Settle(Velocity, Normal);
                    if (!bCarryRecorded)
                    {
                        LandingSpinRpm = Omega.Size() / GolfPhysics::RadPerRpm;
                        RolloutImpactSpinRpm = GolfPhysics::RolloutSpinRpm(Omega, Normal);
                    }

                    TracePhysics(Options.Trace, (Step + ImpactFraction) * Dt, Dt, TEXT("ImpactIn"), SurfaceType, {Position, Velocity, Omega}, Normal);
                    // The ball is descending, so this impact ends the flight (the first one bites; later ones are bounces).
                    const ShotStep::FImpactOutcome Impact = ShotStep::Impact(Velocity, Omega, Normal, Surface, Params, bFlight);
                    bFlight = false;
                    Velocity = Impact.Velocity;
                    Omega = Impact.Spin;
                    bOnGround = !Impact.bRebounds;
                    Result.BounceCount += bOnGround ? 0 : 1;
                    TracePhysics(Options.Trace, (Step + ImpactFraction) * Dt, Dt, TEXT("ImpactOut"), SurfaceType, {Position, Velocity, Omega}, Normal);
                    if (bOnGround)
                    {
                        Velocity = ShotStep::Settle(Velocity, Normal);
                    }

                    if (!bCarryRecorded)
                    {
                        const FVector3f PostImpactTangent = ShotStep::Settle(Velocity, Normal);
                        const float PreTangentSpeed = PreImpactTangent.Size();
                        const float PostTangentSpeed = PostImpactTangent.Size();
                        Result.FirstImpactTangentInMps = PreTangentSpeed;
                        Result.FirstImpactTangentOutMps = PostTangentSpeed;
                        if (PreTangentSpeed > 0.01f && PostTangentSpeed > 0.01f)
                        {
                            Result.bFirstImpactSpinback =
                                FVector3f::DotProduct(PreImpactTangent / PreTangentSpeed, PostImpactTangent / PostTangentSpeed) < -0.001f;
                            if (Result.bFirstImpactSpinback)
                            {
                                Result.FirstImpactTangentOutMps = -PostTangentSpeed;
                            }
                        }
                        Result.LandingSpeedMps = PreImpactSpeed;
                        Result.LandingSpinRpm = LandingSpinRpm;
                        Result.LandingAngleDeg = PreImpactAngleDeg;
                        Result.CarryM = FMath::Max(FVector3f::DotProduct(Position, Vectors.ShotDirection), 0.0f);
                        Result.HangTimeS = (static_cast<float>(Step) + ImpactFraction) * Dt;
                        bCarryRecorded = true;
                    }
                    Position += Velocity * (1.0f - ImpactFraction) * Dt;
                    if (bOnGround) Result.GroundDistanceM += Velocity.Size() * (1.0f - ImpactFraction) * Dt;
                }
            }
            else
            {
                const FBallState Next = GolfPhysics::StepGround(FBallState{Position, Velocity, Omega}, Normal, GroundSurface, Params.Rollout,
                    RolloutImpactSpinRpm, Dt, Params.Environment);
                Velocity = Next.Velocity;
                Omega = Next.Spin;
                Position += Velocity * Dt;
                Result.GroundDistanceM += Velocity.Size() * Dt;
                const float ContactDistance = FVector3f::DotProduct(Position, Normal) - Radius;
                if (ContactDistance < 0.0f)
                {
                    Position -= Normal * ContactDistance;
                }
            }

            TracePhysics(Options.Trace, (Step + 1) * Dt, Dt, bOnGround ? TEXT("Ground") : TEXT("Flight"),
                SurfaceType, {Position, Velocity, Omega}, Normal);
            if (GolfPhysics::CanRest(FBallState{Position, Velocity, Omega}, bOnGround, Normal, Surface, Params.Rollout))
            {
                Result.bAtRest = true;
                TracePhysics(Options.Trace, (Step + 1) * Dt, Dt, TEXT("Rest"), SurfaceType, {Position, Velocity, Omega}, Normal);
                break;
            }
        }

        Result.TotalM = FMath::Max(FVector3f::DotProduct(Position, Vectors.ShotDirection), 0.0f);
        // Offline from the target line (+X); OpenFairway +Z is right.
        Result.LateralM = Position.Z;
        if (!bCarryRecorded)
        {
            Result.CarryM = Result.TotalM;
        }
        return Result;
    }
} // namespace OpenFairway
