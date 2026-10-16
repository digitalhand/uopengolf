// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairwayBall.h"
#include "UObject/Package.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayWindTest, "UOpenGolf.Physics.Wind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
    FLaunchData WindIron(float HorizontalLaunchDeg = 0.0f)
    {
        FLaunchData Launch;
        Launch.SpeedMph = 120.0f;
        Launch.VerticalLaunchDeg = 16.3f;
        Launch.HorizontalLaunchDeg = HorizontalLaunchDeg;
        Launch.TotalSpinRpm = 7097.0f;
        Launch.SpinAxisDeg = 0.0f;
        return Launch;
    }

    FAirConditions Wind(float SpeedMph, float FromDeg)
    {
        FAirConditions Air;
        Air.WindSpeedMph = SpeedMph;
        Air.WindFromDeg = FromDeg;
        return Air;
    }
} // namespace

bool FOpenFairwayWindTest::RunTest(const FString& Parameters)
{
    const FBallPhysicsProfile Profile;
    // Calm air is the default; parity with the pre-wind engine is pinned by the Fixtures and
    // the FlightScopeAnalysis regression carries.
    const FShotSimulationResult Calm = ShotSimulator::Simulate(WindIron(), ESurface::Fairway, Profile, FAirConditions());
    FAirConditions DirectionOnly;
    DirectionOnly.WindFromDeg = 135.0f;
    const FShotSimulationResult NoSpeed = ShotSimulator::Simulate(WindIron(), ESurface::Fairway, Profile, DirectionOnly);
    TestTrue(TEXT("Zero wind speed ignores the direction"), NoSpeed.CarryM == Calm.CarryM && NoSpeed.LateralM == Calm.LateralM);

    constexpr float FiveYardsM = 5.0f / YardsPerMeter;
    constexpr float ThreeYardsM = 3.0f / YardsPerMeter;
    const FShotSimulationResult Headwind = ShotSimulator::Simulate(WindIron(), ESurface::Fairway, Profile, Wind(10.0f, 0.0f));
    const FShotSimulationResult Tailwind = ShotSimulator::Simulate(WindIron(), ESurface::Fairway, Profile, Wind(10.0f, 180.0f));
    AddInfo(FString::Printf(TEXT("Carry calm %.1f yd, 10 mph head %.1f yd, 10 mph tail %.1f yd"), Calm.CarryM * YardsPerMeter,
        Headwind.CarryM * YardsPerMeter, Tailwind.CarryM * YardsPerMeter));
    TestTrue(TEXT("Headwind shortens carry"), Headwind.CarryM < Calm.CarryM - FiveYardsM);
    TestTrue(TEXT("Tailwind lengthens carry"), Tailwind.CarryM > Calm.CarryM + ThreeYardsM);

    // Wind from the right pushes the ball left (negative lateral) whichever way it starts.
    for (float Hla : {2.0f, -2.0f})
    {
        const FShotSimulationResult Still = ShotSimulator::Simulate(WindIron(Hla), ESurface::Fairway, Profile);
        const FShotSimulationResult Cross = ShotSimulator::Simulate(WindIron(Hla), ESurface::Fairway, Profile, Wind(10.0f, 90.0f));
        TestTrue(FString::Printf(TEXT("Wind from the right moves the ball left at HLA %+.0f"), Hla), Cross.LateralM < Still.LateralM - 1.0f);
    }

    // The actor's wind is fixed to the world while AimYawDeg turns the shot, so a prediction
    // for a ball aimed 90 degrees right sees a world headwind as wind from the shot's left.
    AOpenFairwayBall* Ball = NewObject<AOpenFairwayBall>(GetTransientPackage());
    Ball->AimYawDeg = 90.0f;
    Ball->WindSpeedMph = 10.0f;
    Ball->WindFromDeg = 0.0f;
    FOpenFairwayLaunch AimedLaunch;
    AimedLaunch.BallSpeedMph = 120.0f;
    AimedLaunch.VerticalLaunchDeg = 16.3f;
    AimedLaunch.bHasTotalSpin = true;
    AimedLaunch.TotalSpinRpm = 7097.0f;
    AimedLaunch.bHasSpinAxis = true;
    AimedLaunch.SpinAxisDeg = 0.0f;
    const FShotSimulationResult Predicted = Ball->PredictShot(AimedLaunch, EOpenFairwaySurface::Fairway);
    const FShotSimulationResult ShotFrame = ShotSimulator::Simulate(AOpenFairwayBall::ToLaunchData(AimedLaunch), ESurface::Fairway, Ball->Profile, Wind(10.0f, -90.0f));
    TestEqual(TEXT("Aimed prediction keeps world wind: carry"), Predicted.CarryM, ShotFrame.CarryM, 1e-4f);
    TestEqual(TEXT("Aimed prediction keeps world wind: lateral"), Predicted.LateralM, ShotFrame.LateralM, 1e-4f);
    Ball->MarkAsGarbage();

    // A ball moving with the air has no airspeed: gravity only, never NaN.
    FFlightEnvironment Environment;
    Environment.Wind = FVector3f(10.0f, 0.0f, 3.0f);
    const FBallState Drifting{FVector3f::ZeroVector, Environment.Wind, FVector3f(0.0f, 0.0f, 300.0f)};
    const FBallState Next = GolfPhysics::StepFlight(Drifting, Environment, FFlightProfile(), 0.1f);
    TestTrue(TEXT("Still air relative to the ball stays finite"), !Next.Velocity.ContainsNaN() && !Next.Spin.ContainsNaN());
    TestTrue(TEXT("Still air relative to the ball is gravity only"),
        Next.Velocity.Equals(Environment.Wind + FVector3f(0.0f, -GolfPhysics::Gravity * 0.1f, 0.0f), 1e-5f));
    return true;
}
#endif
