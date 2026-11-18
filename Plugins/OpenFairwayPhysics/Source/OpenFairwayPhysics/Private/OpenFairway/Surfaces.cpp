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

        // Recalibrated turf, fitted to the Square Home export in Assets/Data/ShotLibrary (124 shots, flat
        // fairway, 75 F, sea level) by UOpenGolf.Calibration.Rollout. An impact keeps the standard restitution
        // and Coulomb-capped friction impulse, then loses tangential speed by its own angle and load-bounded backspin
        // grip (LandingAngleLoss, LandingSpinLoss) instead of the ported retention multiplier, its normal-speed ramp
        // and the launch-bucketed bite. On the ground: rolling resistance, linear and speed-squared grass drag,
        // and impact spin no longer multiplies turf friction. Every other turf surface keeps the ported ratio of
        // its grass drag and rolling friction to fairway's; the landing loss is the same on all of them, for
        // want of reference shots on any but fairway.
        // Stage 1 of the fit (2026-11-17, Saved/Automation/20261117-145705-5a36a301), on the shots that leave the ground.
        constexpr float TurfLandingAngleLoss = 0.665f;
        constexpr float TurfLandingSpinLoss = 0.464f;
        constexpr float TurfRollingFriction = 0.0469f;
        constexpr float TurfGrassViscosity = 0.0242f;
        constexpr float TurfFormDrag = 0.0086f;
        // Stage 2: a full shot struck into or along the ground (Square's six rows launched below 0 deg, no flight)
        // rolls with these shares of the grass drag. Square rolls such a shot about 1.2 yd per mph, far beyond a ball
        // that lands from a low flight in the same state, so no rule that reads only the ball's state fits both;
        // see GolfPhysics::StruckAlongGround.
        constexpr float TurfStruckViscosityScale = 0.26f;
        constexpr float TurfStruckFormDragScale = 0.69f;

        void RecalibrateTurf(FSurfacePhysicsSettings& Surface, const FSurfacePhysicsSettings& PortedFairway)
        {
            Surface.StruckViscosityScale = TurfStruckViscosityScale;
            Surface.StruckFormDragScale = TurfStruckFormDragScale;
            const float DragRatio = Surface.GrassViscosity / PortedFairway.GrassViscosity;
            Surface.RollingFriction = TurfRollingFriction * Surface.RollingFriction / PortedFairway.RollingFriction;
            Surface.GrassViscosity = TurfGrassViscosity * DragRatio;
            Surface.GrassFormDrag = TurfFormDrag * DragRatio;
            Surface.LandingAngleLoss = TurfLandingAngleLoss;
            Surface.LandingSpinLoss = TurfLandingSpinLoss;
            Surface.RollingSpinScale = 0.0f;
            Surface.SlidingSpinScale = 0.0f;
        }

        TArray<FSurfacePhysicsSettings> BuildCatalog(bool bRecalibratedTurf)
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

            if (bRecalibratedTurf)
            {
                const FSurfacePhysicsSettings PortedFairway = Catalog[static_cast<int32>(ESurface::Fairway)];
                for (const ESurface Turf : {ESurface::Fairway, ESurface::FairwaySoft, ESurface::Rough, ESurface::Firm, ESurface::Fringe})
                {
                    RecalibrateTurf(Catalog[static_cast<int32>(Turf)], PortedFairway);
                }
            }
            return Catalog;
        }
    } // namespace

    const FSurfacePhysicsSettings& SurfaceCatalog::Get(ESurface Surface, bool bRecalibratedTurf)
    {
        static const TArray<FSurfacePhysicsSettings> Ported = BuildCatalog(false);
        static const TArray<FSurfacePhysicsSettings> Recalibrated = BuildCatalog(true);
        const TArray<FSurfacePhysicsSettings>& Catalog = bRecalibratedTurf ? Recalibrated : Ported;
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

    float GreenSpeed::RollingFriction(float StimpFeet, float Viscosity, bool bAirDrag)
    {
        // The stimp fixes the constant deceleration a0 of a rolling ball (a0 + k*v); bisection finds
        // it because the roll distance has no closed-form inverse. Rolling friction = 7*a0 / (5*g).
        const double K = SpeedDragPerSecond(FMath::IsFinite(Viscosity) ? FMath::Max(Viscosity, 0.0f) : GrassViscosity);
        const float Distance = Clamp(StimpFeet) * MetersPerFoot;
        const FVector3f RollVelocity(StimpmeterSpeedMps, 0, 0);
        const auto Aero = FlightAerodynamics::Sample(RollVelocity,
            FVector3f::CrossProduct(FVector3f(0, 1, 0), RollVelocity) / GolfPhysics::Radius,
            1.225f, 1.789e-5f, 1, 1, 0, FFlightProfile());
        const double Q = 5.0 / 7.0 * 0.5 * 1.225 * Aero.DragCoefficient * GolfPhysics::CrossSection / GolfPhysics::Mass;
        auto DistanceWithAir = [&](double A)
        {
            // Integral v/(a + k*v + q*v*v), using the same low-speed aerodynamic cutoff.
            constexpr int32 Intervals = 128;
            const double H = StimpmeterSpeedMps / Intervals;
            double Sum = 0;
            for (int32 I = 0; I <= Intervals; ++I)
            {
                const double V = I * H;
                const double Drag = V >= FlightAerodynamics::MinAerodynamicSpeed ? Q * V * V : 0;
                const double Value = V / (A + K * V + Drag);
                Sum += (I == 0 || I == Intervals ? 1 : I % 2 ? 4 : 2) * Value;
            }
            return Sum * H / 3.0;
        };
        float Low = 0.0f;
        float High = StimpmeterSpeedMps * StimpmeterSpeedMps / (2.0f * Distance);
        for (int32 Iteration = 0; Iteration < SolveIterations; ++Iteration)
        {
            const float Middle = 0.5f * (Low + High);
            if ((bAirDrag ? DistanceWithAir(Middle) : RollMeters(StimpmeterSpeedMps, Middle, K)) > Distance)
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
