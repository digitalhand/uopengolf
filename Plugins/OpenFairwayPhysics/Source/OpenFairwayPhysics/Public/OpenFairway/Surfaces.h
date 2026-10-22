// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/SurfacePhysicsCatalog.cs,
// SurfacePhysicsSettings.cs, PhysicsEnums.cs and GreenSpeed.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d. The default green grass drag differs from that
// source (see GreenSpeed::GrassViscosity); FBallPhysicsProfile::HistoricalReference keeps the ported value.
// Sand landings also plow (FBallPhysicsProfile::SandPlowRatio), which the reference profile leaves off.
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
        /** Share of a landing's bounce the surface gives back (below 1 on sand). */
        float RestitutionScale = 1.0f;
        /** Forward speed kept at a perpendicular impact; grazing contact loses less. */
        float ImpactRetentionScale = 1.0f;
        TOptional<FSurfaceBounceSettings> SurfaceBounce;
    };

    namespace SurfaceCatalog
    {
        /** Single source of truth for surface tuning (the default green is 10 ft on the stimpmeter). */
        OPENFAIRWAYPHYSICS_API const FSurfacePhysicsSettings& Get(ESurface Surface);

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
        OPENFAIRWAYPHYSICS_API float RollingFriction(float StimpFeet, float Viscosity = GrassViscosity);
    } // namespace GreenSpeed
} // namespace OpenFairway
