// SPDX-License-Identifier: MIT
// The gameplay flight model (SpinRatioV1) and the Pro V1 comparison model: coefficient laws, agreement with
// published measurements, and whole flights against the launch-monitor references. Sources: Docs/CarryAudit.md.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace FlightModelTestDetail
{
    // 75 F, sea level, dry air.
    constexpr float Density = 1.1883f;
    constexpr float Viscosity = 1.8318e-5f;

    /** The coefficients at a Reynolds number and spin ratio, for a ball moving along +X. */
    FFlightAerodynamicsSample SampleAt(const FFlightProfile& Flight, float Re, float SpinRatio, float LaunchDeg = 0.0f, float Scale = 1.0f)
    {
        const float Speed = Re * Viscosity / (Density * 2.0f * GolfPhysics::Radius);
        const FVector3f Spin(0.0f, 0.0f, SpinRatio * Speed / GolfPhysics::Radius);
        return FlightAerodynamics::Sample(FVector3f(Speed, 0.0f, 0.0f), Spin, Density, Viscosity, Scale, Scale, LaunchDeg, Flight);
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
        Air.PressurePa = 14.69f * 6894.757f;
        return Air;
    }
} // namespace FlightModelTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightModelTest, "UOpenGolf.Physics.FlightModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightModelTest::RunTest(const FString& Parameters)
{
    using namespace FlightModelTestDetail;
    const FFlightProfile Gameplay = FBallPhysicsProfile().Flight;
    const FFlightProfile ProV1 = FBallPhysicsProfile::ProV1Reference().Flight;
    TestTrue(TEXT("Gameplay uses the spin-ratio model"), Gameplay.Model == EFlightModel::SpinRatioV1);
    TestTrue(TEXT("Gameplay integrates with RK4"), Gameplay.bUseRungeKutta4);

    // SpinRatioV1 (Private/OpenFairway/SpinRatioData.h): curves in spin ratio W plus the low-Reynolds drag rise.
    TestEqual(TEXT("No spin, no lift"), SampleAt(Gameplay, 150000, 0.0f).LiftCoefficient, 0.0f);
    TestEqual(TEXT("Lift rises linearly from zero spin to the first knot"), SampleAt(Gameplay, 150000, 0.03f).LiftCoefficient, 0.0651447f, 1e-4f);
    TestEqual(TEXT("Drag is held below the first knot"), SampleAt(Gameplay, 150000, 0.0f).DragCoefficient, 0.1895485f, 1e-4f);
    TestEqual(TEXT("Table drag at W 0.22"), SampleAt(Gameplay, 150000, 0.22f).DragCoefficient, 0.2985f, 1e-4f);
    TestEqual(TEXT("Table lift at W 0.22"), SampleAt(Gameplay, 150000, 0.22f).LiftCoefficient, 0.2852f, 1e-4f);
    float PreviousLift = 0.0f;
    float PreviousDrag = 0.0f;
    for (const float W : {0.0f, 0.05f, 0.06f, 0.08f, 0.1f, 0.12f, 0.15f, 0.16f, 0.2f, 0.4f, 0.8f, 1.2f})
    {
        const FFlightAerodynamicsSample S = SampleAt(Gameplay, 150000, W);
        TestTrue(FString::Printf(TEXT("Lift never falls with spin (W %.2f)"), W), S.LiftCoefficient >= PreviousLift);
        TestTrue(FString::Printf(TEXT("Drag never falls with spin (W %.2f)"), W), S.DragCoefficient >= PreviousDrag);
        TestTrue(TEXT("Drag at least the measured modern-ball minimum"), S.DragCoefficient >= 0.17f);
        PreviousLift = S.LiftCoefficient;
        PreviousDrag = S.DragCoefficient;
    }
    // Below Re 100k drag rises by 0.68 of the measured Pro V1 drag crisis (2020 Fig 3); above it, Reynolds does not matter.
    TestEqual(TEXT("Drag crisis at Re 65k"), SampleAt(Gameplay, 65000, .2f).DragCoefficient - SampleAt(Gameplay, 150000, .2f).DragCoefficient,
        0.6838f * (0.33f - 0.215f), 1e-4f);
    TestEqual(TEXT("Drag continuous where the crisis starts"),
        SampleAt(Gameplay, 99999, .2f).DragCoefficient, SampleAt(Gameplay, 100001, .2f).DragCoefficient, 1e-4f);
    TestEqual(TEXT("Drag independent of Reynolds above the crisis"),
        SampleAt(Gameplay, 120000, .2f).DragCoefficient, SampleAt(Gameplay, 200000, .2f).DragCoefficient, 1e-6f);
    // Fitted to launch-monitor flights, yet the lift matches the measured Pro V1 (Lyu et al. 2018 Fig 4a).
    TestEqual(TEXT("Lift agrees with the measured Pro V1 at W 0.18"), SampleAt(Gameplay, 140000, .18f).LiftCoefficient, .25f, .025f);
    TestEqual(TEXT("Lift agrees with the measured Pro V1 at W 0.32"), SampleAt(Gameplay, 140000, .32f).LiftCoefficient, .32f, .025f);
    TestFalse(TEXT("A typical full shot is inside the fitted range"), SampleAt(Gameplay, 135000, .2f).bOutsideMeasuredCoverage);
    TestTrue(TEXT("A very fast ball is outside the fitted range"), SampleAt(Gameplay, 260000, .05f).bOutsideMeasuredCoverage);
    for (const float Re : {40000.0f, 90000.0f, 200000.0f})
    {
        const FFlightAerodynamicsSample A = SampleAt(Gameplay, Re, .15f);
        const FFlightAerodynamicsSample B = SampleAt(Gameplay, Re, .15f, 35.0f, 4.0f);
        TestEqual(TEXT("Gameplay: launch angle and scales cannot change lift"), A.LiftCoefficient, B.LiftCoefficient);
        TestEqual(TEXT("Gameplay: launch angle and scales cannot change drag"), A.DragCoefficient, B.DragCoefficient);
        TestEqual(TEXT("Gameplay: lift depends on the spin ratio alone"), A.LiftCoefficient, SampleAt(Gameplay, 120000, .15f).LiftCoefficient, 1e-6f);
    }

    // Pro V1 (comparison only): independent visual checks against the published figures, with extraction uncertainty.
    TestEqual(TEXT("2020 Fig 3 V1 drag near 50k"), SampleAt(ProV1, 50000, .15f).DragCoefficient, .49f, .02f);
    TestEqual(TEXT("2020 Fig 3 V1 drag near 150k"), SampleAt(ProV1, 150000, .1f).DragCoefficient, .17f, .02f);
    TestEqual(TEXT("2020 Fig 4 negative lift near 60k"), SampleAt(ProV1, 60000, .17f).LiftCoefficient, -.09f, .015f);
    TestEqual(TEXT("2020 Fig 5 V1 lift near 65k"), SampleAt(ProV1, 65000, .10f).LiftCoefficient, -.025f, .015f);
    TestEqual(TEXT("2018 Fig 4a V1 high-speed lift"), SampleAt(ProV1, 140000, .18f).LiftCoefficient, .25f, .02f);
    TestEqual(TEXT("Pro V1: zero spin has no lift"), SampleAt(ProV1, 100000, 0.0f).LiftCoefficient, 0.0f);
    TestTrue(TEXT("Pro V1 drag at high spin is outside its measurement"), SampleAt(ProV1, 135000, .24f).bOutsideMeasuredCoverage);
    for (const float Re : {0.0f, 30000.0f, 50000.0f, 60000.0f, 65000.0f, 70000.0f, 75000.0f, 90000.0f, 200000.0f, 300000.0f})
    {
        const FFlightAerodynamicsSample A = SampleAt(ProV1, Re, .15f);
        const FFlightAerodynamicsSample B = SampleAt(ProV1, Re, .15f, 35.0f, 4.0f);
        TestEqual(TEXT("Pro V1: launch angle and scales cannot change lift"), A.LiftCoefficient, B.LiftCoefficient);
        TestEqual(TEXT("Pro V1: launch angle and scales cannot change drag"), A.DragCoefficient, B.DragCoefficient);
        TestTrue(TEXT("Pro V1: nonnegative drag"), A.DragCoefficient >= 0.0f);
        if (Re > 0.0f)
        {
            TestEqual(TEXT("Pro V1: lift continuous at Re knots"),
                SampleAt(ProV1, Re - 1.0f, .15f).LiftCoefficient, SampleAt(ProV1, Re + 1.0f, .15f).LiftCoefficient, 1e-4f);
        }
    }

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

    // Calm air has no preferred heading, and the legacy calibration cannot reach the flight.
    FLaunchData Hybrid = Launch(109.3f, 16.7f, 5072.0f);
    const FShotSimulationResult Straight = ShotSimulator::Simulate(Hybrid);
    Hybrid.HorizontalLaunchDeg = 85.0f;
    const FShotSimulationResult Turned = ShotSimulator::Simulate(Hybrid);
    TestEqual(TEXT("Calm heading invariance"), Turned.CarryM, Straight.CarryM, .001f);
    FBallPhysicsProfile Modified;
    Modified.RegimeScaleOverrides.Reset();
    Modified.LiftScaleMultiplier = 5.0f;
    Modified.DragScaleMultiplier = 3.0f;
    TestEqual(TEXT("Flight independent of the regime table and global scales"),
        ShotSimulator::Simulate(Hybrid, ESurface::Fairway, Modified).CarryM, Turned.CarryM, .001f);
    return true;
}

#endif
