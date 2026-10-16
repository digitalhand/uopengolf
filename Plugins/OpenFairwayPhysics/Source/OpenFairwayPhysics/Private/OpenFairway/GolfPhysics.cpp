// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway core/GolfPhysics.cs, core/Atmosphere.cs and
// core/SurfaceBounceSettings.cs at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
// The C# core throws on invalid input; this port has no exceptions, so an invalid contact
// normal falls back to up and callers validate launch data before simulating.
#include "OpenFairway/GolfPhysics.h"

#include "CoreMath.h"

namespace OpenFairway
{
    namespace
    {
        // A putt is slow: a 100 ft putt leaves the face at about 9 m/s. Above the end of this
        // ramp a low launch with no measured spin is a topped or thin full shot, not a putt.
        // MathF.PI in the C# source: single precision keeps the port bit-close.
        constexpr float PiF = 3.14159265f;

        constexpr float PuttSpeedRampStartMps = 12.0f;
        constexpr float PuttSpeedRampEndMps = 18.0f;

        float Dot(const FVector3f& A, const FVector3f& B)
        {
            return FVector3f::DotProduct(A, B);
        }
        FVector3f Cross(const FVector3f& A, const FVector3f& B)
        {
            return FVector3f::CrossProduct(A, B);
        }

        float SmoothRange(float Value, float Start, float End)
        {
            if (End <= Start)
            {
                return 0.0f;
            }
            const float T = FMath::Clamp((Value - Start) / (End - Start), 0.0f, 1.0f);
            return T * T * (3.0f - 2.0f * T);
        }

        /** Smooth 0 to 1 ramp; a zero-width ramp is a step at Start. */
        float RampUp(float Value, float Start, float End)
        {
            return End <= Start ? (Value >= Start ? 1.0f : 0.0f) : SmoothRange(Value, Start, End);
        }

        FVector3f ValidNormal(const FVector3f& Normal)
        {
            if (!FMath::IsFinite(Normal.X) || !FMath::IsFinite(Normal.Y) || !FMath::IsFinite(Normal.Z) || Normal.SizeSquared() < 0.000001f)
            {
                ensureMsgf(false, TEXT("OpenFairway: contact normal must be a finite nonzero vector."));
                return FVector3f(0.0f, 1.0f, 0.0f);
            }
            return Normal / Normal.Size();
        }

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

        FVector3f DampenSpin(const FVector3f& Spin, float Viscosity, float Dt)
        {
            const float DecayRate = 8.0f * PiF * Viscosity * GolfPhysics::Radius * GolfPhysics::Radius * GolfPhysics::Radius / GolfPhysics::Inertia;
            return Spin * FMath::Exp(-DecayRate * Dt);
        }

        FVector3f ResistContactSpin(const FVector3f& Spin, const FVector3f& Normal, float RollingFriction, float NormalLoad, float Dt)
        {
            const float NormalSpin = Dot(Spin, Normal);
            const float Reduction = RollingFriction * NormalLoad * GolfPhysics::Radius / GolfPhysics::Inertia * Dt;
            const float Remaining = FMath::Max(0.0f, FMath::Abs(NormalSpin) - Reduction);
            return Spin + (FMath::CopySign(Remaining, NormalSpin) - NormalSpin) * Normal;
        }

        /**
         * Penner-style check on top of the contact impulse: incoming backspin removes a further
         * SpinBiteScale * 2Rw/7 of forward tangential speed on steep, energetic landings, floored at
         * zero (or at -MaxSpinbackMps on spin-back surfaces). Lateral velocity and rebound are untouched.
         */
        FBallState ApplyLandingBite(const FBallState& Incoming, const FBallState& Resolved, const FVector3f& Normal,
            const FSurfaceSettings& Surface, const FLandingResponse& Response)
        {
            const FVector3f InTangent = Incoming.Velocity - Dot(Incoming.Velocity, Normal) * Normal;
            const float InTangentSpeed = InTangent.Size();
            if (InTangentSpeed < 0.01f)
            {
                return Resolved;
            }

            const FVector3f Direction = InTangent / InTangentSpeed;
            const float Backspin = Dot(Incoming.Spin, Cross(Direction, Normal));
            const float ImpactAngle = FMath::Atan2(-Dot(Incoming.Velocity, Normal), InTangentSpeed);
            const float Bite = Response.SpinBiteScale * Surface.SpinbackResponseScale * 2.0f / 7.0f * GolfPhysics::Radius * Backspin *
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
            Result.Spin = Resolved.Spin + Cross(Normal, Change) / GolfPhysics::Radius;
            return GolfPhysics::KineticEnergy(Result) <= GolfPhysics::KineticEnergy(Incoming) ? Result : Resolved;
        }

        constexpr float KelvinOffset = 273.15f;
        constexpr float SutherlandKelvin = 110.4f;
        constexpr float SeaLevelPressurePa = 101325.0f;
        constexpr float DryAirGasConstant = 287.058f;
        constexpr float MolarMassDryAir = 0.0289644f;
        constexpr float UniversalGasConstant = 8.314462618f;
        constexpr float MolarMassWater = 0.018016f;
        constexpr float TetensSaturationPa = 610.78f;
    } // namespace

    float Atmosphere::Density(float AltitudeMeters, float TemperatureCelsius, float RelativeHumidity, float StationPressurePa)
    {
        const float TemperatureKelvin = FMath::Max(TemperatureCelsius + KelvinOffset, 1.0f);
        const float Humidity = FMath::IsNaN(RelativeHumidity) ? 0.0f : FMath::Clamp(RelativeHumidity, 0.0f, 1.0f);
        const float Pressure = StationPressurePa > 0.0f
                                   ? StationPressurePa
                                   : SeaLevelPressurePa * FMath::Exp(-9.80665f * MolarMassDryAir * AltitudeMeters / (UniversalGasConstant * TemperatureKelvin));
        if (Humidity <= 0.0f)
        {
            return Pressure / (DryAirGasConstant * TemperatureKelvin);
        }
        // Tetens saturation pressure; the dry air contributes only its partial pressure.
        const float VapourPressure = Humidity * TetensSaturationPa * FMath::Pow(10.0f, 7.5f * TemperatureCelsius / (TemperatureCelsius + 237.3f));
        const float DryPressure = FMath::Max(Pressure - VapourPressure, 0.0f);
        return (DryPressure * MolarMassDryAir + VapourPressure * MolarMassWater) / (UniversalGasConstant * TemperatureKelvin);
    }

    float Atmosphere::DynamicViscosity(float TemperatureCelsius)
    {
        const float TemperatureKelvin = FMath::Max(TemperatureCelsius + KelvinOffset, 1.0f);
        return 1.716e-5f * FMath::Pow(TemperatureKelvin / KelvinOffset, 1.5f) * (KelvinOffset + SutherlandKelvin) /
               (TemperatureKelvin + SutherlandKelvin);
    }

    FBallState GolfPhysics::StepFlight(const FBallState& State, const FFlightEnvironment& Environment, const FFlightProfile& Profile, float Dt)
    {
        const FFlightAerodynamicsSample Sample = FlightAerodynamics::Sample(State.Velocity - Environment.Wind, State.Spin, Environment.AirDensity,
            Environment.AirViscosity, Environment.DragScale, Environment.LiftScale, Environment.InitialLaunchAngleDeg, Profile);
        return StepFlight(State, Environment, Sample, Dt);
    }

    FBallState GolfPhysics::StepFlight(const FBallState& State, const FFlightEnvironment& Environment, const FFlightAerodynamicsSample& Sample, float Dt)
    {
        FVector3f Force(0.0f, -Gravity * Mass, 0.0f);
        if (Sample.HasAerodynamics())
        {
            // Drag and lift act on the velocity through the air; the sample was taken from it too.
            const FVector3f AirVelocity = State.Velocity - Environment.Wind;
            Force += -0.5f * Sample.DragCoefficient * Environment.AirDensity * CrossSection * AirVelocity * Sample.Speed;
            const float SpinLength = State.Spin.Size();
            if (SpinLength > 0.1f)
            {
                Force += 0.5f * Sample.LiftCoefficient * Environment.AirDensity * CrossSection * Cross(State.Spin, AirVelocity) * Sample.Speed / SpinLength;
            }
        }

        FBallState Next;
        Next.Velocity = State.Velocity + Force / Mass * Dt;
        Next.Spin = State.Spin * FMath::Exp(-Sample.SpinDecayRatePerSecond * Dt);
        Next.Position = State.Position + Next.Velocity * Dt;
        return Next;
    }

    FBallState GolfPhysics::ResolveImpact(const FBallState& State, const FVector3f& InNormal, const FSurfaceSettings& Surface,
        const FBounceProfile& Profile, bool bFirstImpact, const FLandingResponse& Response)
    {
        const FVector3f Normal = ValidNormal(InNormal);
        const float NormalSpeed = Dot(State.Velocity, Normal);
        if (NormalSpeed >= 0.0f)
        {
            return State;
        }

        const float IncomingSpeed = -NormalSpeed;
        const float SpinRpm = State.Spin.Size() / RadPerRpm;
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
        const FVector3f Slip = ContactVelocity - Dot(ContactVelocity, Normal) * Normal;
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
        float Retention = bFirstImpact
                              ? FlightRetention(IncomingSpeed, SpinRpm, Profile)
                          : State.Velocity.Size() > 0.1f && Profile.RolloutSpinRatioThreshold > 0.0f
                              ? Lerp(Profile.RolloutLowSpinRetention, Profile.RolloutHighSpinRetention,
                                    FMath::Min(State.Spin.Size() * Radius / State.Velocity.Size() / Profile.RolloutSpinRatioThreshold, 1.0f))
                              : Profile.RolloutHighSpinRetention;
        // A grazing contact spends little energy pushing into the surface.
        const float NormalEnergyShare = FMath::Clamp(IncomingSpeed * IncomingSpeed / FMath::Max(State.Velocity.SizeSquared(), 1e-12f), 0.0f, 1.0f);
        const float MaterialRetention = Lerp(1.0f, Surface.ImpactRetentionScale, NormalEnergyShare);
        Retention = FMath::Clamp(Retention * MaterialRetention, 0.0f, 1.0f);
        Velocity = Normal * Dot(Velocity, Normal) + Retention * (Velocity - Normal * Dot(Velocity, Normal));
        Spin = Normal * Dot(Spin, Normal) + Retention * (Spin - Normal * Dot(Spin, Normal));

        FBallState Resolved = State;
        Resolved.Velocity = Velocity;
        Resolved.Spin = Spin;
        return bFirstImpact && Response.SpinBiteScale > 0.0f ? ApplyLandingBite(State, Resolved, Normal, Surface, Response) : Resolved;
    }

    FBallState GolfPhysics::StepGround(const FBallState& State, const FVector3f& InNormal, const FSurfaceSettings& Surface,
        const FRolloutProfile& Profile, float ImpactSpinRpm, float Dt)
    {
        const FVector3f Normal = ValidNormal(InNormal);
        FVector3f Velocity = State.Velocity - Dot(State.Velocity, Normal) * Normal;
        const FVector3f ContactVelocity = Velocity + Cross(State.Spin, -Radius * Normal);
        const FVector3f Slip = ContactVelocity - Dot(ContactVelocity, Normal) * Normal;
        const float NormalLoad = Mass * Gravity * FMath::Max(0.0f, Normal.Y);
        const FVector3f SlopeForce = FVector3f(0.0f, -Mass * Gravity, 0.0f) + Normal * NormalLoad;
        const float SlopeLength = SlopeForce.Size();
        const float StaticCapacity = Surface.RollingFriction * Profile.SlopeResistanceRatio * NormalLoad;
        if (Velocity.SizeSquared() < 0.05f * 0.05f && State.Spin.SizeSquared() < 8.0f * 8.0f && SlopeLength <= StaticCapacity)
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

        const float Multiplier = SpinFrictionMultiplier(FMath::Max(ImpactSpinRpm, State.Spin.Size() / RadPerRpm), Velocity.Size(), Profile);
        const bool bRolling = Slip.Size() < Profile.TangentVelocityThreshold;
        FVector3f UpdatedSpin = State.Spin;
        if (!bRolling && Slip.SizeSquared() > 0.0000001f)
        {
            FVector3f FrictionImpulse = -2.0f * Mass / 7.0f * Slip;
            const float MaxImpulse = Surface.KineticFriction * Multiplier * NormalLoad * Dt;
            const float ImpulseLength = FrictionImpulse.Size();
            if (ImpulseLength > MaxImpulse)
            {
                FrictionImpulse *= MaxImpulse / ImpulseLength;
            }

            Velocity += FrictionImpulse / Mass;
            UpdatedSpin += Cross(-Radius * Normal, FrictionImpulse) / Inertia;
        }

        FVector3f RollingForce = FVector3f::ZeroVector;
        if (Velocity.SizeSquared() > 0.0000001f)
        {
            // Scale 1 keeps the multiplier exactly; a green (0) rolls at its own resistance.
            const float RollingMultiplier = Surface.RollingSpinScale == 1.0f ? Multiplier : 1.0f + (Multiplier - 1.0f) * Surface.RollingSpinScale;
            RollingForce = -Surface.RollingFriction * RollingMultiplier * NormalLoad * (Velocity / Velocity.Size());
        }
        const FVector3f GrassDrag = -6.0f * PiF * Radius * Surface.GrassViscosity * Velocity;
        const FVector3f Acceleration = (SlopeForce + RollingForce + GrassDrag) / Mass;
        FVector3f UpdatedVelocity = Velocity + Acceleration * Dt;
        if (Dot(Velocity, UpdatedVelocity) < 0.0f && SlopeLength <= StaticCapacity)
        {
            UpdatedVelocity = FVector3f::ZeroVector;
        }

        UpdatedSpin = DampenSpin(UpdatedSpin, Surface.GrassViscosity, Dt);
        UpdatedSpin = ResistContactSpin(UpdatedSpin, Normal, Surface.RollingFriction, NormalLoad, Dt);
        FBallState Next;
        Next.Position = State.Position + UpdatedVelocity * Dt;
        Next.Velocity = UpdatedVelocity;
        Next.Spin = UpdatedSpin;
        return Next;
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
        if (!bSupported || State.Velocity.SizeSquared() >= 0.05f * 0.05f || State.Spin.SizeSquared() >= 0.5f * 0.5f)
        {
            return false;
        }

        const FVector3f Normal = ValidNormal(InNormal);
        const FVector3f SlopeGravity(0.0f, -Mass * Gravity, 0.0f);
        const FVector3f TangentGravity = SlopeGravity - Normal * Dot(SlopeGravity, Normal);
        return TangentGravity.Size() <= Surface.RollingFriction * Profile.SlopeResistanceRatio * Mass * Gravity * FMath::Max(0.0f, Normal.Y);
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
        const float Rolling = (1.0f - LandingWeight(FMath::Abs(NormalSpeed), Profile)) *
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
