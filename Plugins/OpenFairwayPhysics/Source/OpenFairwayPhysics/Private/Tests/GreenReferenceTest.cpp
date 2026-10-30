// SPDX-License-Identifier: MIT
// Green, putting and slope regressions ported from ogolf-physics at
// e6d793ae9477efaf55dc9078d7260f76bb532a7d: PuttRollRegressionTests, GreenSpinBackRegressionTests,
// LandingBucketRegressionTests and BallPhysicsSurfaceResponseTests. Golden values come from the
// .NET harness (`dotnet run -- <ogolf-physics> greens`, historical reference profile); the bands
// are the reference tests' own. See the plugin README to regenerate.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace GreenReferenceTestDetail
{
    struct FShot
    {
        float Speed, Vla, Hla, BackSpin, SideSpin, TotalSpin, SpinAxis;
    };

    // assets/data test shots.
    constexpr FShot Wedge{66.4f, 23.2f, -1.4f, 6399.0f, 793.0f, 6449.0f, 7.1f};
    constexpr FShot MidIron{93.9f, 22.9f, -2.6f, 5375.0f, 0.0f, 5375.0f, 2.0f};
    constexpr FShot Checked{75.054704f, 38.503028869628906f, 1.3455228805541992f, 10500.780354572425f, 699.266167598911f, 10700.8457f, 5.7414904f};
    constexpr FShot Flop{68.054704f, 45.0f, 0.4455228805541992f, 12000.0f, 140.266167598911f, 12000.0f, 0.8414904f};
    constexpr FShot WoodLow{114.46167398452758f, 6.949296951293945f, -0.6264948844909668f, 1931.9213097756137f, -47.75123587127419f, 1932.5113525390625f,
        -1.4158896207809448f};

    FLaunchData MakeLaunch(const FShot& Shot)
    {
        FLaunchData Launch;
        Launch.SpeedMph = Shot.Speed;
        Launch.VerticalLaunchDeg = Shot.Vla;
        Launch.HorizontalLaunchDeg = Shot.Hla;
        Launch.BackSpinRpm = Shot.BackSpin;
        Launch.SideSpinRpm = Shot.SideSpin;
        Launch.TotalSpinRpm = Shot.TotalSpin;
        Launch.SpinAxisDeg = Shot.SpinAxis;
        return Launch;
    }

    /** A putt as the reference tests build it: total spin and axis 0, no measured components. */
    FLaunchData MakePutt(float SpeedMph, float VlaDeg = 0.0f)
    {
        FLaunchData Launch;
        Launch.SpeedMph = SpeedMph;
        Launch.VerticalLaunchDeg = VlaDeg;
        Launch.TotalSpinRpm = 0.0f;
        Launch.SpinAxisDeg = 0.0f;
        return Launch;
    }

    /** Ground tilted downhill along the target line by SlopeDeg (negative is uphill). */
    FVector3f SlopeNormal(float SlopeDeg)
    {
        const float Slope = FMath::DegreesToRadians(SlopeDeg);
        return FVector3f(FMath::Sin(Slope), FMath::Cos(Slope), 0.0f);
    }

    FBallPhysicsProfile Reference(float StimpFeet = GreenSpeed::DefaultStimpFeet, bool bNeutralLanding = false)
    {
        FBallPhysicsProfile Profile = FBallPhysicsProfile::HistoricalReference();
        Profile.GreenStimpFeet = StimpFeet;
        if (bNeutralLanding)
        {
            for (FLandingResponse& Response : Profile.LandingResponses)
            {
                Response.SpinBiteScale = 0.0f;
            }
        }
        return Profile;
    }

    FShotSimulationResult Simulate(const FLaunchData& Launch, ESurface Surface, const FBallPhysicsProfile& Profile, float SlopeDeg = 0.0f)
    {
        return ShotSimulator::Simulate(Launch, Surface, Profile, FAirConditions(), ShotSimulator::DefaultMaxTimeSeconds, SlopeNormal(SlopeDeg));
    }

    float RolloutYd(const FShotSimulationResult& Result)
    {
        return (Result.TotalM - Result.CarryM) * YardsPerMeter;
    }

    struct FGolden
    {
        const TCHAR* Name;
        FLaunchData Launch;
        ESurface Surface;
        float StimpFeet, SlopeDeg;
        bool bNeutralLanding;
        float CarryYd, TotalYd, LateralYd;
        bool bSpinback;
    };

    constexpr float DistanceToleranceYd = 0.1f;
} // namespace GreenReferenceTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayGreenReferenceTest, "UOpenGolf.Physics.GreenReference",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayGreenReferenceTest::RunTest(const FString& Parameters)
{
    using namespace GreenReferenceTestDetail;
    constexpr float Stimp = GreenSpeed::DefaultStimpFeet;

    // clang-format off
    const FGolden Goldens[] = {
        {TEXT("putt 4.1 mph"), MakePutt(4.1f), ESurface::Green, Stimp, 0.0f, false, 0.0f, 3.3061f, 0.0f, false},
        {TEXT("putt 5.5 mph"), MakePutt(5.5f), ESurface::Green, Stimp, 0.0f, false, 0.0f, 5.3758f, 0.0f, false},
        {TEXT("putt 7.8 mph"), MakePutt(7.8f), ESurface::Green, Stimp, 0.0f, false, 0.0f, 9.3734f, 0.0f, false},
        {TEXT("putt 11 mph"), MakePutt(11.0f), ESurface::Green, Stimp, 0.0f, false, 0.0f, 15.7183f, 0.0f, false},
        {TEXT("putt 5.5 mph at 3 deg"), MakePutt(5.5f, 3.0f), ESurface::Green, Stimp, 0.0f, false, 0.0471f, 5.3987f, 0.0f, false},
        {TEXT("putt 3.9 mph stimp 11"), MakePutt(3.9f), ESurface::Green, 11.0f, 0.0f, false, 0.0f, 3.3461f, 0.0f, false},
        {TEXT("putt 6 mph stimp 11"), MakePutt(6.0f), ESurface::Green, 11.0f, 0.0f, false, 0.0f, 6.7454f, 0.0f, false},
        {TEXT("putt 9.5 mph stimp 11"), MakePutt(9.5f), ESurface::Green, 11.0f, 0.0f, false, 0.0f, 13.5647f, 0.0f, false},
        {TEXT("putt 12.5 mph stimp 11"), MakePutt(12.5f), ESurface::Green, 11.0f, 0.0f, false, 0.0f, 20.1113f, 0.0f, false},
        {TEXT("putt 6 mph stimp 8"), MakePutt(6.0f), ESurface::Green, 8.0f, 0.0f, false, 0.0f, 5.0659f, 0.0f, false},
        {TEXT("putt 6 mph stimp 13"), MakePutt(6.0f), ESurface::Green, 13.0f, 0.0f, false, 0.0f, 7.8236f, 0.0f, false},
        {TEXT("flop"), MakeLaunch(Flop), ESurface::Green, Stimp, 0.0f, false, 61.7926f, 60.2227f, 0.5696f, true},
        {TEXT("wedge neutral landing"), MakeLaunch(Wedge), ESurface::Green, Stimp, 0.0f, true, 72.2769f, 78.0353f, 0.3158f, false},
        {TEXT("approach_mid_iron"), MakeLaunch(MidIron), ESurface::Green, Stimp, 0.0f, false, 125.9240f, 132.9257f, -6.0299f, false},
        {TEXT("approach_mid_iron neutral landing"), MakeLaunch(MidIron), ESurface::Green, Stimp, 0.0f, true, 125.9240f, 135.2746f, -6.1364f, false},
        {TEXT("wedge 2 deg downhill"), MakeLaunch(Wedge), ESurface::Green, Stimp, 2.0f, false, 75.9297f, 78.9572f, 0.3082f, false},
        {TEXT("wedge 3 deg uphill"), MakeLaunch(Wedge), ESurface::Green, Stimp, -3.0f, false, 66.5742f, 66.9674f, -0.0003f, false},
        {TEXT("putt 6 mph 2 deg downhill"), MakePutt(6.0f), ESurface::Green, Stimp, 2.0f, false, 0.0294f, 10.6416f, 0.0f, false},
        {TEXT("putt 6 mph 2 deg uphill"), MakePutt(6.0f), ESurface::Green, Stimp, -2.0f, false, 0.0f, 4.4181f, 0.0f, false},
        {TEXT("wood_low 2 deg downhill"), MakeLaunch(WoodLow), ESurface::Fairway, Stimp, 2.0f, false, 141.7142f, 213.0855f, -3.2878f, false},
        {TEXT("wood_low 2 deg uphill"), MakeLaunch(WoodLow), ESurface::Rough, Stimp, -2.0f, false, 102.5549f, 129.2812f, -2.0216f, false},
    };
    // clang-format on

    for (const FGolden& Golden : Goldens)
    {
        const FString Label = FString::Printf(TEXT("%s on %s"), Golden.Name, SurfaceCatalog::GetName(Golden.Surface));
        const FShotSimulationResult Result = Simulate(Golden.Launch, Golden.Surface, Reference(Golden.StimpFeet, Golden.bNeutralLanding), Golden.SlopeDeg);
        AddInfo(FString::Printf(TEXT("%s: carry %.4f (golden %.4f), total %.4f (golden %.4f) yd, first-impact tangent out %.3f m/s"), *Label,
            Result.CarryM * YardsPerMeter, Golden.CarryYd, Result.TotalM * YardsPerMeter, Golden.TotalYd, Result.FirstImpactTangentOutMps));
        if (!TestTrue(Label + TEXT(" valid"), Result.bValid))
        {
            continue;
        }
        TestTrue(Label + TEXT(" at rest"), Result.bAtRest);
        TestEqual(Label + TEXT(" carry yd"), Result.CarryM * YardsPerMeter, Golden.CarryYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" total yd"), Result.TotalM * YardsPerMeter, Golden.TotalYd, DistanceToleranceYd);
        TestEqual(Label + TEXT(" offline yd"), Result.LateralM * YardsPerMeter, Golden.LateralYd, DistanceToleranceYd);
        TestTrue(Label + TEXT(" first-impact spin-back"), Result.bFirstImpactSpinback == Golden.bSpinback);
    }

    // PuttRollRegressionTests: a stimpmeter-speed putt goes the stimp distance; faster putts meet
    // more resistance than the square of the speed.
    auto RollFeet = [](const FLaunchData& Putt, const FBallPhysicsProfile& Profile, ESurface Surface = ESurface::Green)
    {
        return Simulate(Putt, Surface, Profile).TotalM * FeetPerMeter;
    };
    const struct
    {
        float SpeedMph, MinFeet, MaxFeet;
    } Bands[] = {{4.1f, 9.2f, 10.8f}, {5.5f, 14.8f, 17.4f}, {7.8f, 25.9f, 30.4f}, {11.0f, 43.4f, 51.0f}};
    for (const auto& Band : Bands)
    {
        const float Feet = RollFeet(MakePutt(Band.SpeedMph), Reference());
        TestTrue(FString::Printf(TEXT("%.1f mph putt rolls %.2f ft, within %.1f-%.1f ft"), Band.SpeedMph, Feet, Band.MinFeet, Band.MaxFeet),
            Feet >= Band.MinFeet && Feet <= Band.MaxFeet);
    }
    // A simulator in wide use asks for these rolls on a stimp-11 green.
    const struct
    {
        float SpeedMph, Feet;
    } StimpEleven[] = {{3.9f, 10.0f}, {6.0f, 20.0f}, {9.5f, 40.0f}, {12.5f, 60.0f}};
    for (const auto& Roll : StimpEleven)
    {
        TestEqual(FString::Printf(TEXT("%.1f mph putt on stimp 11"), Roll.SpeedMph), RollFeet(MakePutt(Roll.SpeedMph), Reference(11.0f)), Roll.Feet,
            Roll.Feet * 0.08f);
    }
    const float Standard = RollFeet(MakePutt(6.0f), Reference());
    TestTrue(TEXT("A stimp-8 green rolls a putt under 90% as far"), RollFeet(MakePutt(6.0f), Reference(8.0f)) < Standard * 0.9f);
    TestTrue(TEXT("A stimp-13 green rolls a putt over 115% as far"), RollFeet(MakePutt(6.0f), Reference(13.0f)) > Standard * 1.15f);
    const float Flat = RollFeet(MakePutt(5.5f), Reference());
    TestEqual(TEXT("A few degrees of launch is still a putt"), RollFeet(MakePutt(5.5f, 3.0f), Reference()), Flat, Flat * 0.05f);
    const FLaunchData Chip = [] {
        FLaunchData Launch;
        Launch.SpeedMph = 24.676933760261534f;
        Launch.VerticalLaunchDeg = 17.938442f;
        Launch.HorizontalLaunchDeg = 20.353489f;
        Launch.BackSpinRpm = 3203.8918f;
        Launch.SideSpinRpm = 0.0f;
        Launch.TotalSpinRpm = 3203.8918f;
        Launch.SpinAxisDeg = 0.0f;
        return Launch;
    }();
    TestEqual(TEXT("Green speed does not move a shot on the fairway"), Simulate(Chip, ESurface::Fairway, Reference(15.0f)).TotalM,
        Simulate(Chip, ESurface::Fairway, Reference(8.0f)).TotalM);

    // LandingBucketRegressionTests: bucket bite shortens green rollout; the top bucket draws back
    // mildly on green only; Run-bucket shots keep exactly the neutral bounce.
    const struct
    {
        const TCHAR* Name;
        FShot Shot;
        ELandingBucket Bucket;
        float MinYd, MaxYd;
    } BucketShots[] = {{TEXT("wedge"), Wedge, ELandingBucket::Check, 0.0f, 3.0f}, {TEXT("approach_mid_iron"), MidIron, ELandingBucket::Release, 6.0f, 12.0f}};
    for (const auto& Shot : BucketShots)
    {
        const FShotSimulationResult Green = Simulate(MakeLaunch(Shot.Shot), ESurface::Green, Reference());
        const float Neutral = RolloutYd(Simulate(MakeLaunch(Shot.Shot), ESurface::Green, Reference(Stimp, true)));
        const float Rollout = RolloutYd(Green);
        TestTrue(FString::Printf(TEXT("%s is in the %s bucket"), Shot.Name, LandingBucket::GetName(Shot.Bucket)), Green.LandingBucket == Shot.Bucket);
        TestTrue(FString::Printf(TEXT("%s rolls out %.2f yd on green, within %.0f-%.0f yd"), Shot.Name, Rollout, Shot.MinYd, Shot.MaxYd),
            Rollout >= Shot.MinYd && Rollout <= Shot.MaxYd);
        TestTrue(FString::Printf(TEXT("%s bucket bite shortens rollout (%.2f vs neutral %.2f yd)"), Shot.Name, Rollout, Neutral), Rollout < Neutral);
    }
    for (const auto& Shot : {TPair<const TCHAR*, FShot>(TEXT("checked"), Checked), TPair<const TCHAR*, FShot>(TEXT("flop"), Flop)})
    {
        const FShotSimulationResult Green = Simulate(MakeLaunch(Shot.Value), ESurface::Green, Reference());
        const FShotSimulationResult Fairway = Simulate(MakeLaunch(Shot.Value), ESurface::Fairway, Reference());
        TestTrue(FString::Printf(TEXT("%s is in the Spinback bucket"), Shot.Key), Green.LandingBucket == ELandingBucket::Spinback);
        TestTrue(FString::Printf(TEXT("%s spins back on green"), Shot.Key), Green.bFirstImpactSpinback);
        TestTrue(FString::Printf(TEXT("%s draws back %.2f yd on green, within -2.5 to -0.5 yd"), Shot.Key, RolloutYd(Green)),
            RolloutYd(Green) >= -2.5f && RolloutYd(Green) <= -0.5f);
        TestFalse(FString::Printf(TEXT("%s does not spin back on fairway"), Shot.Key), Fairway.bFirstImpactSpinback);
        TestTrue(FString::Printf(TEXT("%s has no fairway spin-back response"), Shot.Key), RolloutYd(Fairway) >= -0.1f);
        // BallPhysicsSurfaceResponseTests: green gives more reverse tangent speed than fairway.
        TestTrue(FString::Printf(TEXT("%s leaves green moving back (%.3f m/s) and fairway not (%.3f m/s)"), Shot.Key, Green.FirstImpactTangentOutMps,
                     Fairway.FirstImpactTangentOutMps),
            Green.FirstImpactTangentOutMps < 0.0f && Green.FirstImpactTangentOutMps < Fairway.FirstImpactTangentOutMps);
    }
    const FLaunchData Drive = [] {
        FLaunchData Launch;
        Launch.SpeedMph = 150.0f;
        Launch.VerticalLaunchDeg = 12.5f;
        Launch.HorizontalLaunchDeg = 1.5f;
        Launch.BackSpinRpm = 2300.0f;
        Launch.SideSpinRpm = -400.0f;
        Launch.TotalSpinRpm = 2335.0f;
        Launch.SpinAxisDeg = -9.9f;
        return Launch;
    }();
    for (const FLaunchData& Launch : {Drive, MakeLaunch(WoodLow), Chip})
    {
        for (const ESurface Surface : {ESurface::Green, ESurface::Fairway})
        {
            const FShotSimulationResult Bucketed = Simulate(Launch, Surface, Reference());
            TestTrue(TEXT("Run bucket"), Bucketed.LandingBucket == ELandingBucket::Run);
            TestEqual(FString::Printf(TEXT("%.1f mph Run shot on %s keeps the neutral rollout"), Launch.SpeedMph, SurfaceCatalog::GetName(Surface)),
                Bucketed.TotalM, Simulate(Launch, Surface, Reference(Stimp, true)).TotalM);
        }
    }

    // GreenSpinBackRegressionTests: on a 10 degree green slope the flop still spins back on
    // landing; downhill finishes no shorter and uphill no longer than flat.
    const FShotSimulationResult FlatFlop = Simulate(MakeLaunch(Flop), ESurface::Green, Reference());
    const FShotSimulationResult Downhill = Simulate(MakeLaunch(Flop), ESurface::Green, Reference(), 10.0f);
    const FShotSimulationResult Uphill = Simulate(MakeLaunch(Flop), ESurface::Green, Reference(), -10.0f);
    AddInfo(FString::Printf(TEXT("Flop on green: flat %.2f yd, downhill %.2f yd, uphill %.2f yd total"), FlatFlop.TotalM * YardsPerMeter,
        Downhill.TotalM * YardsPerMeter, Uphill.TotalM * YardsPerMeter));
    TestTrue(TEXT("Flat green flop draws back at least half a yard"), RolloutYd(FlatFlop) <= -0.5f);
    TestTrue(TEXT("Flat green flop reverses on first impact"), FlatFlop.bFirstImpactSpinback && FlatFlop.FirstImpactTangentOutMps < 0.0f);
    TestTrue(TEXT("Downhill is not meaningfully shorter than flat"), Downhill.TotalM * YardsPerMeter >= FlatFlop.TotalM * YardsPerMeter - 0.25f);
    TestTrue(TEXT("Uphill is not meaningfully longer than flat"), Uphill.TotalM * YardsPerMeter <= FlatFlop.TotalM * YardsPerMeter + 0.25f);
    TestTrue(TEXT("Downhill travels at least as far after impact as uphill"), RolloutYd(Downhill) >= RolloutYd(Uphill) - 0.10f);

    // The current profile keeps the same directions on a slope.
    const FBallPhysicsProfile Current;
    const float CurrentFlat = Simulate(MakePutt(6.0f), ESurface::Green, Current).TotalM;
    const FShotSimulationResult CurrentDownhill = Simulate(MakePutt(6.0f), ESurface::Green, Current, 2.0f);
    const FShotSimulationResult CurrentUphill = Simulate(MakePutt(6.0f), ESurface::Green, Current, -2.0f);
    AddInfo(FString::Printf(TEXT("Current 6 mph putt: flat %.2f ft, 2 deg downhill %.2f ft, 2 deg uphill %.2f ft"), CurrentFlat * FeetPerMeter,
        CurrentDownhill.TotalM * FeetPerMeter, CurrentUphill.TotalM * FeetPerMeter));
    TestTrue(TEXT("Current profile: downhill putt rolls farther and stops"), CurrentDownhill.bAtRest && CurrentDownhill.TotalM > CurrentFlat);
    TestTrue(TEXT("Current profile: uphill putt rolls shorter and stops"), CurrentUphill.bAtRest && CurrentUphill.TotalM < CurrentFlat);
    TestTrue(TEXT("Level ground is the default"), Simulate(MakePutt(6.0f), ESurface::Green, Current, 0.0f).TotalM ==
                                                      ShotSimulator::Simulate(MakePutt(6.0f), ESurface::Green, Current).TotalM);
    return true;
}

#endif
