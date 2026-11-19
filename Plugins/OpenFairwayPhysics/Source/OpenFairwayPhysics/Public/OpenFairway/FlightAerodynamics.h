// SPDX-License-Identifier: MIT
#pragma once

#include "OpenFairway/Profiles.h"

namespace OpenFairway
{
    /** Drag and lift coefficients for one instant of flight. */
    struct FFlightAerodynamicsSample
    {
        float Speed = 0.0f;
        /** Surface speed over air speed, W = |omega| R / v. */
        float SpinRatio = 0.0f;
        float Reynolds = 0.0f;
        float DragCoefficient = 0.0f;
        float LiftCoefficient = 0.0f;
        float SpinDecayRatePerSecond = 0.0f;
        /** True outside the Reynolds number and spin ratio the flight table was fitted over. */
        bool bOutsideMeasuredCoverage = false;

        bool HasAerodynamics() const;
    };

    /** Drag and lift of the ball through the air. */
    namespace FlightAerodynamics
    {
        /** Below this speed (m/s) the ball has no aerodynamic forces. */
        constexpr float MinAerodynamicSpeed = 0.5f;

        /**
         * The coefficients for this air-relative velocity and spin: drag and lift as fitted curves in the spin ratio,
         * plus the measured drag rise at low Reynolds number (Private/OpenFairway/SpinRatioData.h). Nothing about the
         * launch reaches them.
         */
        OPENFAIRWAYPHYSICS_API FFlightAerodynamicsSample Sample(const FVector3f& Velocity, const FVector3f& Omega, float AirDensity,
            float AirViscosity, const FFlightProfile& Profile = FFlightProfile());

        /**
         * Drag coefficient of a ball moving along the ground: a drag polynomial in Reynolds number times a spin-drag
         * multiplier, from the flight model the turf and the green speed were calibrated with. GolfPhysics::StepGround
         * and GreenSpeed::RollingFriction read it; flight does not.
         */
        OPENFAIRWAYPHYSICS_API float GroundDragCoefficient(float Reynolds, float SpinRatio);
    } // namespace FlightAerodynamics
} // namespace OpenFairway
