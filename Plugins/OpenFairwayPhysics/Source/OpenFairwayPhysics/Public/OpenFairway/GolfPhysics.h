// SPDX-License-Identifier: MIT
// Impact, ground and rest equations originally ported from ogolf-physics (core/GolfPhysics.cs); flight is RK4 on
// the fitted spin-ratio aerodynamics.
#pragma once

#include "OpenFairway/Atmosphere.h"
#include "OpenFairway/FlightAerodynamics.h"
#include "OpenFairway/Profiles.h"

namespace OpenFairway
{
    /** Flight, impact, ground and rest equations shared by the headless simulator and the ball actor. */
    namespace GolfPhysics
    {
        constexpr float Mass = 0.04592623f;
        constexpr float Radius = 0.021335f;
        constexpr float CrossSection = UE_PI * Radius * Radius;
        constexpr float Inertia = 0.4f * Mass * Radius * Radius;
        constexpr float Gravity = 9.81f;
        constexpr float RadPerRpm = 0.10472f;

        /** Simulation rate of the headless simulator and the ball actor, and the rate the physics was calibrated at. */
        constexpr float SimulationHz = 120.0f;
        constexpr float SimulationDt = 1.0f / SimulationHz;

        /**
         * Tangential speed (m/s) that tells a putt from a ball struck along the ground: a 100 ft putt leaves the face at
         * about 9 m/s. Below the start a launch with no measured spin rolls like a putt; from the end it is a topped or
         * thin full shot. LaunchSpin and StruckAlongGroundWeight share the ramp.
         */
        constexpr float PuttSpeedRampStartMps = 12.0f;
        constexpr float PuttSpeedRampEndMps = 18.0f;

        /** Below this ground speed (m/s), with little rolling spin and a slope the turf holds, a ball stops. */
        constexpr float RestSpeedMps = 0.05f;

        /** Matches the 0.5 m/s settling threshold on level ground. */
        constexpr float DefaultMinimumBounceHeight = 0.25f / (2.0f * Gravity);

        /**
         * One coupled RK4 flight step (position, velocity and spin): gravity, drag and Magnus lift on the air-relative
         * velocity, and spin decay, resampling the height-dependent wind and the aerodynamics at every stage. The wind
         * is read at Dot(Position, GroundNormal), the ball centre's height above the launch ground (it starts at Radius).
         */
        OPENFAIRWAYPHYSICS_API FBallState StepFlight(const FBallState& State, const FFlightEnvironment& Environment,
            const FFlightProfile& Profile, float Dt, const FVector3f& GroundNormal = FVector3f(0, 1, 0));

        /** Time within Dt (s) at which the flight first meets the plane Dot(Position, Normal) = PlaneHeight, by bisection. */
        OPENFAIRWAYPHYSICS_API float FlightContactTime(const FBallState& State, const FFlightEnvironment& Environment,
            const FFlightProfile& Profile, float Dt, const FVector3f& Normal, float PlaneHeight);

        /**
         * Contact impulse with spin-dependent restitution and a friction-limited tangential response. On turf
         * (LandingAngleLoss or LandingSpinLoss set) the impact then keeps a share of its tangential speed read from
         * its own angle and backspin, the backspin part bounded by the contact's Coulomb limit; other surfaces use
         * the bounce profile's retention and, on the first impact, the landing bucket's bite.
         */
        OPENFAIRWAYPHYSICS_API FBallState ResolveImpact(const FBallState& State, const FVector3f& Normal, const FSurfaceSettings& Surface,
            const FBounceProfile& Profile, bool bFirstImpact = true, const FLandingResponse& Response = FLandingResponse());

        /**
         * One ground step: rolling resistance, linear and speed-squared grass drag, air drag and slope, then a
         * Coulomb-capped contact impulse that turns sliding into rolling. The ball meets the wind one radius above its
         * lie, whatever State.Position is, and its air drag reads only its velocity, spin and Environment.GroundDragScale.
         */
        OPENFAIRWAYPHYSICS_API FBallState StepGround(const FBallState& State, const FVector3f& Normal, const FSurfaceSettings& Surface,
            const FRolloutProfile& Profile, float ImpactSpinRpm, float Dt, const FFlightEnvironment& Environment = FFlightEnvironment());

        /** Friction multiplier for the landing spin a roll-out remembers, faded in with ground speed. */
        OPENFAIRWAYPHYSICS_API float SpinFrictionMultiplier(float SpinRpm, float Speed, const FRolloutProfile& Profile);

        /** The landing spin a roll-out remembers (StepGround's ImpactSpinRpm), rpm: the spin in the contact plane. */
        OPENFAIRWAYPHYSICS_API float RolloutSpinRpm(const FVector3f& Spin, const FVector3f& Normal);

        /** True when a supported ball is slow enough and the slope cannot move it. */
        OPENFAIRWAYPHYSICS_API bool CanRest(const FBallState& State, bool bSupported, const FVector3f& Normal, const FSurfaceSettings& Surface,
            const FRolloutProfile& Profile = FRolloutProfile());

        /** Spin the ball leaves the club with: measured spin, or rolling spin for a putt with none measured. */
        OPENFAIRWAYPHYSICS_API FVector3f LaunchSpin(const FVector3f& Velocity, const FVector3f& MeasuredSpin, const FVector3f& Normal, const FBounceProfile& Profile);

        /**
         * How much of a turf surface's struck-along-the-ground drag a shot gets when it is launched into or
         * along the ground at this tangential speed (m/s): none for a putt (below 12 m/s), all of it for a
         * topped full shot (18 m/s and up). The same ramp tells a putt from a topped shot in LaunchSpin.
         */
        OPENFAIRWAYPHYSICS_API float StruckAlongGroundWeight(float TangentSpeed);

        /**
         * The surface a ball struck along the ground rolls on for the rest of its shot: grass drags scaled
         * toward StruckViscosityScale and StruckFormDragScale by Weight (0 returns Surface unchanged). This is
         * the one rule in the roll that reads how the shot started; see the reference docs, "Topped shots".
         */
        OPENFAIRWAYPHYSICS_API FSurfaceSettings StruckAlongGround(const FSurfaceSettings& Surface, float Weight);

        /** True when the ball leaves the surface high enough to count as a bounce rather than a roll. */
        OPENFAIRWAYPHYSICS_API bool HasRebound(const FVector3f& Velocity, const FVector3f& Normal, const TOptional<FSurfaceBounceSettings>& Settings);

        OPENFAIRWAYPHYSICS_API float KineticEnergy(const FBallState& State);
    } // namespace GolfPhysics
} // namespace OpenFairway
