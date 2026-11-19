// SPDX-License-Identifier: MIT
// The RK4 flight step: ballistics in a vacuum, spin decay, symmetry and continuity of whole flights.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightIntegrationTest, "UOpenGolf.Physics.FlightIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightIntegrationTest::RunTest(const FString& Parameters)
{
    FFlightProfile Profile;
    FFlightEnvironment Vacuum;
    Vacuum.AirDensity = 0;
    Profile.SpinDecayCoefficient = 0;
    const FBallState Initial{FVector3f(0, 2, 0), FVector3f(40, 20, 3), FVector3f(0, 0, 400)};
    const auto Next = GolfPhysics::StepFlight(Initial, Vacuum, Profile, 0.5f);
    TestTrue(TEXT("Vacuum position matches ballistic equation"),
        Next.Position.Equals(Initial.Position + Initial.Velocity * 0.5f + FVector3f(0, -0.5f * GolfPhysics::Gravity * 0.25f, 0), 1e-5f));
    TestTrue(TEXT("Vacuum velocity matches ballistic equation"),
        Next.Velocity.Equals(Initial.Velocity + FVector3f(0, -GolfPhysics::Gravity * 0.5f, 0), 1e-5f));
    TestTrue(TEXT("No torque preserves spin"), Next.Spin.Equals(Initial.Spin, 1e-5f));

    // Spin decays in proportion to speed (Smits and Smith) and keeps its axis.
    const FFlightEnvironment Air;
    const FFlightProfile Flight;
    const FVector3f Spin(30.0f, -50.0f, 400.0f);
    for (float Speed : {0.0f, 0.25f, 33.841f, 67.0f})
    {
        const FBallState State{FVector3f::ZeroVector, FVector3f(Speed, 0.0f, 0.0f), Spin};
        const FBallState After = GolfPhysics::StepFlight(State, Air, Flight, 0.1f);
        TestTrue(TEXT("Spin direction preserved"), After.Spin.GetSafeNormal().Equals(Spin.GetSafeNormal(), 0.0001f));
        TestTrue(TEXT("Decay cannot add spin"), After.Spin.Size() <= Spin.Size());
    }
    FBallState ZeroSpin = Initial;
    ZeroSpin.Spin = FVector3f::ZeroVector;
    const auto InAir = GolfPhysics::StepFlight(ZeroSpin, Air, Flight, 0.1f);
    TestTrue(TEXT("Zero spin remains zero"), InAir.Spin.IsZero());
    TestTrue(TEXT("Drag slows horizontal motion"), InAir.Velocity.X < Initial.Velocity.X);
    const FBallState Slow{FVector3f::ZeroVector, FVector3f(20.0f, 0.0f, 0.0f), Spin};
    const FBallState Fast{FVector3f::ZeroVector, FVector3f(60.0f, 0.0f, 0.0f), Spin};
    TestTrue(TEXT("Faster flight loses more spin"),
        GolfPhysics::StepFlight(Fast, Air, Flight, 0.1f).Spin.Size() < GolfPhysics::StepFlight(Slow, Air, Flight, 0.1f).Spin.Size());

    // Mirrored launches and mirrored spin give mirrored flights.
    FLaunchData Launch;
    Launch.SpeedMph = 109.3f;
    Launch.VerticalLaunchDeg = 16.7f;
    Launch.BackSpinRpm = 5072;
    Launch.SideSpinRpm = 600;
    const auto RightSpin = ShotSimulator::Simulate(Launch);
    Launch.SideSpinRpm = -600;
    const auto LeftSpin = ShotSimulator::Simulate(Launch);
    TestEqual(TEXT("Mirrored spin preserves carry"), LeftSpin.CarryM, RightSpin.CarryM, 0.001f);
    TestEqual(TEXT("Mirrored spin mirrors lateral displacement"), LeftSpin.LateralM, -RightSpin.LateralM, 0.001f);

    FLaunchData Pitch;
    Pitch.SpeedMph = 75.7f;
    Pitch.VerticalLaunchDeg = 20.78f;
    Pitch.TotalSpinRpm = 4463.0f;
    Pitch.SpinAxisDeg = 0.0f;
    Pitch.HorizontalLaunchDeg = -0.97f;
    const auto Left = ShotSimulator::Simulate(Pitch);
    Pitch.HorizontalLaunchDeg = 0.97f;
    const auto Right = ShotSimulator::Simulate(Pitch);
    TestTrue(TEXT("Signed shots go to correct sides"), Left.LateralM < 0.0f && Right.LateralM > 0.0f);
    TestEqual(TEXT("Mirrored shot carry"), Left.CarryM, Right.CarryM, 0.001f);
    TestEqual(TEXT("Mirrored shot offline"), Left.LateralM, -Right.LateralM, 0.001f);
    // Flight decay leaves less spin at first impact than at launch, but not none.
    TestTrue(TEXT("Landing spin recorded"), Left.LandingSpinRpm > 0.0f && Left.LandingSpinRpm < 4463.0f);

    // No speed has a step in carry or total: the flight reads the ball's air speed and spin alone.
    Pitch.SpinAxisDeg = 20.62f;
    for (float Boundary : {72.0f, 85.0f})
    {
        Pitch.SpeedMph = Boundary - 0.01f;
        const auto Before = ShotSimulator::Simulate(Pitch);
        Pitch.SpeedMph = Boundary;
        const auto After = ShotSimulator::Simulate(Pitch);
        const float Change = (After.CarryM - Before.CarryM) * YardsPerMeter;
        AddInfo(FString::Printf(TEXT("%.2f -> %.2f mph: %.4f -> %.4f yd (change %.4f yd)"),
            Boundary - 0.01f, Boundary, Before.CarryM * YardsPerMeter, After.CarryM * YardsPerMeter, Change));
        TestTrue(TEXT("Increasing speed has no carry drop"), Change >= 0.0f && Change < 0.1f);
        TestTrue(TEXT("No total-distance jump"), FMath::Abs(After.TotalM - Before.TotalM) * YardsPerMeter < 0.1f);
    }
    return true;
}
#endif
