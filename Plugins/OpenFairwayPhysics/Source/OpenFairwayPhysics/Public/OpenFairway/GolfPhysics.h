// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway core/GolfPhysics.cs, core/Atmosphere.cs,
// core/SurfaceBounceSettings.cs and physics/FlightAerodynamicsModel.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#pragma once

#include "OpenFairway/Profiles.h"

namespace OpenFairway
{
    /** Air properties from the barometric formula and Sutherland's law. */
    namespace Atmosphere
    {
        /**
         * Air density in kg/m^3. RelativeHumidity is 0-1 (clamped; NaN is dry air). A station
         * pressure of 0 derives the pressure from altitude; otherwise it is used as measured.
         */
        OPENFAIRWAYPHYSICS_API float Density(float AltitudeMeters, float TemperatureCelsius, float RelativeHumidity = 0.0f, float StationPressurePa = 0.0f);

        /** Dynamic viscosity in kg/(m*s). */
        OPENFAIRWAYPHYSICS_API float DynamicViscosity(float TemperatureCelsius);

        inline float FahrenheitToCelsius(float TemperatureF)
        {
            return (TemperatureF - 32.0f) * 5.0f / 9.0f;
        }
    } // namespace Atmosphere

    /** Drag and lift coefficients for one instant of flight. */
    struct FFlightAerodynamicsSample
    {
        float Speed = 0.0f;
        float SpinRatio = 0.0f;
        float Reynolds = 0.0f;
        float SpinDragMultiplier = 1.0f;
        float LowLaunchLiftScale = 1.0f;
        float DragCoefficient = 0.0f;
        float LiftCoefficient = 0.0f;
        float SpinDecayRatePerSecond = 0.0f;

        bool HasAerodynamics() const;
    };

    /** Wind-tunnel polynomial fits for drag and lift with the calibrated corrections. */
    namespace FlightAerodynamics
    {
        /** Below this speed (m/s) the ball has no aerodynamic forces. */
        constexpr float MinAerodynamicSpeed = 0.5f;

        OPENFAIRWAYPHYSICS_API FFlightAerodynamicsSample Sample(const FVector3f& Velocity, const FVector3f& Omega, float AirDensity,
            float AirViscosity, float DragScale, float LiftScale, float InitialLaunchAngleDeg, const FFlightProfile& Profile);

        OPENFAIRWAYPHYSICS_API float GetCd(float Reynolds, const FFlightProfile& Profile);
        OPENFAIRWAYPHYSICS_API float GetCl(float Reynolds, float SpinRatio, const FFlightProfile& Profile);
        OPENFAIRWAYPHYSICS_API float GetSpinDragMultiplier(float SpinRatio, float Reynolds, const FFlightProfile& Profile);
        OPENFAIRWAYPHYSICS_API float GetLowLaunchLiftScale(float InitialLaunchAngleDeg, float SpinRatio, float Reynolds, const FFlightProfile& Profile);
        OPENFAIRWAYPHYSICS_API float GetHighLaunchDragScale(float InitialLaunchAngleDeg, float SpinRatio, const FFlightProfile& Profile);
        OPENFAIRWAYPHYSICS_API float GetMidSpinClBoost(float SpinRatio, const FFlightProfile& Profile);
        OPENFAIRWAYPHYSICS_API float GetClMax(float SpinRatio, const FFlightProfile& Profile);
    } // namespace FlightAerodynamics

    /** Flight, impact, ground and rest equations shared by the headless simulator and the ball actor. */
    namespace GolfPhysics
    {
        constexpr float Mass = 0.04592623f;
        constexpr float Radius = 0.021335f;
        constexpr float CrossSection = 3.14159265f * Radius * Radius;
        constexpr float Inertia = 0.4f * Mass * Radius * Radius;
        constexpr float Gravity = 9.81f;
        constexpr float RadPerRpm = 0.10472f;

        /** Simulation rate used by ogolf-physics and its calibration. */
        constexpr float SimulationHz = 120.0f;
        constexpr float SimulationDt = 1.0f / SimulationHz;

        /**
         * State.Position.Y must be the ball centre's height above the launch ground (it starts at
         * Radius): with a profiled Environment the wind at height 0 is zero.
         */
        OPENFAIRWAYPHYSICS_API FBallState StepFlight(const FBallState& State, const FFlightEnvironment& Environment, const FFlightProfile& Profile, float Dt);
        /**
         * Sample must come from the local air-relative velocity, State.Velocity minus
         * WindProfile::At(Environment, State.Position.Y) (the wind at the ball's height).
         */
        OPENFAIRWAYPHYSICS_API FBallState StepFlight(const FBallState& State, const FFlightEnvironment& Environment, const FFlightAerodynamicsSample& Sample, float Dt);

        /** Contact impulse with spin-dependent restitution, friction-limited tangential response and landing bite. */
        OPENFAIRWAYPHYSICS_API FBallState ResolveImpact(const FBallState& State, const FVector3f& Normal, const FSurfaceSettings& Surface,
            const FBounceProfile& Profile, bool bFirstImpact = true, const FLandingResponse& Response = FLandingResponse());

        /** One ground step: sliding friction until rolling, rolling resistance, grass drag and slope. */
        OPENFAIRWAYPHYSICS_API FBallState StepGround(const FBallState& State, const FVector3f& Normal, const FSurfaceSettings& Surface,
            const FRolloutProfile& Profile, float ImpactSpinRpm, float Dt);

        OPENFAIRWAYPHYSICS_API float SpinFrictionMultiplier(float SpinRpm, float Speed, const FRolloutProfile& Profile);

        /** True when a supported ball is slow enough and the slope cannot move it. */
        OPENFAIRWAYPHYSICS_API bool CanRest(const FBallState& State, bool bSupported, const FVector3f& Normal, const FSurfaceSettings& Surface,
            const FRolloutProfile& Profile = FRolloutProfile());

        /** Spin the ball leaves the club with: measured spin, or rolling spin for a putt with none measured. */
        OPENFAIRWAYPHYSICS_API FVector3f LaunchSpin(const FVector3f& Velocity, const FVector3f& MeasuredSpin, const FVector3f& Normal, const FBounceProfile& Profile);

        /** True when the ball leaves the surface high enough to count as a bounce rather than a roll. */
        OPENFAIRWAYPHYSICS_API bool HasRebound(const FVector3f& Velocity, const FVector3f& Normal, const TOptional<FSurfaceBounceSettings>& Settings);

        OPENFAIRWAYPHYSICS_API float KineticEnergy(const FBallState& State);

        /** Matches the 0.5 m/s settling threshold on level ground. */
        constexpr float DefaultMinimumBounceHeight = 0.25f / (2.0f * Gravity);
    } // namespace GolfPhysics
} // namespace OpenFairway
