// SPDX-License-Identifier: MIT
// Diagnostic experiments for the FlightScope screenshot dated 2026-10-14 220535.
// FlightScope values are external comparison data, not golden values from this engine.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace
{
    struct FFlightObservation
    {
        float CarryYd = 0.0f;
        float ApexFt = 0.0f;
        float HangS = 0.0f;
        float LandingSpinRpm = 0.0f;
    };

    // Flight-only diagnostic using production forces and the selected profile.
    FFlightObservation ObserveFlight(const FLaunchData& Launch, const FBallPhysicsProfile& Profile,
        const FAirConditions& Air, float Dt)
    {
        const FShotSpin Spin = ShotSetup::ParseSpin(Launch);
        const FLaunchVectors Vectors = ShotSetup::BuildLaunchVectors(Launch.SpeedMph, Launch.VerticalLaunchDeg,
            Launch.HorizontalLaunchDeg, Spin.BackSpinRpm, Spin.SideSpinRpm);
        const FResolvedShotParams Params = FResolvedShotParams::Resolve(Profile, Air, Launch.SpeedMph,
            Launch.VerticalLaunchDeg, Spin.TotalSpinRpm);
        FBallState State{FVector3f(0.0f, GolfPhysics::Radius, 0.0f), Vectors.Velocity, Vectors.Omega};
        FFlightObservation Result;
        for (int32 Step = 0; Step < static_cast<int32>(15.0f / Dt); ++Step)
        {
            FBallState Next = GolfPhysics::StepFlight(State, Params.Environment, Params.Flight, Dt);
            Result.ApexFt = FMath::Max(Result.ApexFt, Next.Position.Y * FeetPerMeter);
            if (Next.Position.Y <= GolfPhysics::Radius && Next.Velocity.Y < 0.0f)
            {
                const float Fraction = FMath::Clamp((State.Position.Y - GolfPhysics::Radius) /
                                                        (State.Position.Y - Next.Position.Y),
                    0.0f, 1.0f);
                const FVector3f Contact = State.Position + (Next.Position - State.Position) * Fraction;
                Result.CarryYd = FVector3f::DotProduct(Contact, Vectors.ShotDirection) * YardsPerMeter;
                Result.HangS = (Step + Fraction) * Dt;
                Result.LandingSpinRpm = Next.Spin.Size() / GolfPhysics::RadPerRpm;
                break;
            }
            State = Next;
        }
        return Result;
    }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayFlightScopeAnalysisTest, "UOpenGolf.Physics.FlightScopeAnalysis",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayFlightScopeAnalysisTest::RunTest(const FString& Parameters)
{
    FLaunchData Launch;
    Launch.SpeedMph = 75.7f;
    Launch.VerticalLaunchDeg = 20.78f;
    Launch.HorizontalLaunchDeg = -0.97f; // Game HUD; FlightScope screenshot instead has +0.97.
    Launch.TotalSpinRpm = 4463.0f;
    Launch.SpinAxisDeg = 20.62f;
    FBallPhysicsProfile Profile;
    FAirConditions Air;
    const FShotSimulationResult Baseline = ShotSimulator::Simulate(Launch);
    TestTrue(TEXT("Screenshot shot completes"), Baseline.bValid && Baseline.bAtRest);
    TestEqual(TEXT("Screenshot descriptive regime"), Baseline.RegimeKey, FString(TEXT("I-S1b-V2-P2")));
    TestEqual(TEXT("Current scales are interpolated"), Baseline.MatchedRegimeKey, FString(TEXT("interpolated")));
    AddInfo(FString::Printf(TEXT("Game input: carry %.4f yd, total %.4f yd, apex %.4f ft, hang %.4f s, offline %.4f yd"),
        Baseline.CarryM * YardsPerMeter, Baseline.TotalM * YardsPerMeter, Baseline.ApexM * FeetPerMeter,
        Baseline.HangTimeS, Baseline.LateralM * YardsPerMeter));
    const FFlightObservation Flight = ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt);
    TestEqual(TEXT("Diagnostic flight agrees with production carry"), Flight.CarryYd, Baseline.CarryM * YardsPerMeter, 0.001f);
    TestEqual(TEXT("Diagnostic flight agrees with production hang time"), Flight.HangS, Baseline.HangTimeS, 0.001f);

    auto LogFlight = [this](const TCHAR* Label, const FFlightObservation& Value)
    {
        AddInfo(FString::Printf(TEXT("%s: carry %.4f yd, apex %.4f ft, hang %.4f s, landing spin %.1f rpm"),
            Label, Value.CarryYd, Value.ApexFt, Value.HangS, Value.LandingSpinRpm));
    };
    const FFlightObservation Historical = ObserveFlight(Launch, FBallPhysicsProfile::HistoricalReference(), Air, GolfPhysics::SimulationDt);
    TestEqual(TEXT("Historical screenshot reproduced"), Historical.CarryYd, 82.6934f, 0.01f);
    LogFlight(TEXT("Historical model"), Historical);
    FBallPhysicsProfile DecayOnly;
    DecayOnly.bInterpolateCalibration = false;
    const FFlightObservation Decay = ObserveFlight(Launch, DecayOnly, Air, GolfPhysics::SimulationDt);
    TestEqual(TEXT("Isolated decay experiment reproduced"), Decay.CarryYd, 85.1825f, 0.01f);
    LogFlight(TEXT("Speed-dependent spin decay only"), Decay);
    LogFlight(TEXT("Current 120 Hz"), Flight);
    LogFlight(TEXT("Current 960 Hz"), ObserveFlight(Launch, Profile, Air, 1.0f / 960.0f));
    Air.TemperatureF = 77.0f;
    LogFlight(TEXT("77 F only (dry air)"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Air.TemperatureF = 75.0f;
    Profile.DragScaleMultiplier *= 0.9f;
    LogFlight(TEXT("10 percent less drag only"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Profile = FBallPhysicsProfile();
    Profile.LiftScaleMultiplier *= 1.1f;
    LogFlight(TEXT("10 percent more lift only"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Profile = FBallPhysicsProfile();
    Launch.HorizontalLaunchDeg = 0.97f;
    const FShotSimulationResult RightLaunch = ShotSimulator::Simulate(Launch);
    TestEqual(TEXT("Heading rotation preserves carry on level ground"), RightLaunch.CarryM, Baseline.CarryM, 0.001f);
    AddInfo(FString::Printf(TEXT("FlightScope heading (+0.97): carry %.4f yd, offline at rest %.4f yd"),
        RightLaunch.CarryM * YardsPerMeter, RightLaunch.LateralM * YardsPerMeter));

    // Reveal discontinuities around the calibration-bin boundary near this shot.
    for (float Speed : {71.99f, 72.0f, 75.7f, 84.99f, 85.0f})
    {
        Launch.SpeedMph = Speed;
        LogFlight(*FString::Printf(TEXT("Speed %.2f mph"), Speed), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    }
    for (float Re : {49999.0f, 50000.0f, 60000.0f, 65000.0f, 70000.0f, 75000.0f})
    {
        AddInfo(FString::Printf(TEXT("Re %.0f, spin ratio 0.25: Cd %.5f, Cl %.5f"), Re,
            FlightAerodynamics::GetCd(Re, Profile.Flight), FlightAerodynamics::GetCl(Re, 0.25f, Profile.Flight)));
    }

    // Second screenshot (23:05:52): the built-in wood replay. Use decoded packet
    // precision for the game, then the rounded FlightScope inputs separately.
    Launch.SpeedMph = 51.17f / MpsPerMph;
    Launch.VerticalLaunchDeg = 6.95f;
    Launch.HorizontalLaunchDeg = -0.63f;
    Launch.TotalSpinRpm = 1933.0f;
    Launch.SpinAxisDeg = -1.42f;
    Launch.BackSpinRpm = 1932.0f;
    Launch.SideSpinRpm = -48.0f;
    FBallPhysicsProfile BeforeLiftCorrection;
    BeforeLiftCorrection.Flight.LowLaunchLiftRecoveryMax = 1.08f;
    Profile = BeforeLiftCorrection;
    DecayOnly.Flight.LowLaunchLiftRecoveryMax = 1.08f;
    const auto Wood = ShotSimulator::Simulate(Launch, ESurface::Fairway, BeforeLiftCorrection);
    TestTrue(TEXT("Low wood replay completes"), Wood.bValid && Wood.bAtRest);
    TestEqual(TEXT("Wood descriptive regime"), Wood.RegimeKey, FString(TEXT("D-S3-V0-P0")));
    const FResolvedShotParams WoodParams = FResolvedShotParams::Resolve(Profile, Air, Launch.SpeedMph, Launch.VerticalLaunchDeg, 1933.0f);
    AddInfo(FString::Printf(TEXT("Wood regime %s (%s): drag multiplier %.6f, lift multiplier %.6f; net drag %.6f"),
        *WoodParams.RegimeKey, *WoodParams.MatchedRegimeKey, WoodParams.Regime.DragScaleMultiplier,
        WoodParams.Regime.LiftScaleMultiplier, WoodParams.Environment.DragScale));
    LogFlight(TEXT("Wood before lift correction, packet inputs"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    LogFlight(TEXT("Wood historical"), ObserveFlight(Launch, FBallPhysicsProfile::HistoricalReference(), Air, GolfPhysics::SimulationDt));
    LogFlight(TEXT("Wood decay only"), ObserveFlight(Launch, DecayOnly, Air, GolfPhysics::SimulationDt));
    FBallPhysicsProfile BlendOnly = FBallPhysicsProfile::HistoricalReference();
    BlendOnly.bInterpolateCalibration = true;
    LogFlight(TEXT("Wood blending only"), ObserveFlight(Launch, BlendOnly, Air, GolfPhysics::SimulationDt));
    Profile.RegimeScaleOverrides.Reset();
    LogFlight(TEXT("Wood without regime calibration"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Profile = BeforeLiftCorrection;
    Profile.Flight.LowLaunchLiftRecoveryMax = 1.0f;
    LogFlight(TEXT("Wood without low-launch lift boost"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Profile = BeforeLiftCorrection;
    Profile.Flight.MidSpinClBoostMax = 0.0f;
    LogFlight(TEXT("Wood without mid-spin lift boost"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Profile = BeforeLiftCorrection;
    Profile.LiftScaleMultiplier *= 0.9f;
    LogFlight(TEXT("Wood 10 percent less lift"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Profile = BeforeLiftCorrection;
    Profile.DragScaleMultiplier *= 1.1f;
    LogFlight(TEXT("Wood 10 percent more drag"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    Profile = BeforeLiftCorrection;
    LogFlight(TEXT("Wood 960 Hz"), ObserveFlight(Launch, Profile, Air, 1.0f / 960.0f));
    const auto CorrectedWood = ShotSimulator::Simulate(Launch);
    LogFlight(TEXT("Wood gameplay after lift correction"), ObserveFlight(Launch, FBallPhysicsProfile(), Air, GolfPhysics::SimulationDt));
    TestTrue(TEXT("Corrected wood completes"), CorrectedWood.bValid && CorrectedWood.bAtRest);
    TestEqual(TEXT("Corrected production wood carry regression"), CorrectedWood.CarryM * YardsPerMeter, 121.5451f, 0.01f);
    AddInfo(FString::Printf(TEXT("Corrected wood flat fairway: carry %.4f, roll %.4f, total %.4f yd"),
        CorrectedWood.CarryM * YardsPerMeter, (CorrectedWood.TotalM - CorrectedWood.CarryM) * YardsPerMeter, CorrectedWood.TotalM * YardsPerMeter));
    Launch.SpeedMph = 114.5f;
    Launch.BackSpinRpm.Reset();
    Launch.SideSpinRpm.Reset();
    Air.TemperatureF = 77.0f;
    LogFlight(TEXT("Wood FlightScope inputs, 77 F dry air"), ObserveFlight(Launch, Profile, Air, GolfPhysics::SimulationDt));
    AddInfo(FString::Printf(TEXT("Wood flat fairway: carry %.4f, roll %.4f, total %.4f yd. Screenshot range ends on Green; FS medium: 119.8 / 38.7 / 158.5."),
        Wood.CarryM * YardsPerMeter, (Wood.TotalM - Wood.CarryM) * YardsPerMeter, Wood.TotalM * YardsPerMeter));
    return true;
}

#endif
