// SPDX-License-Identifier: MIT
// Ported from ogolf-physics addons/openfairway physics/SurfacePhysicsCatalog.cs and GreenSpeed.cs
// at e6d793ae9477efaf55dc9078d7260f76bb532a7d.
#include "OpenFairway/Surfaces.h"

#include "OpenFairway/GolfPhysics.h"

namespace OpenFairway
{
    namespace
    {
        constexpr float MetersPerFoot = 0.3048f;
        constexpr int32 SolveIterations = 32;

        /** Deceleration a rolling ball gains per m/s: viscous drag on its center and spin, through the 5/7 of a rolling ball. */
        double SpeedDragPerSecond(float Viscosity)
        {
            return 10.0 * 3.14159265 * GolfPhysics::Radius * Viscosity / GolfPhysics::Mass;
        }

        /**
         * Meters a ball rolling at this speed goes while it decelerates at a0 + k*v. Double precision:
         * with a small k the two terms nearly cancel.
         */
        double RollMeters(double Speed, double ConstantDeceleration, double K)
        {
            if (K <= 0.0)
            {
                return Speed * Speed / (2.0 * ConstantDeceleration);
            }
            return Speed / K - ConstantDeceleration / (K * K) * FMath::Loge(1.0 + K * Speed / ConstantDeceleration);
        }

        FSurfacePhysicsSettings Make(float KineticFriction, float RollingFriction, float GrassViscosity, float CriticalAngle, float SpinbackResponseScale)
        {
            FSurfacePhysicsSettings Settings;
            Settings.KineticFriction = KineticFriction;
            Settings.RollingFriction = RollingFriction;
            Settings.GrassViscosity = GrassViscosity;
            Settings.CriticalAngle = CriticalAngle;
            Settings.SpinbackResponseScale = SpinbackResponseScale;
            return Settings;
        }

        TArray<FSurfacePhysicsSettings> BuildCatalog()
        {
            TArray<FSurfacePhysicsSettings> Catalog;
            Catalog.SetNum(static_cast<int32>(ESurface::Count));
            Catalog[static_cast<int32>(ESurface::Fairway)] = Make(0.50f, 0.050f, 0.0017f, 0.29f, 0.78f);
            Catalog[static_cast<int32>(ESurface::FairwaySoft)] = Make(0.56f, 0.070f, 0.0024f, 0.32f, 0.92f);
            Catalog[static_cast<int32>(ESurface::Rough)] = Make(0.62f, 0.095f, 0.0032f, 0.35f, 0.70f);
            Catalog[static_cast<int32>(ESurface::Firm)] = Make(0.30f, 0.030f, 0.0010f, 0.25f, 0.60f);
            Catalog[static_cast<int32>(ESurface::Fringe)] = Make(0.56f, 0.0725f, 0.00245f, 0.29f, 0.78f);

            // A green rolls at its stimp speed for every ball: impact spin does not scale its rolling
            // friction (RollingSpinScale 0), and its rolling friction is solved with its grass drag so
            // the two stop a stimpmeter ball at the stimp distance. The profile's stimp replaces this.
            FSurfacePhysicsSettings& Green = Catalog[static_cast<int32>(ESurface::Green)];
            Green = Make(0.58f, GreenSpeed::RollingFriction(GreenSpeed::DefaultStimpFeet), GreenSpeed::GrassViscosity, 0.36f, 1.12f);
            Green.SpinbackThetaBoostMax = 0.12f;
            Green.SpinbackSpinStartRpm = 3500.0f;
            Green.SpinbackSpinEndRpm = 5500.0f;
            Green.SpinbackSpeedStartMps = 8.0f;
            Green.SpinbackSpeedEndMps = 20.0f;
            Green.RollingSpinScale = 0.0f;

            // Ordinary maintained sand: contact speed and spin decide whether it skips or settles.
            FSurfacePhysicsSettings& Sand = Catalog[static_cast<int32>(ESurface::Sand)];
            Sand = Make(0.65f, 0.30f, 0.012f, 0.45f, 1.0f);
            Sand.RestitutionScale = 0.60f;
            Sand.ImpactRetentionScale = 0.25f;
            Sand.SurfaceBounce = FSurfaceBounceSettings{0.15f, 0.15f, 0.85f, 0.001f};
            return Catalog;
        }
    } // namespace

    const FSurfacePhysicsSettings& SurfaceCatalog::Get(ESurface Surface)
    {
        static const TArray<FSurfacePhysicsSettings> Catalog = BuildCatalog();
        const int32 Index = static_cast<int32>(Surface);
        return Catalog.IsValidIndex(Index) ? Catalog[Index] : Catalog[static_cast<int32>(ESurface::Fairway)];
    }

    const TCHAR* SurfaceCatalog::GetName(ESurface Surface)
    {
        switch (Surface)
        {
        case ESurface::FairwaySoft:
            return TEXT("FairwaySoft");
        case ESurface::Rough:
            return TEXT("Rough");
        case ESurface::Firm:
            return TEXT("Firm");
        case ESurface::Green:
            return TEXT("Green");
        case ESurface::Sand:
            return TEXT("Sand");
        case ESurface::Fringe:
            return TEXT("Fringe");
        default:
            return TEXT("Fairway");
        }
    }

    float GreenSpeed::Clamp(float StimpFeet)
    {
        return FMath::IsFinite(StimpFeet) ? FMath::Clamp(StimpFeet, MinStimpFeet, MaxStimpFeet) : DefaultStimpFeet;
    }

    float GreenSpeed::RollingFriction(float StimpFeet, float Viscosity)
    {
        // The stimp fixes the constant deceleration a0 of a rolling ball (a0 + k*v); bisection finds
        // it because the roll distance has no closed-form inverse. Rolling friction = 7*a0 / (5*g).
        const double K = SpeedDragPerSecond(FMath::IsFinite(Viscosity) ? FMath::Max(Viscosity, 0.0f) : GrassViscosity);
        const float Distance = Clamp(StimpFeet) * MetersPerFoot;
        float Low = 0.0f;
        float High = StimpmeterSpeedMps * StimpmeterSpeedMps / (2.0f * Distance);
        for (int32 Iteration = 0; Iteration < SolveIterations; ++Iteration)
        {
            const float Middle = 0.5f * (Low + High);
            if (RollMeters(StimpmeterSpeedMps, Middle, K) > Distance)
            {
                Low = Middle;
            }
            else
            {
                High = Middle;
            }
        }
        return 7.0f * 0.5f * (Low + High) / (5.0f * GolfPhysics::Gravity);
    }
} // namespace OpenFairway
