// SPDX-License-Identifier: MIT
// The gameplay flight model (the spin-ratio table): coefficient laws, agreement with published measurements of a modern
// ball, and whole flights against the reference flights it was fitted to. Sources: Docs/Accuracy.md.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "../OpenFairway/BallData.h"
#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace FlightModelTestDetail
{
    // 75 F, sea level, dry air.
    constexpr float Density = 1.1883f;
    constexpr float Viscosity = 1.8318e-5f;

    /** The coefficients at a Reynolds number and spin ratio, for a ball moving along +X. */
    FFlightAerodynamicsSample SampleAt(float Re, float SpinRatio)
    {
        const float Speed = Re * Viscosity / (Density * 2.0f * GolfPhysics::Radius);
        const FVector3f Spin(0.0f, 0.0f, SpinRatio * Speed / GolfPhysics::Radius);
        return FlightAerodynamics::Sample(FVector3f(Speed, 0.0f, 0.0f), Spin, Density, Viscosity);
    }

    FLaunchData Launch(float SpeedMph, float LaunchDeg, float SpinRpm)
    {
        FLaunchData Data;
        Data.SpeedMph = SpeedMph;
        Data.VerticalLaunchDeg = LaunchDeg;
        Data.BackSpinRpm = SpinRpm;
        Data.SideSpinRpm = 0.0f;
        return Data;
    }

    /** The air the trajectory-calculator reference rows were computed in: 77 F, 50% humidity, 14.69 psi. */
    FAirConditions CalculatorAir()
    {
        FAirConditions Air;
        Air.TemperatureF = 77.0f;
        Air.RelativeHumidity = 0.5f;
        Air.PressurePa = 14.69f * PaPerPsi;
        return Air;
    }
} // namespace FlightModelTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightModelTest, "UOpenGolf.Physics.FlightModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightModelTest::RunTest(const FString& Parameters)
{
    using namespace FlightModelTestDetail;
    // The flight table (Private/OpenFairway/SpinRatioData.h): curves in spin ratio W plus the low-Reynolds drag rise.
    TestEqual(TEXT("No spin, no lift"), SampleAt(150000, 0.0f).LiftCoefficient, 0.0f);
    TestEqual(TEXT("Lift rises linearly from zero spin to the first knot"), SampleAt(150000, 0.03f).LiftCoefficient, 0.0651447f, 1e-4f);
    TestEqual(TEXT("Drag is held below the first knot"), SampleAt(150000, 0.0f).DragCoefficient, 0.1895485f, 1e-4f);
    TestEqual(TEXT("Table drag at W 0.22"), SampleAt(150000, 0.22f).DragCoefficient, 0.2985f, 1e-4f);
    TestEqual(TEXT("Table lift at W 0.22"), SampleAt(150000, 0.22f).LiftCoefficient, 0.2852f, 1e-4f);
    float PreviousLift = 0.0f;
    float PreviousDrag = 0.0f;
    for (const float W : {0.0f, 0.05f, 0.06f, 0.08f, 0.1f, 0.12f, 0.15f, 0.16f, 0.2f, 0.4f, 0.8f, 1.2f})
    {
        const FFlightAerodynamicsSample S = SampleAt(150000, W);
        TestTrue(FString::Printf(TEXT("Lift never falls with spin (W %.2f)"), W), S.LiftCoefficient >= PreviousLift);
        TestTrue(FString::Printf(TEXT("Drag never falls with spin (W %.2f)"), W), S.DragCoefficient >= PreviousDrag);
        TestTrue(TEXT("Drag at least the measured modern-ball minimum"), S.DragCoefficient >= 0.17f);
        PreviousLift = S.LiftCoefficient;
        PreviousDrag = S.DragCoefficient;
    }
    // Below Re 100k drag rises by 0.68 of the measured modern-ball drag crisis (Lyu et al. 2020 Fig 3); above it, Reynolds does not matter.
    TestEqual(TEXT("Drag crisis at Re 65k"), SampleAt(65000, .2f).DragCoefficient - SampleAt(150000, .2f).DragCoefficient,
        0.6838f * (0.33f - 0.215f), 1e-4f);
    TestEqual(TEXT("Drag continuous where the crisis starts"),
        SampleAt(99999, .2f).DragCoefficient, SampleAt(100001, .2f).DragCoefficient, 1e-4f);
    TestEqual(TEXT("Drag independent of Reynolds above the crisis"),
        SampleAt(120000, .2f).DragCoefficient, SampleAt(200000, .2f).DragCoefficient, 1e-6f);
    // Fitted to reference flights, yet the lift matches the measured modern ball (Lyu et al. 2018 Fig 4a).
    TestEqual(TEXT("Lift agrees with the measured ball at W 0.18"), SampleAt(140000, .18f).LiftCoefficient, .25f, .025f);
    TestEqual(TEXT("Lift agrees with the measured ball at W 0.32"), SampleAt(140000, .32f).LiftCoefficient, .32f, .025f);
    TestFalse(TEXT("A typical full shot is inside the fitted range"), SampleAt(135000, .2f).bOutsideMeasuredCoverage);
    TestTrue(TEXT("A very fast ball is outside the fitted range"), SampleAt(260000, .05f).bOutsideMeasuredCoverage);
    for (const float Re : {40000.0f, 90000.0f, 200000.0f})
    {
        TestEqual(TEXT("Lift depends on the spin ratio alone"), SampleAt(Re, .15f).LiftCoefficient, SampleAt(120000, .15f).LiftCoefficient, 1e-6f);
    }

    // The measured drag table the crisis comes from (BallData.h), against the published figure.
    TestEqual(TEXT("Measured drag near Re 50k"), BallData::DragAt(50000.0f), .49f, .02f);
    TestEqual(TEXT("Measured drag near Re 150k"), BallData::DragAt(150000.0f), .17f, .02f);
    TestEqual(TEXT("Measured drag held below the table"), BallData::DragAt(20000.0f), BallData::DragAt(50000.0f));
    TestEqual(TEXT("Measured drag held above the table"), BallData::DragAt(300000.0f), BallData::DragAt(200000.0f));

    // Whole flights against the reference carries the model was fitted to (flat fairway, no wind; the full sets live in
    // the local reference data, see Assets/Data/README.md). Calculator carries take priority for driver launches; conflicting tour averages have wider tolerances. The sources differ by
    // several yards for the same launch, so these guard the fit rather than pin it: a driver once came up 46 yd short.
    struct FReference
    {
        const TCHAR* Name;
        FLaunchData Launch;
        FAirConditions Air;
        float CarryYd;
        float ToleranceYd = 6.0f;
    };
    const FReference References[] = {
        {TEXT("Tour-average driver, 167 mph"), Launch(167.0f, 10.9f, 2686.0f), FAirConditions(), 275.0f, 8.0f},
        {TEXT("Tour-average driver, 140 mph"), Launch(140.0f, 13.2f, 2611.0f), FAirConditions(), 218.0f, 11.0f},
        {TEXT("Tour-average iron, 132 mph"), Launch(132.0f, 12.1f, 5361.0f), FAirConditions(), 194.0f},
        {TEXT("Tour-average wedge, 102 mph"), Launch(102.0f, 24.2f, 9304.0f), FAirConditions(), 136.0f},
        {TEXT("Tour-average iron, 109 mph"), Launch(109.0f, 17.1f, 5943.0f), FAirConditions(), 152.0f},
        {TEXT("Calculator driver, 150 mph"), Launch(150.0f, 12.5f, 2335.0f), CalculatorAir(), 249.7f, 2.0f},
        {TEXT("Calculator driver, 120 mph"), Launch(120.0f, 15.0f, 3200.0f), CalculatorAir(), 184.1f, 2.0f},
        {TEXT("Launch-monitor hybrid, 109.3 mph"), Launch(109.3f, 16.7f, 5072.0f), FAirConditions(), 157.0f},
    };
    for (const FReference& Reference : References)
    {
        const FShotSimulationResult Result = ShotSimulator::Simulate(Reference.Launch, ESurface::Fairway, FBallPhysicsProfile(), Reference.Air);
        const float CarryYd = Result.CarryM * YardsPerMeter;
        AddInfo(FString::Printf(TEXT("%s: carry %.1f yd (reference %.1f), apex %.1f ft, hang %.2f s"), Reference.Name,
            CarryYd, Reference.CarryYd, Result.ApexM * FeetPerMeter, Result.HangTimeS));
        TestEqual(FString(Reference.Name) + TEXT(" carry within reference tolerance"), CarryYd, Reference.CarryYd, Reference.ToleranceYd);
    }

    // Calm air has no preferred heading, and nothing on the ground reaches the flight.
    FLaunchData Hybrid = Launch(109.3f, 16.7f, 5072.0f);
    const FShotSimulationResult Straight = ShotSimulator::Simulate(Hybrid);
    Hybrid.HorizontalLaunchDeg = 85.0f;
    const FShotSimulationResult Turned = ShotSimulator::Simulate(Hybrid);
    TestEqual(TEXT("Calm heading invariance"), Turned.CarryM, Straight.CarryM, .001f);
    FBallPhysicsProfile Modified;
    Modified.GroundDragScale = 3.0f;
    Modified.KineticFrictionMultiplier = 2.0f;
    TestEqual(TEXT("Flight independent of the ground scales"),
        ShotSimulator::Simulate(Hybrid, ESurface::Fairway, Modified).CarryM, Turned.CarryM, .001f);
    return true;
}

#endif
