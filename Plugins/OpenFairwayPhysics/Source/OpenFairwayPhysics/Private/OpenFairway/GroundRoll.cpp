// SPDX-License-Identifier: MIT
// Roll-out originally ported from ogolf-physics core/GolfPhysics.cs; the ground step now solves resistance and contact
// together, and a ball struck along the ground keeps its own share of the grass drag.
#include "OpenFairway/GolfPhysics.h"
#include "OpenFairway/WindProfile.h"

#include "CoreMath.h"

namespace OpenFairway
{
    namespace GroundRollDetail
    {
        using namespace GolfPhysics;

        /** Below this rolling spin (rad/s) a ball under the rest speed stops dead. */
        constexpr float StopSpinRadPerS = 8.0f;

        FVector3f DampenSpin(const FVector3f& Spin, float Viscosity, float Dt)
        {
            const float DecayRate = 8.0f * UE_PI * Viscosity * Radius * Radius * Radius / Inertia;
            return Spin * FMath::Exp(-DecayRate * Dt);
        }

        FVector3f ResistContactSpin(const FVector3f& Spin, const FVector3f& Normal, float RollingFriction, float NormalLoad, float Dt)
        {
            const float NormalSpin = Dot(Spin, Normal);
            const float Reduction = RollingFriction * NormalLoad * Radius / Inertia * Dt;
            const float Remaining = FMath::Max(0.0f, FMath::Abs(NormalSpin) - Reduction);
            return Spin + (FMath::CopySign(Remaining, NormalSpin) - NormalSpin) * Normal;
        }
    } // namespace GroundRollDetail

    FBallState GolfPhysics::StepGround(const FBallState& State, const FVector3f& InNormal, const FSurfaceSettings& Surface,
        const FRolloutProfile& Profile, float ImpactSpinRpm, float Dt, const FFlightEnvironment& Environment)
    {
        using namespace GroundRollDetail;
        const FVector3f Normal = ValidNormal(InNormal);
        const FVector3f Velocity = AlongPlane(State.Velocity, Normal);
        const FVector3f ContactVelocity = Velocity + Cross(State.Spin, -Radius * Normal);
        const FVector3f Slip = AlongPlane(ContactVelocity, Normal);
        const float NormalLoad = Mass * Gravity * FMath::Max(0.0f, Normal.Y);
        const FVector3f SlopeForce = FVector3f(0.0f, -Mass * Gravity, 0.0f) + Normal * NormalLoad;
        const float SlopeLength = SlopeForce.Size();
        const float StaticCapacity = Surface.RollingFriction * Profile.SlopeResistanceRatio * NormalLoad;
        // Spin about the normal pivots the ball in place; only contact-plane spin can still roll it.
        const FVector3f RollingSpin = AlongPlane(State.Spin, Normal);
        if (Velocity.SizeSquared() < RestSpeedMps * RestSpeedMps && RollingSpin.SizeSquared() < StopSpinRadPerS * StopSpinRadPerS &&
            SlopeLength <= StaticCapacity)
        {
            FBallState Stopped = State;
            Stopped.Velocity = FVector3f::ZeroVector;
            Stopped.Spin = FVector3f::ZeroVector;
            return Stopped;
        }

        if (Velocity.SizeSquared() < 0.0001f && Slip.SizeSquared() < 0.0001f && SlopeLength <= StaticCapacity)
        {
            FBallState Held = State;
            Held.Velocity = FVector3f::ZeroVector;
            Held.Spin = ResistContactSpin(DampenSpin(State.Spin, Surface.GrassViscosity, Dt), Normal, Surface.RollingFriction, NormalLoad, Dt);
            return Held;
        }

        const float Multiplier = SpinFrictionMultiplier(FMath::Max(ImpactSpinRpm, RollingSpin.Size() / RadPerRpm), Velocity.Size(), Profile);
        // Passive resistance first, then the contact: linear (viscous) and speed-squared (form) grass drag, integrated exactly.
        const float Speed = Velocity.Size();
        const float K = 6.0f * UE_PI * Radius * Surface.GrassViscosity / Mass;
        const float Decay = FMath::Exp(-K * Dt);
        const float DragIntegral = K > 1e-8f ? (1.0f - Decay) / K : Dt;
        FVector3f NextVelocity = Velocity * (Decay / (1.0f + Surface.GrassFormDrag * Speed * DragIntegral));
        // Air drag does not disappear when the ball touches grass; aerodynamic and turf drag stay separate. A ball on the
        // ground is one radius above its lie wherever that lie is, so it meets the wind at that height, and its drag reads
        // only its own state and the ground drag scale.
        const FVector3f Wind = WindProfile::At(Environment, Radius);
        const FVector3f TangentWind = AlongPlane(Wind, Normal);
        const FVector3f AirVelocity = NextVelocity - TangentWind;
        const float AirSpeed = AirVelocity.Size();
        if (AirSpeed >= FlightAerodynamics::MinAerodynamicSpeed)
        {
            const float Reynolds = Environment.AirDensity * AirSpeed * Radius * 2.0f / Environment.AirViscosity;
            const float SpinRatio = State.Spin.Size() * Radius / AirSpeed;
            const float DragCoefficient = FlightAerodynamics::GroundDragCoefficient(Reynolds, SpinRatio) * Environment.GroundDragScale;
            const float AirDragRate = 0.5f * DragCoefficient * Environment.AirDensity * CrossSection / Mass;
            NextVelocity = TangentWind + AirVelocity / (1.0f + AirDragRate * AirSpeed * Dt);
        }
        const float RollingMultiplier = 1.0f + (Multiplier - 1.0f) * Surface.RollingSpinScale;
        const float RollingDelta = Surface.RollingFriction * RollingMultiplier * NormalLoad / Mass * Dt;
        const float DraggedSpeed = NextVelocity.Size();
        if (DraggedSpeed > 0.0f)
        {
            NextVelocity *= FMath::Max(0.0f, DraggedSpeed - RollingDelta) / DraggedSpeed;
        }
        NextVelocity += SlopeForce / Mass * Dt;
        FVector3f NextSpin = ResistContactSpin(DampenSpin(State.Spin, Surface.GrassViscosity, Dt), Normal, Surface.RollingFriction, NormalLoad, Dt);
        // The contact: a Coulomb-capped impulse toward rolling without slipping (2/7 of the slip for a solid sphere).
        const FVector3f Arm = -Radius * Normal;
        const FVector3f NextSlip = AlongPlane(NextVelocity + Cross(NextSpin, Arm), Normal);
        FVector3f Impulse = -2.0f * Mass / 7.0f * NextSlip;
        const float SlidingMultiplier = 1.0f + (Multiplier - 1.0f) * Surface.SlidingSpinScale;
        const float Capacity = Surface.KineticFriction * SlidingMultiplier * NormalLoad * Dt;
        const float ImpulseSize = Impulse.Size();
        if (ImpulseSize > Capacity && ImpulseSize > 0.0f)
        {
            Impulse *= Capacity / ImpulseSize;
        }
        NextVelocity += Impulse / Mass;
        NextSpin += Cross(Arm, Impulse) / Inertia;
        return FBallState{State.Position + NextVelocity * Dt, NextVelocity, NextSpin};
    }

    float GolfPhysics::SpinFrictionMultiplier(float SpinRpm, float Speed, const FRolloutProfile& Profile)
    {
        const float SpeedScale = Speed < Profile.ChipSpeedThreshold
                                     ? Lerp(Profile.ChipVelocityScaleMin, Profile.ChipVelocityScaleMax, Speed / Profile.ChipSpeedThreshold)
                                 : Speed < Profile.PitchSpeedThreshold
                                     ? Lerp(Profile.ChipVelocityScaleMax, 1.0f, (Speed - Profile.ChipSpeedThreshold) / (Profile.PitchSpeedThreshold - Profile.ChipSpeedThreshold))
                                     : 1.0f;
        const float SpinScale = SpinRpm < Profile.LowSpinThreshold
                                    ? Lerp(1.0f, Profile.LowSpinMultiplierMax, SpinRpm / Profile.LowSpinThreshold)
                                : SpinRpm < Profile.MidSpinThreshold
                                    ? Lerp(Profile.LowSpinMultiplierMax, Profile.MidSpinMultiplierMax,
                                          (SpinRpm - Profile.LowSpinThreshold) / (Profile.MidSpinThreshold - Profile.LowSpinThreshold))
                                    : Lerp(Profile.MidSpinMultiplierMax, Profile.HighSpinMultiplierMax,
                                          FMath::Min((SpinRpm - Profile.MidSpinThreshold) / Profile.HighSpinRampRange, 1.0f));
        return 1.0f + (SpinScale - 1.0f) * SpeedScale;
    }

    bool GolfPhysics::CanRest(const FBallState& State, bool bSupported, const FVector3f& InNormal, const FSurfaceSettings& Surface,
        const FRolloutProfile& Profile)
    {
        if (!bSupported || State.Velocity.SizeSquared() >= RestSpeedMps * RestSpeedMps)
        {
            return false;
        }

        const FVector3f Normal = ValidNormal(InNormal);
        // A ball pivoting about the normal is not rolling anywhere; it rests once its contact-plane spin is gone.
        if (AlongPlane(State.Spin, Normal).SizeSquared() >= 0.5f * 0.5f)
        {
            return false;
        }
        const FVector3f SlopeGravity(0.0f, -Mass * Gravity, 0.0f);
        const FVector3f TangentGravity = SlopeGravity - Normal * Dot(SlopeGravity, Normal);
        return TangentGravity.Size() <= Surface.RollingFriction * Profile.SlopeResistanceRatio * Mass * Gravity * FMath::Max(0.0f, Normal.Y);
    }

    float GolfPhysics::RolloutSpinRpm(const FVector3f& Spin, const FVector3f& InNormal)
    {
        return AlongPlane(Spin, ValidNormal(InNormal)).Size() / RadPerRpm;
    }

    float GolfPhysics::StruckAlongGroundWeight(float TangentSpeed)
    {
        return RampUp(TangentSpeed, PuttSpeedRampStartMps, PuttSpeedRampEndMps);
    }

    FSurfaceSettings GolfPhysics::StruckAlongGround(const FSurfaceSettings& Surface, float Weight)
    {
        const float W = FMath::Clamp(Weight, 0.0f, 1.0f);
        if (W <= 0.0f)
        {
            return Surface;
        }
        FSurfaceSettings Struck = Surface;
        Struck.GrassViscosity *= Lerp(1.0f, Surface.StruckViscosityScale, W);
        Struck.GrassFormDrag *= Lerp(1.0f, Surface.StruckFormDragScale, W);
        return Struck;
    }
} // namespace OpenFairway
