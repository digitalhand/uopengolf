// SPDX-License-Identifier: MIT
// Impact response originally ported from ogolf-physics core/GolfPhysics.cs and core/SurfaceBounceSettings.cs. The C#
// core throws on invalid input; this port has no exceptions, so an invalid contact normal falls back to up and
// callers validate launch data before simulating.
#include "OpenFairway/GolfPhysics.h"

#include "CoreMath.h"

namespace OpenFairway
{
    namespace ImpactDetail
    {
        using namespace GolfPhysics;

        /** 0 for a ball moving along the ground, 1 from the normal speed it bounces at. */
        float LandingWeight(float NormalSpeed, const FBounceProfile& Profile)
        {
            return RampUp(NormalSpeed, Profile.FlightRetentionRampStartMps, Profile.FlightRetentionRampEndMps);
        }

        /** Tangential speed kept through the first impact; the calibrated loss fades in with normal speed. */
        float FlightRetention(float NormalSpeed, float SpinRpm, const FBounceProfile& Profile)
        {
            const float Landing = Profile.FlightTangentialRetentionBase *
                                  FMath::Clamp(1.0f - SpinRpm / Profile.FlightSpinFactorDivisor, Profile.FlightSpinFactorMin, 1.0f);
            const float Weight = LandingWeight(NormalSpeed, Profile);
            return Weight >= 1.0f ? Landing : Lerp(1.0f, Landing, Weight);
        }

        bool UsesTurfLanding(const FSurfaceSettings& Surface)
        {
            return Surface.LandingAngleLoss > 0.0f || Surface.LandingSpinLoss > 0.0f;
        }

        /**
         * Tangential speed a turf impact keeps after its contact impulse, read only from the ball's state at
         * the impact: exp(-k * vn/vt) for the turf the ball drives into (a steeper landing digs in more; a ball
         * skimming the ground loses almost nothing) and max(0, 1 - min(c * R*backspin, GripLimit) / vt) for
         * backspin gripping it. GripLimit is the Coulomb bound of the contact impulse (friction times the normal
         * impulse, per unit mass): like friction, the grip needs load, so it fades out as the normal speed goes
         * to zero. Nothing here depends on how the ball was launched.
         */
        float TurfRetention(const FBallState& State, const FVector3f& Normal, float IncomingSpeed, float GripLimit, const FSurfaceSettings& Surface)
        {
            constexpr float MinTangentSpeed = 0.01f;
            const FVector3f Tangent = State.Velocity + IncomingSpeed * Normal;
            const float TangentSpeed = Tangent.Size();
            const float Backspin = TangentSpeed >= MinTangentSpeed ? FMath::Max(0.0f, Dot(State.Spin, Cross(Tangent / TangentSpeed, Normal))) : 0.0f;
            const float Speed = FMath::Max(TangentSpeed, MinTangentSpeed);
            const float Grip = FMath::Min(Surface.LandingSpinLoss * Radius * Backspin, FMath::Max(0.0f, GripLimit));
            return FMath::Exp(-Surface.LandingAngleLoss * IncomingSpeed / Speed) * FMath::Max(0.0f, 1.0f - Grip / Speed);
        }

        /** The spin an impact reads: a rate for restitution, retention and spin-back (rpm) and one for the bounce spin ratio (rad/s). */
        struct FImpactSpin
        {
            float Rpm = 0.0f;
            float RatioRadPerSecond = 0.0f;
        };

        /**
         * The backspin across the ball's travel, so neither sidespin nor topspin checks the ball (the bounce spin
         * ratio keeps its size either way); with no travel along the ground, the contact-plane spin.
         */
        FImpactSpin ReadImpactSpin(const FBallState& State, const FVector3f& Normal, float NormalSpeed)
        {
            const FVector3f Tangent = State.Velocity - NormalSpeed * Normal;
            const float TangentSpeed = Tangent.Size();
            if (TangentSpeed < 0.01f)
            {
                const float Plane = AlongPlane(State.Spin, Normal).Size();
                return {Plane / RadPerRpm, Plane};
            }
            const float Backspin = Dot(State.Spin, Cross(Tangent / TangentSpeed, Normal));
            return {FMath::Max(0.0f, Backspin) / RadPerRpm, FMath::Abs(Backspin)};
        }

        float Restitution(float NormalSpeed, float SpinRpm, const FBounceProfile& Profile, float MinimumSpeed)
        {
            const float BaseCor = NormalSpeed < MinimumSpeed                    ? 0.0f
                                  : NormalSpeed > Profile.CorHighSpeedThreshold ? Profile.CorHighSpeedCap
                                                                                : Profile.CorBaseA + Profile.CorBaseB * NormalSpeed + Profile.CorBaseC * NormalSpeed * NormalSpeed;
            const float SpeedFactor = NormalSpeed < Profile.CorVelocityLowThreshold
                                          ? Profile.CorVelocityLowScale * NormalSpeed / Profile.CorVelocityLowThreshold
                                      : NormalSpeed < Profile.CorVelocityMidThreshold
                                          ? Lerp(Profile.CorVelocityLowScale, 1.0f,
                                                (NormalSpeed - Profile.CorVelocityLowThreshold) / (Profile.CorVelocityMidThreshold - Profile.CorVelocityLowThreshold))
                                          : 1.0f;
            const float SpinFraction = SpinRpm < Profile.SpinCorLowSpinThreshold
                                           ? Profile.SpinCorLowSpinMaxReduction * SpinRpm / Profile.SpinCorLowSpinThreshold
                                           : Profile.SpinCorLowSpinMaxReduction + Profile.SpinCorHighSpinAdditionalReduction *
                                                                                      FMath::Min((SpinRpm - Profile.SpinCorLowSpinThreshold) / Profile.SpinCorHighSpinRangeRpm, 1.0f);
            return FMath::Clamp(BaseCor * (1.0f - SpinFraction * SpeedFactor), 0.0f, 1.0f);
        }

        /**
         * Penner-style check on top of the contact impulse: incoming backspin removes a further
         * SpinBiteScale * 2Rw/7 of forward tangential speed on steep, energetic landings, floored at
         * zero (or at -MaxSpinbackMps on spin-back surfaces). Lateral velocity and rebound are untouched.
         */
        FBallState ApplyLandingBite(const FBallState& Incoming, const FBallState& Resolved, const FVector3f& Normal,
            const FSurfaceSettings& Surface, const FLandingResponse& Response)
        {
            const FVector3f InTangent = AlongPlane(Incoming.Velocity, Normal);
            const float InTangentSpeed = InTangent.Size();
            if (InTangentSpeed < 0.01f)
            {
                return Resolved;
            }

            const FVector3f Direction = InTangent / InTangentSpeed;
            const float Backspin = Dot(Incoming.Spin, Cross(Direction, Normal));
            const float ImpactAngle = FMath::Atan2(-Dot(Incoming.Velocity, Normal), InTangentSpeed);
            const float Bite = Response.SpinBiteScale * Surface.SpinbackResponseScale * 2.0f / 7.0f * Radius * Backspin *
                               FMath::Clamp((ImpactAngle - Surface.CriticalAngle) / 0.1f, 0.0f, 1.0f) *
                               RampUp(Incoming.Velocity.Size(), Response.SpeedRampStartMps, Response.SpeedRampEndMps);
            if (Bite <= 0.0f)
            {
                return Resolved;
            }

            const float Rebound = Dot(Resolved.Velocity, Normal);
            const FVector3f OutTangent = Resolved.Velocity - Rebound * Normal;
            const float Forward = Dot(OutTangent, Direction);
            const float Floor = Response.MaxSpinbackMps > 0.0f && Surface.SpinbackResponseScale > 1.0f ? -Response.MaxSpinbackMps : 0.0f;
            const float Bitten = FMath::Max(Forward - Bite, FMath::Min(Floor, Forward));
            if (Bitten == Forward)
            {
                return Resolved;
            }

            const FVector3f Change = (Bitten - Forward) * Direction;
            FBallState Result = Resolved;
            Result.Velocity = Resolved.Velocity + Change;
            Result.Spin = Resolved.Spin + Cross(Normal, Change) / Radius;
            return KineticEnergy(Result) <= KineticEnergy(Incoming) ? Result : Resolved;
        }
    } // namespace ImpactDetail

    FBallState GolfPhysics::ResolveImpact(const FBallState& State, const FVector3f& InNormal, const FSurfaceSettings& Surface,
        const FBounceProfile& Profile, bool bFirstImpact, const FLandingResponse& Response)
    {
        using namespace ImpactDetail;
        const FVector3f Normal = ValidNormal(InNormal);
        const float NormalSpeed = Dot(State.Velocity, Normal);
        if (NormalSpeed >= 0.0f)
        {
            return State;
        }

        const float IncomingSpeed = -NormalSpeed;
        const FImpactSpin ImpactSpin = ReadImpactSpin(State, Normal, NormalSpeed);
        const float SpinRpm = ImpactSpin.Rpm;
        const float MinimumSpeed = bFirstImpact
                                       ? (Surface.SurfaceBounce ? Surface.SurfaceBounce->FirstImpactMinimumSpeed : Profile.CorKillThreshold)
                                       : (Surface.SurfaceBounce ? Surface.SurfaceBounce->LaterImpactMinimumSpeed
                                                                : FMath::Max(Profile.CorKillThreshold, Profile.RolloutBounceCorKillThreshold));
        float RestitutionValue = Restitution(IncomingSpeed, SpinRpm, Profile, MinimumSpeed);
        if (!bFirstImpact)
        {
            RestitutionValue *= Surface.SurfaceBounce ? Surface.SurfaceBounce->LaterImpactScale : Profile.RolloutBounceCorScale;
        }
        RestitutionValue *= Surface.RestitutionScale;
        const float NormalImpulse = Mass * (1.0f + RestitutionValue) * IncomingSpeed;
        const FVector3f Arm = -Radius * Normal;
        const FVector3f ContactVelocity = State.Velocity + Cross(State.Spin, Arm);
        const FVector3f Slip = AlongPlane(ContactVelocity, Normal);
        FVector3f TangentialImpulse = -2.0f * Mass / 7.0f * Slip;
        float SpinbackScale = Surface.SpinbackResponseScale;
        if (bFirstImpact && SpinbackScale > 1.0f)
        {
            const float SpinT = SmoothRange(SpinRpm, Surface.SpinbackSpinStartRpm, Surface.SpinbackSpinEndRpm);
            const float SpeedT = SmoothRange(State.Velocity.Size(), Surface.SpinbackSpeedStartMps, Surface.SpinbackSpeedEndMps);
            const float EffectiveAngle = Surface.CriticalAngle + Surface.SpinbackThetaBoostMax * SpinT * SpeedT;
            const float TangentSpeed = (State.Velocity - Normal * NormalSpeed).Size();
            const float ImpactAngle = FMath::Atan2(IncomingSpeed, TangentSpeed);
            const float Steepness = FMath::Clamp((ImpactAngle - EffectiveAngle) / 0.1f, 0.0f, 1.0f);
            SpinbackScale = 1.0f + (SpinbackScale - 1.0f) * Steepness;
        }

        const float MaxTangentialImpulse = Surface.KineticFriction * FMath::Max(0.0f, SpinbackScale) * NormalImpulse;
        const float TangentialLength = TangentialImpulse.Size();
        if (TangentialLength > MaxTangentialImpulse && TangentialLength > 0.0f)
        {
            TangentialImpulse *= MaxTangentialImpulse / TangentialLength;
        }

        FVector3f Velocity = State.Velocity + Normal * (NormalImpulse / Mass) + TangentialImpulse / Mass;
        FVector3f Spin = State.Spin + Cross(Arm, TangentialImpulse) / Inertia;
        // Turf reads its loss from this impact alone; other surfaces keep the ported first-impact and rollout retention.
        const bool bTurfLanding = UsesTurfLanding(Surface);
        float Retention = bTurfLanding ? TurfRetention(State, Normal, IncomingSpeed, MaxTangentialImpulse / Mass, Surface)
                          : bFirstImpact
                              ? FlightRetention(IncomingSpeed, SpinRpm, Profile)
                          : State.Velocity.Size() > 0.1f && Profile.RolloutSpinRatioThreshold > 0.0f
                              ? Lerp(Profile.RolloutLowSpinRetention, Profile.RolloutHighSpinRetention,
                                    FMath::Min(ImpactSpin.RatioRadPerSecond * Radius / State.Velocity.Size() / Profile.RolloutSpinRatioThreshold, 1.0f))
                              : Profile.RolloutHighSpinRetention;
        // A grazing contact spends little energy pushing into the surface.
        const float NormalEnergyShare = FMath::Clamp(IncomingSpeed * IncomingSpeed / FMath::Max(State.Velocity.SizeSquared(), 1e-12f), 0.0f, 1.0f);
        const float MaterialRetention = Lerp(1.0f, Surface.ImpactRetentionScale, NormalEnergyShare);
        Retention = FMath::Clamp(Retention * MaterialRetention, 0.0f, 1.0f);
        Velocity = Normal * Dot(Velocity, Normal) + Retention * (Velocity - Normal * Dot(Velocity, Normal));
        Spin = Normal * Dot(Spin, Normal) + Retention * (Spin - Normal * Dot(Spin, Normal));
        if (Surface.ImpactPlowRatio > 0.0f)
        {
            // Sand: the crater the ball digs takes forward speed in proportion to how hard it
            // drives in, so the landing angle matters far less than on turf.
            const FVector3f Tangent = Velocity - Normal * Dot(Velocity, Normal);
            const float TangentSpeed = Tangent.Size();
            const float Kept = TangentSpeed > 0.0f ? FMath::Max(0.0f, TangentSpeed - Surface.ImpactPlowRatio * IncomingSpeed) / TangentSpeed : 1.0f;
            Velocity = Normal * Dot(Velocity, Normal) + Kept * Tangent;
            Spin = Normal * Dot(Spin, Normal) + Kept * (Spin - Normal * Dot(Spin, Normal));
        }

        FBallState Resolved = State;
        Resolved.Velocity = Velocity;
        Resolved.Spin = Spin;
        return bFirstImpact && !bTurfLanding && Response.SpinBiteScale > 0.0f ? ApplyLandingBite(State, Resolved, Normal, Surface, Response) : Resolved;
    }

    FVector3f GolfPhysics::LaunchSpin(const FVector3f& Velocity, const FVector3f& MeasuredSpin, const FVector3f& InNormal, const FBounceProfile& Profile)
    {
        if (MeasuredSpin.SizeSquared() > 0.0f)
        {
            return MeasuredSpin;
        }

        const FVector3f Normal = ValidNormal(InNormal);
        const float NormalSpeed = Dot(Velocity, Normal);
        const FVector3f Tangent = Velocity - NormalSpeed * Normal;
        const float Rolling = (1.0f - ImpactDetail::LandingWeight(FMath::Abs(NormalSpeed), Profile)) *
                              (1.0f - RampUp(Tangent.Size(), PuttSpeedRampStartMps, PuttSpeedRampEndMps));
        return Rolling <= 0.0f ? FVector3f::ZeroVector : Rolling * Cross(Normal, Tangent) / Radius;
    }

    bool GolfPhysics::HasRebound(const FVector3f& Velocity, const FVector3f& InNormal, const TOptional<FSurfaceBounceSettings>& Settings)
    {
        const FVector3f Normal = ValidNormal(InNormal);
        const float Speed = Dot(Velocity, Normal);
        const float Height = Settings ? Settings->MinimumBounceHeight : DefaultMinimumBounceHeight;
        const float GravityIntoGround = Gravity * FMath::Max(Normal.Y, 0.0f);
        return Speed > 0.0f && Speed * Speed >= 2.0f * GravityIntoGround * Height;
    }

    float GolfPhysics::KineticEnergy(const FBallState& State)
    {
        return 0.5f * Mass * State.Velocity.SizeSquared() + 0.5f * Inertia * State.Spin.SizeSquared();
    }
} // namespace OpenFairway
