// SPDX-License-Identifier: MIT
// Bounce and roll-out parameters originally ported from ogolf-physics (BounceProfile.cs, RolloutProfile.cs,
// LandingResponse.cs and SurfaceBounceSettings.cs); the flight is the fitted spin-ratio model (SpinRatioData.h).
#pragma once

#include "CoreMinimal.h"
#include "OpenFairway/WindProfile.h"

/**
 * OpenFairway core: plain C++ on Unreal's Core module only, with no UObjects, worlds or
 * scene queries. Units are SI (meters, m/s, rad/s) in the OpenFairway frame: +X downrange,
 * +Y up, +Z to the right (right-handed). The Unreal ball actor converts at its boundary.
 */
namespace OpenFairway
{
    /** Ball position, velocity and spin (angular velocity). */
    struct FBallState
    {
        FVector3f Position = FVector3f::ZeroVector;
        FVector3f Velocity = FVector3f::ZeroVector;
        FVector3f Spin = FVector3f::ZeroVector;
    };

    /** The air a shot flies and rolls through, fixed at launch. */
    struct FFlightEnvironment
    {
        float AirDensity = 1.225f;
        float AirViscosity = 1.789e-5f;
        /** Scale on the air drag of a ball moving along the ground (FBallPhysicsProfile::GroundDragScale); flight ignores it. */
        float GroundDragScale = 1.0f;
        /** Air velocity (m/s, OpenFairway frame). Drag and lift act on the ball's speed through the air. */
        FVector3f Wind = FVector3f::ZeroVector;
        /** Height (m) at which Wind is measured; 0 keeps the wind uniform with height. See WindProfile. */
        float WindReferenceHeightM = 0.0f;
        /** Roughness length (m) of the ground the wind blows over. */
        float WindRoughnessM = WindProfile::OpenGrassRoughnessM;
    };

    /** Flight constants. Drag and lift come from the spin ratio alone (FlightAerodynamics::Sample). */
    struct FFlightProfile
    {
        // Smits/Smith spin-down: d(omega)/dt = -coefficient * speed/radius * omega (SI).
        // See Docs/Reference.md (References: Nathan; Smits and Smith) for the source and applicability limits.
        float SpinDecayCoefficient = 2.0e-5f;
    };

    /** Coefficient of restitution and tangential retention for impacts. */
    struct FBounceProfile
    {
        // Impacts read the backspin across the ball's travel, not the total spin: sidespin and spin about the
        // contact normal cannot check a ball.

        // COR curve.
        float CorBaseA = 0.45f;
        float CorBaseB = -0.01f;
        float CorBaseC = 0.0002f;
        float CorHighSpeedCap = 0.25f;
        float CorHighSpeedThreshold = 20.0f;
        float CorKillThreshold = 2.0f;

        // Tangential retention on the first bounce from flight.
        float FlightTangentialRetentionBase = 0.55f;
        float FlightSpinFactorMin = 0.40f;
        float FlightSpinFactorDivisor = 8000.0f;

        // The first-impact loss fades in with normal impact speed: none at or below the start
        // (a ball struck along the ground only touches the turf), all of it from the end.
        float FlightRetentionRampStartMps = 0.75f;
        float FlightRetentionRampEndMps = 2.0f;

        // Tangential retention on rollout bounces.
        float RolloutLowSpinRetention = 0.85f;
        float RolloutHighSpinRetention = 0.70f;
        float RolloutSpinRatioThreshold = 0.20f;

        // Spin COR reduction.
        float SpinCorLowSpinThreshold = 1500.0f;
        float SpinCorLowSpinMaxReduction = 0.30f;
        float SpinCorHighSpinRangeRpm = 1500.0f;
        float SpinCorHighSpinAdditionalReduction = 0.40f;

        // Velocity scaling for the COR reduction.
        float CorVelocityLowThreshold = 12.0f;
        float CorVelocityMidThreshold = 25.0f;
        float CorVelocityLowScale = 0.50f;

        // Rollout bounce COR.
        float RolloutBounceCorKillThreshold = 4.0f;
        float RolloutBounceCorScale = 0.5f;
    };

    /** Ground friction constants for roll-out. */
    struct FRolloutProfile
    {
        // Resistance and contact are solved together each step. Ground contact reads only the spin in the contact
        // plane: spin about the normal pivots the ball without rolling it, so it neither adds friction nor keeps a
        // stopped ball moving.

        // Impact-spin friction multiplier (GolfPhysics::SpinFrictionMultiplier), by ground speed and landing spin.
        float ChipSpeedThreshold = 20.0f;
        float PitchSpeedThreshold = 35.0f;
        float ChipVelocityScaleMin = 0.60f;
        float ChipVelocityScaleMax = 0.87f;
        float LowSpinThreshold = 1250.0f;
        float MidSpinThreshold = 1750.0f;
        float LowSpinMultiplierMax = 1.30f;
        float MidSpinMultiplierMax = 2.25f;
        float HighSpinMultiplierMax = 2.50f;
        float HighSpinRampRange = 1000.0f;

        /**
         * Turf resists the down-slope drive up to RollingFriction * SlopeResistanceRatio of the
         * normal load. No effect on flat ground, so flat roll-out calibration is unchanged.
         */
        float SlopeResistanceRatio = 1.6f;
    };

    /**
     * Per-shot first-impact "bite" chosen by the landing bucket. Incoming backspin removes extra
     * forward speed (Penner's 2Rw/7 term) so high-spin wedges check; on spin-back surfaces the
     * ball may draw back up to MaxSpinbackMps. Zero (the default) leaves the impact unchanged.
     */
    struct FLandingResponse
    {
        float SpinBiteScale = 0.0f;
        float MaxSpinbackMps = 0.0f;
        float SpeedRampStartMps = 0.0f;
        float SpeedRampEndMps = 0.0f;
    };

    /** Bounce thresholds for soft surfaces (sand) that replace the bounce profile's. */
    struct FSurfaceBounceSettings
    {
        float FirstImpactMinimumSpeed = 0.0f;
        float LaterImpactMinimumSpeed = 0.0f;
        float LaterImpactScale = 1.0f;
        float MinimumBounceHeight = 0.0f;
    };

    /** Contact parameters of the surface the ball is touching. */
    struct FSurfaceSettings
    {
        float KineticFriction = 0.5f;
        float RollingFriction = 0.05f;
        float GrassViscosity = 0.0017f;
        float SpinbackResponseScale = 1.0f;
        float CriticalAngle = 0.29f;
        float SpinbackThetaBoostMax = 0.0f;
        float SpinbackSpinStartRpm = 0.0f;
        float SpinbackSpinEndRpm = 0.0f;
        float SpinbackSpeedStartMps = 0.0f;
        float SpinbackSpeedEndMps = 0.0f;
        float RollingSpinScale = 1.0f;
        /** Share of the impact-spin friction multiplier that reaches sliding friction. */
        float SlidingSpinScale = 1.0f;
        float RestitutionScale = 1.0f;
        float ImpactRetentionScale = 1.0f;
        /** Forward speed removed per m/s of incoming normal speed at an impact (a sand crater); 0 elsewhere. */
        float ImpactPlowRatio = 0.0f;
        /**
         * Turf landing loss read from the impact state (see FSurfacePhysicsSettings::LandingAngleLoss): the
         * impact keeps exp(-LandingAngleLoss * vn / vt) * max(0, 1 - min(LandingSpinLoss * R * backspin, grip) / vt)
         * of its tangential velocity, grip being the contact's Coulomb bound per unit mass. Either set replaces the
         * bounce profile's retention and the landing bite.
         */
        float LandingAngleLoss = 0.0f;
        float LandingSpinLoss = 0.0f;
        /** Speed-squared grass drag on a ball moving along the ground: deceleration GrassFormDrag * v^2 (1/m). */
        float GrassFormDrag = 0.0f;
        /** Grass drag scales for a full shot struck into or along the ground (see FSurfacePhysicsSettings). */
        float StruckViscosityScale = 1.0f;
        float StruckFormDragScale = 1.0f;
        TOptional<FSurfaceBounceSettings> SurfaceBounce;
    };
} // namespace OpenFairway
