// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFlightIntegrationTest, "UOpenGolf.Physics.FlightIntegration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FFlightIntegrationTest::RunTest(const FString& Parameters)
{
    FFlightProfile Profile;
    Profile.bUseRungeKutta4 = true;
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
    Profile.SpinDecayCoefficient = 2e-5f;
    FBallState ZeroSpin = Initial;
    ZeroSpin.Spin = FVector3f::ZeroVector;
    const auto Air = GolfPhysics::StepFlight(ZeroSpin, FFlightEnvironment(), Profile, 0.1f);
    TestTrue(TEXT("Zero spin remains zero"), Air.Spin.IsZero());
    TestTrue(TEXT("Drag slows horizontal motion"), Air.Velocity.X < Initial.Velocity.X);
    FBallPhysicsProfile Ball;
    Ball.Flight = Profile;
    FLaunchData Launch;
    Launch.SpeedMph = 109.3f;
    Launch.VerticalLaunchDeg = 16.7f;
    Launch.BackSpinRpm = 5072;
    Launch.SideSpinRpm = 600;
    const auto Right = ShotSimulator::Simulate(Launch, ESurface::Fairway, Ball);
    Launch.SideSpinRpm = -600;
    const auto Left = ShotSimulator::Simulate(Launch, ESurface::Fairway, Ball);
    TestEqual(TEXT("Mirrored spin preserves carry"), Left.CarryM, Right.CarryM, 0.001f);
    TestEqual(TEXT("Mirrored spin mirrors lateral displacement"), Left.LateralM, -Right.LateralM, 0.001f);
    return true;
}
#endif
