// SPDX-License-Identifier: MIT
// Golden values come from ogolf-physics at e6d793ae9477efaf55dc9078d7260f76bb532a7d: its
// Godot-free core sources compiled into a .NET harness that mirrors PhysicsAdapter's headless
// shot loop (flat ground, 75 F, sea level, default ball profile). Launch data is from its
// assets/data test shots; "putt" is an added no-spin putt. See the plugin README to regenerate.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace
{
    struct FFixture
    {
        const TCHAR* Name;
        ESurface Surface;
        float Speed, Vla, Hla, BackSpin, SideSpin, TotalSpin, SpinAxis;
        float CarryYd, TotalYd, ApexFt, HangS, LateralYd;
        ELandingBucket Bucket;
    };

    // clang-format off
    const FFixture Fixtures[] = {
        {TEXT("wood_low"), ESurface::Fairway, 114.46167398452758f, 6.949296951293945f, -0.6264948844909668f, 1931.9213097756137f, -47.75123587127419f, 1932.5113525390625f, -1.4158896207809448f, 123.8130f, 170.7061f, 16.0014f, 2.7969f, -2.7995f, ELandingBucket::Run},
        {TEXT("drive"), ESurface::Fairway, 150.0f, 12.5f, 1.5f, 2300.0f, -400.0f, 2335.0f, -9.9f, 248.1921f, 274.6819f, 82.1891f, 5.8530f, -10.0061f, ELandingBucket::Run},
        {TEXT("approach_mid_iron"), ESurface::Fairway, 93.9f, 22.9f, -2.6f, 5375.0f, 0.0f, 5375.0f, 2.0f, 125.9240f, 133.0238f, 64.3004f, 4.6557f, -6.0343f, ELandingBucket::Release},
        {TEXT("wedge"), ESurface::Fairway, 66.4f, 23.2f, -1.4f, 6399.0f, 793.0f, 6449.0f, 7.1f, 72.2769f, 74.4750f, 30.7598f, 3.2215f, 0.1951f, ELandingBucket::Check},
        {TEXT("chip"), ESurface::Fairway, 24.676933760261534f, 17.938442f, 20.353489f, 3203.8918f, 0.0f, 3203.8918f, 0.0f, 7.6178f, 9.9024f, 1.9241f, 0.6788f, 3.4442f, ELandingBucket::Run},
        {TEXT("bump"), ESurface::Fairway, 78.27245326499938f, 5.5726221f, -0.8502867f, 1700.360008455391f, 75.7332313414943f, 1850.0674f, 2.314628f, 37.0760f, 81.7672f, 3.0057f, 1.0515f, -0.8831f, ELandingBucket::Run},
        {TEXT("checked"), ESurface::Fairway, 75.054704f, 38.503028869628906f, 1.3455228805541992f, 10500.780354572425f, 699.266167598911f, 10700.8457f, 5.7414904f, 75.8079f, 75.8075f, 70.1335f, 4.4095f, 2.7247f, ELandingBucket::Spinback},
        {TEXT("flop"), ESurface::Fairway, 68.054704f, 45.0f, 0.4455228805541992f, 12000.0f, 140.266167598911f, 12000.0f, 0.8414904f, 61.7926f, 61.7926f, 68.5819f, 4.2420f, 0.5850f, ELandingBucket::Spinback},
        {TEXT("topped"), ESurface::Fairway, 91.78173648796081f, 5.0f, -0.8898518085479736f, 2108.1735594691377f, 612.311284467954f, 2195.295166015625f, 16.19574546813965f, 55.3458f, 99.6162f, 4.2544f, 1.3765f, 2.1575f, ELandingBucket::Run},
        {TEXT("5iron"), ESurface::Fairway, 102.1f, 17.4f, 1.5f, 5266.0f, 1151.0f, 5391.0f, 12.3f, 140.9999f, 149.4223f, 61.5425f, 4.7414f, 14.8185f, ELandingBucket::Release},
        {TEXT("driver1"), ESurface::Fairway, 124.0f, 9.4f, -9.5f, 2107.923f, -973.19f, 2322.0f, -24.8f, 157.4217f, 200.0365f, 29.3416f, 3.5816f, -55.0228f, ELandingBucket::Run},
        {TEXT("p_wedge_shot_1"), ESurface::Fairway, 82.27544053993225f, 26.45549964904785f, -3.1465682983398438f, 4912.299856410009f, 580.3159525795915f, 4946.458984375f, 6.7374267578125f, 106.2205f, 113.2564f, 58.4605f, 4.3230f, -2.9685f, ELandingBucket::Release},
        {TEXT("bump_and_run"), ESurface::Fairway, 58.27245326499938f, 15.5726221f, -0.8502867f, 1700.360008455391f, 75.7332313414943f, 1850.0674f, 2.314628f, 37.9370f, 63.0549f, 8.7386f, 1.4836f, -0.8755f, ELandingBucket::Run},
        {TEXT("wedge"), ESurface::Green, 66.4f, 23.2f, -1.4f, 6399.0f, 793.0f, 6449.0f, 7.1f, 72.2769f, 73.4257f, 30.7598f, 3.2215f, 0.1717f, ELandingBucket::Check},
        {TEXT("approach_mid_iron"), ESurface::Rough, 93.9f, 22.9f, -2.6f, 5375.0f, 0.0f, 5375.0f, 2.0f, 125.9240f, 131.1594f, 64.3004f, 4.6557f, -5.9498f, ELandingBucket::Release},
        {TEXT("approach_mid_iron"), ESurface::Sand, 93.9f, 22.9f, -2.6f, 5375.0f, 0.0f, 5375.0f, 2.0f, 125.9240f, 126.8931f, 64.3004f, 4.6557f, -5.7562f, ELandingBucket::Release},
        {TEXT("wood_low"), ESurface::Firm, 114.46167398452758f, 6.949296951293945f, -0.6264948844909668f, 1931.9213097756137f, -47.75123587127419f, 1932.5113525390625f, -1.4158896207809448f, 123.8130f, 215.8024f, 16.0014f, 2.7969f, -3.7326f, ELandingBucket::Run},
        {TEXT("wood_low"), ESurface::FairwaySoft, 114.46167398452758f, 6.949296951293945f, -0.6264948844909668f, 1931.9213097756137f, -47.75123587127419f, 1932.5113525390625f, -1.4158896207809448f, 123.8130f, 159.1775f, 16.0014f, 2.7969f, -2.5761f, ELandingBucket::Run},
        {TEXT("bump_and_run"), ESurface::Fringe, 58.27245326499938f, 15.5726221f, -0.8502867f, 1700.360008455391f, 75.7332313414943f, 1850.0674f, 2.314628f, 37.9370f, 56.5818f, 8.7386f, 1.4836f, -0.7864f, ELandingBucket::Run},
        {TEXT("putt"), ESurface::Green, 8.0f, 1.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0381f, 9.7539f, 0.0703f, 0.0097f, 0.0f, ELandingBucket::Run},
        {TEXT("checked"), ESurface::Green, 75.054704f, 38.503028869628906f, 1.3455228805541992f, 10500.780354572425f, 699.266167598911f, 10700.8457f, 5.7414904f, 75.8079f, 74.2398f, 70.1335f, 4.4095f, 2.6628f, ELandingBucket::Spinback},
    };
    // clang-format on

    // Single-precision results differ from .NET's by rounding only; these leave room for that.
    constexpr float DistanceToleranceYd = 0.1f;
    constexpr float ApexToleranceFt = 0.1f;
    constexpr float HangToleranceS = 0.01f;
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayPhysicsFixtureTest, "UOpenGolf.Physics.Fixtures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayPhysicsFixtureTest::RunTest(const FString& Parameters)
{
    for (const FFixture& Fixture : Fixtures)
    {
        FLaunchData Launch;
        Launch.SpeedMph = Fixture.Speed;
        Launch.VerticalLaunchDeg = Fixture.Vla;
        Launch.HorizontalLaunchDeg = Fixture.Hla;
        Launch.BackSpinRpm = Fixture.BackSpin;
        Launch.SideSpinRpm = Fixture.SideSpin;
        Launch.TotalSpinRpm = Fixture.TotalSpin;
        Launch.SpinAxisDeg = Fixture.SpinAxis;

        const FShotSimulationResult Result = ShotSimulator::Simulate(Launch, Fixture.Surface);
        const FString Label = FString::Printf(TEXT("%s on %s"), Fixture.Name, SurfaceCatalog::GetName(Fixture.Surface));
        AddInfo(FString::Printf(TEXT("%s: carry %.4f (golden %.4f), total %.4f (golden %.4f), apex %.4f ft, hang %.4f s, offline %.4f yd"), *Label,
            Result.CarryM * YardsPerMeter, Fixture.CarryYd, Result.TotalM * YardsPerMeter, Fixture.TotalYd, Result.ApexM * FeetPerMeter, Result.HangTimeS,
            Result.LateralM * YardsPerMeter));
        if (!TestTrue(Label + TEXT(" valid"), Result.bValid))
        {
            continue;
        }
        TestTrue(Label + TEXT(" at rest"), Result.bAtRest);
        TestEqual(Label + TEXT(" carry yd"), Result.CarryM * YardsPerMeter, Fixture.CarryYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" total yd"), Result.TotalM * YardsPerMeter, Fixture.TotalYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" offline yd"), Result.LateralM * YardsPerMeter, Fixture.LateralYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" apex ft"), Result.ApexM * FeetPerMeter, Fixture.ApexFt, ApexToleranceFt);
        TestEqual(Label + TEXT(" hang s"), Result.HangTimeS, Fixture.HangS, HangToleranceS);
        TestTrue(Label + TEXT(" landing bucket"), Result.LandingBucket == Fixture.Bucket);
    }
    return true;
}

#endif
