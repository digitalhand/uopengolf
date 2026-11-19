// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/SurfacePhysicsCatalog.cs,
// SurfacePhysicsSettings.cs, PhysicsEnums.cs and GreenSpeed.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d. The default green grass drag differs from that
// source (see GreenSpeed::GrassViscosity); FBallPhysicsProfile::HistoricalReference keeps the ported value.
// Sand landings also plow (FBallPhysicsProfile::SandPlowRatio), which the reference profile leaves off, and sand
// rolls at FBallPhysicsProfile::SandRollingFriction; the reference keeps the catalog's ported 0.30.
// The turf surfaces also have a recalibrated catalog fitted to launch-monitor rollout (FBallPhysicsProfile::bRecalibratedTurf).
#pragma once

#include "OpenFairway/Profiles.h"

namespace OpenFairway
{
    /** Ground surfaces with their own contact tuning. The order matches EOpenFairwaySurface. */
    enum class ESurface : uint8
    {
        Fairway,
        FairwaySoft,
        Rough,
        Firm,
        Green,
        Sand,
        Fringe,
        Count
    };

    /** Surface tuning values before ball-profile and launch-regime multipliers. */
    struct FSurfacePhysicsSettings
    {
        float KineticFriction = 0.5f;
        float RollingFriction = 0.05f;
        float GrassViscosity = 0.0017f;
        float CriticalAngle = 0.29f;
        float SpinbackResponseScale = 1.0f;
        float SpinbackThetaBoostMax = 0.0f;
        float SpinbackSpinStartRpm = 0.0f;
        float SpinbackSpinEndRpm = 0.0f;
        float SpinbackSpeedStartMps = 0.0f;
        float SpinbackSpeedEndMps = 0.0f;
        /** Share of the impact-spin friction multiplier that reaches rolling resistance (0 on a green). */
        float RollingSpinScale = 1.0f;
        /** Share of the impact-spin friction multiplier that reaches sliding friction. */
        float SlidingSpinScale = 1.0f;
        /** Share of a landing's bounce the surface gives back (below 1 on sand). */
        float RestitutionScale = 1.0f;
        /** Forward speed kept at a perpendicular impact; grazing contact loses less. */
        float ImpactRetentionScale = 1.0f;
        /**
         * Turf landing loss from the impact itself (k): after the contact impulse an impact keeps
         * exp(-k * vn / vt) of its tangential velocity, vn and vt being the incoming normal and tangential
         * speeds. With this or LandingSpinLoss set, impacts on the surface use neither the bounce profile's
         * retention nor the landing bucket's bite. 0 on both keeps the ported impact.
         */
        float LandingAngleLoss = 0.0f;
        /**
         * Turf landing loss from backspin (c): the impact also keeps max(0, 1 - min(c * R * backspin, grip) / vt),
         * grip being the contact's Coulomb bound (kinetic friction times the normal impulse, per unit mass), so
         * a ball that barely touches the turf cannot be checked by it.
         */
        float LandingSpinLoss = 0.0f;
        /** Speed-squared grass drag along the ground: deceleration GrassFormDrag * v^2 (1/m). */
        float GrassFormDrag = 0.0f;
        /**
         * Grass drag scales (linear and speed-squared) for a full shot struck into or along the ground, a topped
         * shot that never flies, for the rest of its roll. Blended in from putt speed by
         * GolfPhysics::StruckAlongGroundWeight. 1 on both (green, sand, the ported catalog) changes nothing.
         */
        float StruckViscosityScale = 1.0f;
        float StruckFormDragScale = 1.0f;
        TOptional<FSurfaceBounceSettings> SurfaceBounce;
    };

    namespace SurfaceCatalog
    {
        /**
         * Single source of truth for surface tuning (the default green is 10 ft on the stimpmeter).
         * The ported ogolf-physics values by default; bRecalibratedTurf gives the turf surfaces (fairway,
         * soft fairway, rough, firm, fringe) fitted to launch-monitor rollout. Green and sand are the same in both.
         */
        OPENFAIRWAYPHYSICS_API const FSurfacePhysicsSettings& Get(ESurface Surface, bool bRecalibratedTurf = false);

        OPENFAIRWAYPHYSICS_API const TCHAR* GetName(ESurface Surface);
    } // namespace SurfaceCatalog

    /**
     * Green speed as a stimp reading: the feet a ball rolls on a level green after leaving a
     * stimpmeter at 1.83 m/s. RollingFriction turns that reading into the rolling friction the
     * ground solver needs.
     */
    namespace GreenSpeed
    {
        constexpr float MinStimpFeet = 8.0f;
        constexpr float MaxStimpFeet = 15.0f;
        constexpr float DefaultStimpFeet = 10.0f;
        constexpr float StimpmeterSpeedMps = 1.83f;
        /**
         * Speed-dependent resistance of every green: the original OpenFairway green value, below the
         * fairway's 0.0017, so a ball crossing a green from fairway rolls farther.
         */
        constexpr float GrassViscosity = 0.0009f;
        /** The ported ogolf-physics green drag, kept by FBallPhysicsProfile::HistoricalReference. */
        constexpr float ReferenceGrassViscosity = 0.011f;

        OPENFAIRWAYPHYSICS_API float Clamp(float StimpFeet);
        /** Rolling friction that, with this grass viscosity, stops a stimpmeter ball at the stimp distance. */
        OPENFAIRWAYPHYSICS_API float RollingFriction(float StimpFeet, float Viscosity = GrassViscosity, bool bAirDrag = true);
    } // namespace GreenSpeed
} // namespace OpenFairway
