// SPDX-License-Identifier: MIT
// Gameplay regression pins: headless shots on the gameplay profile, recorded from the build that produced the
// shot-library results in Plugins/OpenFairwayPhysics/Docs/Accuracy.md. A refactor that changes any of them changed
// the physics. Refresh them only for an intended physics change, and say so in Docs/PhasesStatus.md.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace OpenFairwayGameplayPins
{
    struct FPin
    {
        const TCHAR* Name;
        ESurface Surface;
        float Speed, Vla, Hla, BackSpin, SideSpin;
        /** Wind speed (mph) and the direction it blows from (deg); 0 is calm. */
        float WindMph, WindFromDeg;
        /** 0 is the standard air (75 F, sea level, dry); 1 is hot, high and humid. */
        int32 Air;
        /** Ground slope (percent) falling to the right of the target line. */
        float SlopePercent;
        float CarryYd, TotalYd, LateralYd, ApexFt, HangS, LandingDeg;
        int32 Bounces;
    };

    // clang-format off
    const FPin Pins[] = {
        {TEXT("drive"), ESurface::Fairway, 150.0f, 12.5f, 1.5f, 2300.0f, -400.0f, 0, 0, 0, 0, 248.0529f, 267.2144f, -12.4602f, 85.1776f, 6.2527f, 35.0918f, 2},
        {TEXT("wood_low"), ESurface::Fairway, 114.46167f, 6.949297f, -0.626495f, 1931.9213f, -47.751236f, 0, 0, 0, 0, 122.3703f, 164.6198f, -2.7708f, 15.3271f, 2.8246f, 12.1328f, 1},
        {TEXT("approach_mid_iron"), ESurface::Fairway, 93.9f, 22.9f, -2.6f, 5375.0f, 0.0f, 0, 0, 0, 0, 123.9325f, 126.6353f, -5.7446f, 72.5921f, 5.1499f, 46.8743f, 1},
        {TEXT("wedge"), ESurface::Fairway, 66.4f, 23.2f, -1.4f, 6399.0f, 793.0f, 0, 0, 0, 0, 69.4685f, 73.9475f, 0.5807f, 30.9239f, 3.2611f, 35.9741f, 1},
        {TEXT("chip"), ESurface::Fairway, 24.676934f, 17.938442f, 20.353489f, 3203.8918f, 0.0f, 0, 0, 0, 0, 8.1587f, 13.5043f, 4.6969f, 2.1130f, 0.7412f, 18.8720f, 1},
        {TEXT("bump"), ESurface::Fairway, 78.272453f, 5.5726221f, -0.8502867f, 1700.36f, 75.733231f, 0, 0, 0, 0, 38.8951f, 81.7196f, -0.7849f, 3.1626f, 1.1099f, 6.5270f, 1},
        {TEXT("checked"), ESurface::Fairway, 75.054704f, 38.503029f, 1.3455229f, 10500.78f, 699.26617f, 0, 0, 0, 0, 82.0930f, 82.4033f, 3.8616f, 82.2091f, 4.9946f, 55.9247f, 1},
        {TEXT("flop"), ESurface::Fairway, 68.054704f, 45.0f, 0.4455229f, 12000.0f, 140.26617f, 0, 0, 0, 0, 66.5654f, 66.7083f, 0.7569f, 79.0689f, 4.7696f, 58.7899f, 1},
        {TEXT("low_runner"), ESurface::Fairway, 91.781736f, 5.0f, -0.8898518f, 2108.1736f, 612.31128f, 0, 0, 0, 0, 57.9195f, 104.4457f, 3.3865f, 4.4816f, 1.4676f, 6.5956f, 1},
        {TEXT("long_iron"), ESurface::Fairway, 102.1f, 17.4f, 1.5f, 5266.0f, 1151.0f, 0, 0, 0, 0, 137.7670f, 142.6565f, 16.9965f, 64.3444f, 5.0816f, 42.1367f, 1},
        {TEXT("driver_pull"), ESurface::Fairway, 124.0f, 9.4f, -9.5f, 2107.923f, -973.19f, 0, 0, 0, 0, 161.6472f, 193.7823f, -58.1090f, 32.1138f, 3.9181f, 19.9750f, 1},
        {TEXT("pitch"), ESurface::Fairway, 82.27544f, 26.4555f, -3.1465683f, 4912.2999f, 580.31595f, 0, 0, 0, 0, 101.7366f, 104.4416f, -1.6179f, 62.1955f, 4.6109f, 46.5491f, 1},
        {TEXT("topped"), ESurface::Fairway, 80.0f, -2.0f, 0.0f, 3000.0f, 0.0f, 0, 0, 0, 0, 0.0000f, 96.2284f, 0.0000f, 0.0700f, 0.0000f, 2.0000f, 0},
        {TEXT("along_ground"), ESurface::Fairway, 60.0f, 0.0f, 0.0f, 1500.0f, 0.0f, 0, 0, 0, 0, 0.0000f, 79.3083f, 0.0000f, 0.0700f, 0.0000f, 0.0000f, 0},
        {TEXT("wedge"), ESurface::Green, 66.4f, 23.2f, -1.4f, 6399.0f, 793.0f, 0, 0, 0, 0, 69.4685f, 69.4668f, 0.3173f, 30.9239f, 3.2611f, 35.9741f, 1},
        {TEXT("checked"), ESurface::Green, 75.054704f, 38.503029f, 1.3455229f, 10500.78f, 699.26617f, 0, 0, 0, 0, 82.0930f, 80.5834f, 3.7904f, 82.2091f, 4.9946f, 55.9247f, 1},
        {TEXT("putt"), ESurface::Green, 8.0f, 1.5f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 0, 0.0739f, 11.8774f, 0.0000f, 0.0714f, 0.0189f, 1.5007f, 0},
        {TEXT("putt_sidehill"), ESurface::Green, 6.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0, 2.0f, 0.0000f, 7.0255f, -0.9473f, 0.0700f, 0.0000f, 0.0000f, 0},
        {TEXT("approach_mid_iron"), ESurface::Rough, 93.9f, 22.9f, -2.6f, 5375.0f, 0.0f, 0, 0, 0, 0, 123.9325f, 126.1138f, -5.7209f, 72.5921f, 5.1499f, 46.8743f, 1},
        {TEXT("approach_mid_iron"), ESurface::Sand, 93.9f, 22.9f, -2.6f, 5375.0f, 0.0f, 0, 0, 0, 0, 123.9325f, 123.9325f, -5.6220f, 72.5921f, 5.1499f, 46.8743f, 2},
        {TEXT("sand_chip"), ESurface::Sand, 30.0f, 30.0f, 0.0f, 5000.0f, 0.0f, 0, 0, 0, 0, 16.9053f, 16.9053f, 0.0000f, 7.9708f, 1.4673f, 33.0186f, 2},
        {TEXT("wood_low"), ESurface::Firm, 114.46167f, 6.949297f, -0.626495f, 1931.9213f, -47.751236f, 0, 0, 0, 0, 122.3703f, 188.8532f, -3.3366f, 15.3271f, 2.8246f, 12.1328f, 1},
        {TEXT("wood_low"), ESurface::FairwaySoft, 114.46167f, 6.949297f, -0.626495f, 1931.9213f, -47.751236f, 0, 0, 0, 0, 122.3703f, 154.8595f, -2.5568f, 15.3271f, 2.8246f, 12.1328f, 1},
        {TEXT("bump_and_run"), ESurface::Fringe, 58.272453f, 15.572622f, -0.8502867f, 1700.36f, 75.733231f, 0, 0, 0, 0, 42.6623f, 58.5952f, -0.5562f, 10.1183f, 1.7921f, 19.2557f, 1},
        {TEXT("drive_headwind"), ESurface::Fairway, 150.0f, 12.5f, 1.5f, 2300.0f, -400.0f, 15.0f, 0.0f, 0, 0, 227.3537f, 233.6507f, -14.9106f, 96.5120f, 6.7733f, 47.0045f, 2},
        {TEXT("iron_crosswind"), ESurface::Fairway, 120.0f, 16.3f, 0.0f, 7097.0f, 0.0f, 10.0f, 90.0f, 0, 0, 167.4254f, 169.4482f, -16.0240f, 97.1418f, 6.2767f, 48.4306f, 1},
        {TEXT("drive_hot_high"), ESurface::Fairway, 150.0f, 12.5f, 1.5f, 2300.0f, -400.0f, 0, 0, 1, 0, 257.7233f, 285.6647f, -9.4391f, 74.5454f, 5.7924f, 29.6597f, 1},
    };

    struct FStimpPin
    {
        float StimpFeet;
        float RollingFriction;
    };
    const FStimpPin StimpPins[] = {{8.0f, 0.09425302f}, {10.0f, 0.07466027f}, {13.0f, 0.05657888f}};

    struct FGroundDragPin
    {
        float Reynolds, SpinRatio, DragCoefficient;
    };
    const FGroundDragPin GroundDragPins[] = {
        {5300.0f, 1.0f, 0.45980000f}, {20000.0f, 0.5f, 0.48639998f}, {60000.0f, 0.3f, 0.45819190f}, {120000.0f, 0.15f, 0.20406023f}, {210000.0f, 0.1f, 0.20799999f},
    };
    // clang-format on

    constexpr float DistanceToleranceYd = 0.01f;
    constexpr float ApexToleranceFt = 0.01f;
    constexpr float HangToleranceS = 0.001f;
    constexpr float AngleToleranceDeg = 0.01f;

    FAirConditions AirFor(const FPin& Pin)
    {
        FAirConditions Air;
        if (Pin.Air == 1)
        {
            Air.TemperatureF = 95.0f;
            Air.AltitudeFt = 5000.0f;
            Air.RelativeHumidity = 0.5f;
        }
        Air.WindSpeedMph = Pin.WindMph;
        Air.WindFromDeg = Pin.WindFromDeg;
        return Air;
    }

    FVector3f GroundFor(const FPin& Pin)
    {
        // Falls toward +Z (right of the target line).
        return FVector3f(0.0f, 1.0f, -Pin.SlopePercent / 100.0f).GetSafeNormal();
    }

} // namespace OpenFairwayGameplayPins

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayGameplayPinsTest, "UOpenGolf.Physics.GameplayPins",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayGameplayPinsTest::RunTest(const FString& Parameters)
{
    using namespace OpenFairwayGameplayPins;
    for (const FPin& Pin : Pins)
    {
        FLaunchData Launch;
        Launch.SpeedMph = Pin.Speed;
        Launch.VerticalLaunchDeg = Pin.Vla;
        Launch.HorizontalLaunchDeg = Pin.Hla;
        Launch.BackSpinRpm = Pin.BackSpin;
        Launch.SideSpinRpm = Pin.SideSpin;
        const FShotSimulationResult Result = ShotSimulator::Simulate(Launch, Pin.Surface, FBallPhysicsProfile(), AirFor(Pin),
            ShotSimulator::DefaultMaxTimeSeconds, GroundFor(Pin));
        const FString Label = FString::Printf(TEXT("%s on %s"), Pin.Name, SurfaceCatalog::GetName(Pin.Surface));
        AddInfo(FString::Printf(TEXT("PIN %s|%.4f, %.4f, %.4f, %.4f, %.4f, %.4f, %d"), *Label, Result.CarryM * YardsPerMeter,
            Result.TotalM * YardsPerMeter, Result.LateralM * YardsPerMeter, Result.ApexM * FeetPerMeter, Result.HangTimeS,
            Result.LandingAngleDeg, Result.BounceCount));
        if (!TestTrue(Label + TEXT(" valid"), Result.bValid))
        {
            continue;
        }
        TestTrue(Label + TEXT(" at rest"), Result.bAtRest);
        TestEqual(Label + TEXT(" carry yd"), Result.CarryM * YardsPerMeter, Pin.CarryYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" total yd"), Result.TotalM * YardsPerMeter, Pin.TotalYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" offline yd"), Result.LateralM * YardsPerMeter, Pin.LateralYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" apex ft"), Result.ApexM * FeetPerMeter, Pin.ApexFt, ApexToleranceFt);
        TestEqual(Label + TEXT(" hang s"), Result.HangTimeS, Pin.HangS, HangToleranceS);
        TestEqual(Label + TEXT(" landing deg"), Result.LandingAngleDeg, Pin.LandingDeg, AngleToleranceDeg);
        TestEqual(Label + TEXT(" bounces"), Result.BounceCount, Pin.Bounces);
    }
    for (const FStimpPin& Pin : StimpPins)
    {
        const float Friction = GreenSpeed::RollingFriction(Pin.StimpFeet);
        AddInfo(FString::Printf(TEXT("PIN stimp %.1f|%.8f"), Pin.StimpFeet, Friction));
        TestEqual(FString::Printf(TEXT("Stimp %.1f ft rolling friction"), Pin.StimpFeet), Friction, Pin.RollingFriction, 1e-6f);
    }
    for (const FGroundDragPin& Pin : GroundDragPins)
    {
        const float Drag = FlightAerodynamics::GroundDragCoefficient(Pin.Reynolds, Pin.SpinRatio);
        AddInfo(FString::Printf(TEXT("PIN drag %.0f %.2f|%.8f"), Pin.Reynolds, Pin.SpinRatio, Drag));
        TestEqual(FString::Printf(TEXT("Ground drag at Re %.0f, W %.2f"), Pin.Reynolds, Pin.SpinRatio), Drag, Pin.DragCoefficient, 1e-6f);
    }
    return true;
}

#endif
