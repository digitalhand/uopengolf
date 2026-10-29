// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/ShotSetup.cs and
// PhysicsAdapter.SimulateShotInternal at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairway/WindProfile.h"

namespace OpenFairway
{
    namespace
    {
        constexpr float DegToRad = 3.14159265f / 180.0f;
        constexpr float RadToDeg = 180.0f / 3.14159265f;
        const FVector3f UpAxis(0.0f, 1.0f, 0.0f);
        // Godot's Vector3.Forward, the axis the reference rotates the launch angle about.
        const FVector3f GodotForward(0.0f, 0.0f, -1.0f);

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
        Vectors.Velocity = Rotate(Rotate(FVector3f(SpeedMps, 0.0f, 0.0f), GodotForward, -VerticalLaunchDeg * DegToRad), UpAxis, -HorizontalLaunchDeg * DegToRad);
        const FVector3f Omega = Rotate(FVector3f(0.0f, -SideSpinRpm * GolfPhysics::RadPerRpm, BackSpinRpm * GolfPhysics::RadPerRpm), UpAxis,
            -HorizontalLaunchDeg * DegToRad);
        Vectors.Omega = GolfPhysics::LaunchSpin(Vectors.Velocity, Omega, UpAxis, FBounceProfile());
        const FVector3f Flat(Vectors.Velocity.X, 0.0f, Vectors.Velocity.Z);
        Vectors.ShotDirection = Flat.Size() > 0.001f ? Flat / Flat.Size() : FVector3f(1.0f, 0.0f, 0.0f);
        return Vectors;
    }

    FShotSimulationResult ShotSimulator::Simulate(const FLaunchData& Launch, ESurface SurfaceType, const FBallPhysicsProfile& Profile,
        const FAirConditions& Air, float MaxTimeSeconds)
    {
        FShotSimulationResult Result;
        if (!Launch.IsValid())
        {
            return Result;
        }

        Result.Spin = ShotSetup::ParseSpin(Launch);
        const FResolvedShotParams Params =
            FResolvedShotParams::Resolve(Profile, Air, Launch.SpeedMph, Launch.VerticalLaunchDeg, Result.Spin.TotalSpinRpm);
        const FSurfaceSettings Surface = Params.ResolveSurface(SurfaceType, Profile);
        const FLaunchVectors Vectors =
            ShotSetup::BuildLaunchVectors(Launch.SpeedMph, Launch.VerticalLaunchDeg, Launch.HorizontalLaunchDeg, Result.Spin.BackSpinRpm, Result.Spin.SideSpinRpm);
        Result.bValid = true;
        Result.LandingBucket = Params.LandingBucket;
        Result.RegimeKey = Params.RegimeKey;
        Result.MatchedRegimeKey = Params.MatchedRegimeKey;

        constexpr float Dt = GolfPhysics::SimulationDt;
        constexpr float Radius = GolfPhysics::Radius;
        const FVector3f Normal = UpAxis;
        FVector3f Velocity = Vectors.Velocity;
        FVector3f Omega = Vectors.Omega;
        FVector3f Position = Normal * Radius;
        bool bFlight = true;
        bool bOnGround = false;
        bool bCarryRecorded = false;
        float RolloutImpactSpinRpm = 0.0f;
        Result.ApexM = Position.Y;

        const int32 Steps = static_cast<int32>(MaxTimeSeconds / Dt);
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            if (!bOnGround)
            {
                // Position.Y is the ball centre's height above the flat ground.
                const FVector3f Wind = WindProfile::At(Params.Environment, Position.Y);
                const FFlightAerodynamicsSample Sample = FlightAerodynamics::Sample(Velocity - Wind, Omega, Params.Environment.AirDensity,
                    Params.Environment.AirViscosity, Params.Environment.DragScale, Params.Environment.LiftScale, Params.Environment.InitialLaunchAngleDeg,
                    Params.Flight);
                const FVector3f Previous = Position;
                const FBallState Next = GolfPhysics::StepFlight(FBallState{Position, Velocity, Omega}, Params.Environment, Sample, Dt);
                Velocity = Next.Velocity;
                Omega = Next.Spin;
                Position += Velocity * Dt;
                Result.ApexM = FMath::Max(Result.ApexM, Position.Y);

                const float ContactDistance = FVector3f::DotProduct(Position, Normal) - Radius;
                if (ContactDistance <= 0.0f && FVector3f::DotProduct(Velocity, Normal) < -0.01f)
                {
                    // Back up to the moment of contact, bounce, then use the rest of the step.
                    const float PreviousDistance = FVector3f::DotProduct(Previous, Normal) - Radius;
                    const float ImpactFraction =
                        PreviousDistance > 0.0f ? FMath::Clamp(PreviousDistance / (PreviousDistance - ContactDistance), 0.0f, 1.0f) : 0.0f;
                    Position = Previous + (Position - Previous) * ImpactFraction;
                    Position += Normal * (Radius - FVector3f::DotProduct(Position, Normal));
                    const float PreImpactSpeed = Velocity.Size();
                    const float PreImpactNormalSpeed = FMath::Abs(FVector3f::DotProduct(Velocity, Normal));
                    const FVector3f PreImpactTangent = Velocity - Normal * FVector3f::DotProduct(Velocity, Normal);
                    if (!bCarryRecorded)
                    {
                        RolloutImpactSpinRpm = Omega.Size() / GolfPhysics::RadPerRpm;
                    }

                    const FBallState Resolved = GolfPhysics::ResolveImpact(FBallState{FVector3f::ZeroVector, Velocity, Omega}, Normal, Surface,
                        Params.Bounce, bFlight, Params.Landing);
                    if (FVector3f::DotProduct(Velocity, Normal) < 0.0f && bFlight)
                    {
                        bFlight = false;
                    }
                    Velocity = Resolved.Velocity;
                    Omega = Resolved.Spin;
                    bOnGround = !bFlight && !GolfPhysics::HasRebound(Velocity, Normal, Surface.SurfaceBounce);
                    if (bOnGround)
                    {
                        Velocity -= Normal * FVector3f::DotProduct(Velocity, Normal);
                    }

                    if (!bCarryRecorded)
                    {
                        Result.LandingSpeedMps = PreImpactSpeed;
                        Result.LandingSpinRpm = RolloutImpactSpinRpm;
                        Result.LandingAngleDeg = FMath::Atan2(PreImpactNormalSpeed, FMath::Max(PreImpactTangent.Size(), 0.0001f)) * RadToDeg;
                        Result.CarryM = FMath::Max(FVector3f::DotProduct(Position, Vectors.ShotDirection), 0.0f);
                        Result.HangTimeS = (static_cast<float>(Step) + ImpactFraction) * Dt;
                        bCarryRecorded = true;
                    }
                    Position += Velocity * (1.0f - ImpactFraction) * Dt;
                }
            }
            else
            {
                const FBallState Next = GolfPhysics::StepGround(FBallState{FVector3f::ZeroVector, Velocity, Omega}, Normal, Surface, Params.Rollout,
                    RolloutImpactSpinRpm, Dt);
                Velocity = Next.Velocity;
                Omega = Next.Spin;
                Position += Velocity * Dt;
                const float ContactDistance = FVector3f::DotProduct(Position, Normal) - Radius;
                if (ContactDistance < 0.0f)
                {
                    Position -= Normal * ContactDistance;
                }
            }

            if (GolfPhysics::CanRest(FBallState{Position, Velocity, Omega}, bOnGround, Normal, Surface, Params.Rollout))
            {
                Result.bAtRest = true;
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
