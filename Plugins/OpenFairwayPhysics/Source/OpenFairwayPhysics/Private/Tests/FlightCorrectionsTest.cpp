// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightCorrectionsTest, "UOpenGolf.Physics.FlightCorrections",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightCorrectionsTest::RunTest(const FString& Parameters)
{
    const FFlightEnvironment Air;
    const FFlightProfile Flight;
    // Removing the launch-angle recovery must retain Magnus lift while avoiding an
    // extra boost for identical instantaneous speed/spin at different launch angles.
    for (float Vla : {0.0f, 6.5f, 6.95f, 9.49f, 9.5f, 20.78f})
    {
        TestEqual(TEXT("No empirical low-launch lift amplification by default"),
            FlightAerodynamics::GetLowLaunchLiftScale(Vla, 0.1f, 130000.0f, Flight), 1.0f);
    }
    const FVector3f Spin(30.0f, -50.0f, 400.0f);
    // Fixed-speed, one-step analytic solution checks units independently of trajectory fitting.
    for (float Speed : {0.0f, 0.25f, 33.841f, 67.0f})
    {
        const FBallState State{FVector3f::ZeroVector, FVector3f(Speed, 0.0f, 0.0f), Spin};
        const auto Sample = FlightAerodynamics::Sample(State.Velocity, Spin, Air.AirDensity, Air.AirViscosity,
            Air.DragScale, Air.LiftScale, Air.InitialLaunchAngleDeg, Flight);
        const FBallState ByProfile = GolfPhysics::StepFlight(State, Air, Flight, 0.1f);
        const FBallState BySample = GolfPhysics::StepFlight(State, Air, Sample, 0.1f);
        const FVector3f Expected = Spin * FMath::Exp(-2.0e-5f * Speed / 0.021335f * 0.1f);
        TestTrue(TEXT("Analytical spin decay including low speed"), ByProfile.Spin.Equals(Expected, 0.0001f));
        TestTrue(TEXT("Both overloads agree"), ByProfile.Spin.Equals(BySample.Spin, 0.0001f) && ByProfile.Velocity.Equals(BySample.Velocity, 0.0001f));
        TestTrue(TEXT("Spin direction preserved"), ByProfile.Spin.GetSafeNormal().Equals(Spin.GetSafeNormal(), 0.0001f));
        TestTrue(TEXT("Decay cannot add spin"), ByProfile.Spin.Size() <= Spin.Size());
    }
    const FBallState ZeroSpin{FVector3f::ZeroVector, FVector3f(50.0f, 0.0f, 0.0f), FVector3f::ZeroVector};
    TestTrue(TEXT("Zero spin stays zero"), GolfPhysics::StepFlight(ZeroSpin, Air, Flight, 1.0f).Spin.IsZero());
    const FBallState Slow{FVector3f::ZeroVector, FVector3f(20.0f, 0.0f, 0.0f), Spin};
    const FBallState Fast{FVector3f::ZeroVector, FVector3f(60.0f, 0.0f, 0.0f), Spin};
    TestTrue(TEXT("Faster flight loses more spin"), GolfPhysics::StepFlight(Fast, Air, Flight, 0.1f).Spin.Size() <
                                                        GolfPhysics::StepFlight(Slow, Air, Flight, 0.1f).Spin.Size());

    FLaunchData Launch;
    Launch.SpeedMph = 75.7f;
    Launch.VerticalLaunchDeg = 20.78f;
    Launch.TotalSpinRpm = 4463.0f;
    Launch.SpinAxisDeg = 0.0f;
    Launch.HorizontalLaunchDeg = -0.97f;
    const auto Left = ShotSimulator::Simulate(Launch);
    Launch.HorizontalLaunchDeg = 0.97f;
    const auto Right = ShotSimulator::Simulate(Launch);
    TestTrue(TEXT("Signed shots go to correct sides"), Left.LateralM < 0.0f && Right.LateralM > 0.0f);
    TestEqual(TEXT("Mirrored shot carry"), Left.CarryM, Right.CarryM, 0.001f);
    TestEqual(TEXT("Mirrored shot offline"), Left.LateralM, -Right.LateralM, 0.001f);

    const FBallPhysicsProfile Profile;
    const FBallPhysicsProfile Historical = FBallPhysicsProfile::HistoricalReference();
    FString Key, Matched;
    // Every anchor must reproduce the old calibrated value, including fallback and family rules.
    for (float Speed : {54.0f, 66.0f, 78.5f, 95.0f, 107.5f, 115.0f, 125.0f})
    {
        for (float Vla : {5.0f, 14.0f, 21.5f, 27.5f, 31.5f, 36.5f})
        {
            for (float Rpm : {1250.0f, 3250.0f, 4750.0f, 6500.0f, 8500.0f})
            {
                const auto Actual = Profile.ResolveScaleOverride(Speed, Vla, Rpm, Key, Matched);
                const auto Expected = Historical.ResolveScaleOverride(Speed, Vla, Rpm, Key, Matched);
                TestEqual(TEXT("Anchor drag"), Actual.DragScaleMultiplier, Expected.DragScaleMultiplier, 1e-6f);
                TestEqual(TEXT("Anchor lift"), Actual.LiftScaleMultiplier, Expected.LiftScaleMultiplier, 1e-6f);
            }
        }
    }
    FBallPhysicsProfile Neutral;
    Neutral.RegimeScaleOverrides.Empty();
    const auto NeutralValue = Neutral.ResolveScaleOverride(75.7f, 20.78f, 4463.0f, Key, Matched);
    TestEqual(TEXT("Missing overrides stay neutral"), NeutralValue.DragScaleMultiplier, 1.0f, 1e-6f);
    TestEqual(TEXT("Neutral lift"), NeutralValue.LiftScaleMultiplier, 1.0f, 1e-6f);
    // A synthetic cell gives independently calculable weights for every override field.
    FRegimeScaleOverride Custom{0.6f, 1.2f, 1.4f, 1.6f, 1.8f, 0.2f, 2.0f};
    Neutral.RegimeScaleOverrides.Add(TEXT("I-S1a-V2-P2"), Custom);
    const auto Half = Neutral.ResolveScaleOverride(72.25f, 21.5f, 4750.0f, Key, Matched);
    TestEqual(TEXT("Linear drag midpoint"), Half.DragScaleMultiplier, 0.8f, 1e-6f);
    TestEqual(TEXT("Linear lift midpoint"), Half.LiftScaleMultiplier, 1.1f, 1e-6f);
    TestEqual(TEXT("Linear kinetic midpoint"), Half.KineticFrictionMultiplier, 1.2f, 1e-6f);
    TestEqual(TEXT("Linear rolling midpoint"), Half.RollingFrictionMultiplier, 1.3f, 1e-6f);
    TestEqual(TEXT("Linear viscosity midpoint"), Half.GrassViscosityMultiplier, 1.4f, 1e-6f);
    TestEqual(TEXT("Linear angle midpoint"), Half.CriticalAngleOffsetRadians, 0.1f, 1e-6f);
    TestEqual(TEXT("Linear spinback midpoint"), Half.SpinbackThetaBoostMultiplier, 1.5f, 1e-6f);
    const auto ClampedLow = Profile.ResolveScaleOverride(0.0f, -20.0f, 0.0f, Key, Matched);
    const auto LowAnchor = Profile.ResolveScaleOverride(54.0f, 5.0f, 1250.0f, Key, Matched);
    const auto ClampedHigh = Profile.ResolveScaleOverride(200.0f, 80.0f, 15000.0f, Key, Matched);
    const auto HighAnchor = Profile.ResolveScaleOverride(125.0f, 36.5f, 8500.0f, Key, Matched);
    TestEqual(TEXT("Low coordinates clamp"), ClampedLow.DragScaleMultiplier, LowAnchor.DragScaleMultiplier, 1e-6f);
    TestEqual(TEXT("High coordinates clamp"), ClampedHigh.LiftScaleMultiplier, HighAnchor.LiftScaleMultiplier, 1e-6f);
    ELandingBucket Bucket;
    const auto LandingMidpoint = Profile.ResolveLandingResponse(21.5f, 5625.0f, Bucket);
    TestEqual(TEXT("Landing bite midpoint"), LandingMidpoint.SpinBiteScale, 0.675f, 1e-6f);
    TestEqual(TEXT("Landing speed ramp preserved"), LandingMidpoint.SpeedRampEndMps, 20.0f, 1e-6f);

    // Cross each old boundary, their intersections, and each interpolation knot. Comparing
    // diagonal neighbors crosses all three dimensions and exercises family changes as well.
    for (float Speed : {54.0f, 60.0f, 66.0f, 72.0f, 78.5f, 85.0f, 95.0f, 105.0f, 107.5f, 110.0f, 115.0f, 120.0f, 125.0f})
    {
        for (float Vla : {5.0f, 10.0f, 14.0f, 18.0f, 21.5f, 25.0f, 27.5f, 30.0f, 31.5f, 33.0f, 36.5f})
        {
            for (float Rpm : {1250.0f, 2500.0f, 3250.0f, 4000.0f, 4750.0f, 5500.0f, 6500.0f, 7500.0f, 8500.0f})
            {
                const auto Below = FResolvedShotParams::Resolve(Profile, FAirConditions(), Speed - 0.001f, Vla - 0.001f, Rpm - 0.01f);
                const auto Above = FResolvedShotParams::Resolve(Profile, FAirConditions(), Speed + 0.001f, Vla + 0.001f, Rpm + 0.01f);
                const FString Label = FString::Printf(TEXT("Boundary %.2f mph / %.2f deg / %.0f rpm"), Speed, Vla, Rpm);
                TestEqual(Label + TEXT(" drag continuous"), Below.Environment.DragScale, Above.Environment.DragScale, 0.001f);
                TestEqual(Label + TEXT(" lift continuous"), Below.Environment.LiftScale, Above.Environment.LiftScale, 0.001f);
                TestEqual(Label + TEXT(" bite continuous"), Below.Landing.SpinBiteScale, Above.Landing.SpinBiteScale, 0.001f);
                TestEqual(Label + TEXT(" spinback continuous"), Below.Landing.MaxSpinbackMps, Above.Landing.MaxSpinbackMps, 0.001f);
                TestEqual(Label + TEXT(" ramp start continuous"), Below.Landing.SpeedRampStartMps, Above.Landing.SpeedRampStartMps, 0.001f);
                TestEqual(Label + TEXT(" ramp end continuous"), Below.Landing.SpeedRampEndMps, Above.Landing.SpeedRampEndMps, 0.001f);
                TestTrue(Label + TEXT(" bounded drag"), Above.Regime.DragScaleMultiplier >= 0.54999f && Above.Regime.DragScaleMultiplier <= 1.11001f);
                TestTrue(Label + TEXT(" bounded lift"), Above.Regime.LiftScaleMultiplier >= 0.93999f && Above.Regime.LiftScaleMultiplier <= 1.25001f);
            }
        }
    }
    for (float Boundary : {72.0f, 85.0f})
    {
        Launch.SpinAxisDeg = 20.62f;
        Launch.SpeedMph = Boundary - 0.01f;
        const auto Before = ShotSimulator::Simulate(Launch);
        Launch.SpeedMph = Boundary;
        const auto After = ShotSimulator::Simulate(Launch);
        const float Change = (After.CarryM - Before.CarryM) * YardsPerMeter;
        AddInfo(FString::Printf(TEXT("%.2f -> %.2f mph: %.4f -> %.4f yd (change %.4f yd)"),
            Boundary - 0.01f, Boundary, Before.CarryM * YardsPerMeter, After.CarryM * YardsPerMeter, Change));
        TestTrue(TEXT("Increasing speed has no boundary carry drop"), Change >= 0.0f && Change < 0.1f);
        TestTrue(TEXT("No boundary total-distance jump"), FMath::Abs(After.TotalM - Before.TotalM) * YardsPerMeter < 0.1f);
    }
    return true;
}
#endif
