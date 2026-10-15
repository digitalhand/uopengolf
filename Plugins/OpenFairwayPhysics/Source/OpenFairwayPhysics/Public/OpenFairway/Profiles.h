// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/FlightProfile.cs, BounceProfile.cs,
// RolloutProfile.cs and core/LandingResponse.cs, SurfaceBounceSettings.cs, GolfPhysics.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#pragma once

#include "CoreMinimal.h"

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

    /** Air and scale inputs for one flight step. */
    struct FFlightEnvironment
    {
        float AirDensity = 1.225f;
        float AirViscosity = 1.789e-5f;
        float DragScale = 1.0f;
        float LiftScale = 1.0f;
        float InitialLaunchAngleDeg = 0.0f;
    };

    /** Aerodynamic constants: drag polynomial, lift caps and the calibrated spin/launch corrections. */
    struct FFlightProfile
    {
        // Smits/Smith spin-down: d(omega)/dt = -coefficient * speed/radius * omega (SI).
        // See Docs/FlightScopeAnalysis.md for the source and applicability limits.
        float SpinDecayCoefficient = 2.0e-5f;
        // Nonzero only for historical reference comparisons (the old 5-second decay).
        float ConstantSpinDecayRatePerSecond = 0.0f;

        // Drag curve (Cd polynomial).
        float CdPolyA = 1.1948f;
        float CdPolyB = -0.0000209661f;
        float CdPolyC = 1.42472e-10f;
        float CdPolyD = -3.14383e-16f;
        float HighReCdCap = 0.2f;
        float LowReCdFloor = 0.38f;
        float LowReBlendStart = 30000.0f;
        float CdAt50k = 0.4632f;
        float CdMin = 0.223f;

        // Lift caps.
        float ClMaxBase = 0.268f;
        float ClMaxHighSpin = 0.32f;
        float ClMaxSrTransitionStart = 0.35f;
        float ClMaxSrTransitionEnd = 0.50f;

        // Spin drag.
        float SpinDragMultiplierCoeff = 4.0f;
        float SpinDragMultiplierMax = 1.20f;
        float SpinDragMultiplierHighSpinMax = 1.03f;
        float SpinDragMultiplierUltraHighSpinMax = 1.21f;
        float HighSpinDragSrStart = 0.30f;
        float HighSpinDragSrEnd = 0.48f;
        float HighSpinDragReliefReFullMax = 90000.0f;
        float HighSpinDragReliefReZero = 105000.0f;
        float UltraHighSpinDragSrStart = 0.57f;
        float UltraHighSpinDragSrEnd = 0.77f;

        // High-Re lift.
        float HighReStart = 75000.0f;
        float HighReMidSpinGain = 16.0f;
        float HighReSpinGain = 16.0f;
        float HighReGainReductionStart = 0.10f;
        float HighReGainReductionEnd = 0.18f;
        float HighReGainRecoveryStart = 0.26f;
        float HighReGainRecoveryEnd = 0.40f;

        // High-Re lift attenuation.
        float HighSpinClAttenuationStart = 0.45f;
        float HighSpinClAttenuationEnd = 0.55f;
        float HighSpinClAttenuationMax = 0.09f;
        float UltraHighSpinClAttenuationStart = 0.58f;
        float UltraHighSpinClAttenuationEnd = 0.85f;
        float UltraHighSpinClAttenuationMax = 0.10f;

        // Low-Re lift attenuation.
        float LowReHighSpinClAttenuationMax = 0.10f;
        float LowReUltraHighSpinClAttenuationMax = 0.06f;

        // Optional legacy low-launch lift recovery. Neutral by default: retaining
        // spin longer exposed excess lift in low-launch woods. HistoricalReference
        // restores the original 1.08 for pinned C# fixture parity.
        float LowLaunchLiftRecoveryMax = 1.0f;
        float LowLaunchVlaFullDeg = 6.5f;
        float LowLaunchVlaZeroDeg = 9.5f;
        float LowLaunchReStart = 85000.0f;
        float LowLaunchReEnd = 110000.0f;
        float LowLaunchSpinRatioFull = 0.18f;
        float LowLaunchSpinRatioMax = 0.22f;

        // Progressive spin drag cap boost (more form drag at high spin ratio).
        float SpinDragProgressiveCapSrStart = 0.33f;
        float SpinDragProgressiveCapSrEnd = 0.50f;
        float SpinDragProgressiveCapBoostMax = 0.25f;

        // Mid-spin lift boost (bell-shaped recovery for the mid-iron spin ratio regime).
        float MidSpinClBoostSrStart = 0.17f;
        float MidSpinClBoostSrEnd = 0.31f;
        float MidSpinClBoostMax = 0.45f;

        // High-launch drag boost.
        float HighLaunchDragBoostMax = 1.24f;
        float HighLaunchDragVlaStartDeg = 24.5f;
        float HighLaunchDragVlaFullDeg = 31.5f;
        float HighLaunchDragSrStart = 0.50f;
        float HighLaunchDragSrEnd = 0.70f;
    };

    /** Coefficient of restitution and tangential retention for impacts. */
    struct FBounceProfile
    {
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
        float FrictionBlendSpeed = 15.0f;
        float TangentVelocityThreshold = 0.05f;

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
        float RestitutionScale = 1.0f;
        float ImpactRetentionScale = 1.0f;
        TOptional<FSurfaceBounceSettings> SurfaceBounce;
    };
} // namespace OpenFairway
