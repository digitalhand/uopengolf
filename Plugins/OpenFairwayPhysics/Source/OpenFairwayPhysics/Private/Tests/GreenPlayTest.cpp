// SPDX-License-Identifier: MIT
// How the gameplay ball plays on greens: putt distance with green speed, the landing buckets' bite, spin-back on the
// green but not the fairway, and slopes. The checks follow the putting, spin-back and landing-bucket regression tests of
// ogolf-physics, as relations on the gameplay profile; GameplayPins pins the exact distances.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace GreenPlayTestDetail
{
    struct FShot
    {
        float Speed, Vla, Hla, BackSpin, SideSpin;
    };

    constexpr FShot Wedge{66.4f, 23.2f, -1.4f, 6399.0f, 793.0f};
    constexpr FShot MidIron{93.9f, 22.9f, -2.6f, 5375.0f, 0.0f};
    constexpr FShot Checked{75.054704f, 38.503029f, 1.3455229f, 10500.78f, 699.26617f};
    constexpr FShot Flop{68.054704f, 45.0f, 0.4455229f, 12000.0f, 140.26617f};
    constexpr FShot WoodLow{114.46167f, 6.949297f, -0.626495f, 1931.9213f, -47.751236f};
    constexpr FShot Drive{150.0f, 12.5f, 1.5f, 2300.0f, -400.0f};
    constexpr FShot Chip{24.676934f, 17.938442f, 20.353489f, 3203.8918f, 0.0f};

    FLaunchData MakeLaunch(const FShot& Shot)
    {
        FLaunchData Launch;
        Launch.SpeedMph = Shot.Speed;
        Launch.VerticalLaunchDeg = Shot.Vla;
        Launch.HorizontalLaunchDeg = Shot.Hla;
        Launch.BackSpinRpm = Shot.BackSpin;
        Launch.SideSpinRpm = Shot.SideSpin;
        return Launch;
    }

    /** A putt with no measured spin: it leaves the face rolling. */
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

    /** The gameplay ball on a green of this stimp, optionally with no landing bite. */
    FBallPhysicsProfile Ball(float StimpFeet = GreenSpeed::DefaultStimpFeet, bool bNeutralLanding = false)
    {
        FBallPhysicsProfile Profile;
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

    float RollFeet(const FLaunchData& Putt, const FBallPhysicsProfile& Profile)
    {
        return Simulate(Putt, ESurface::Green, Profile).TotalM * FeetPerMeter;
    }
} // namespace GreenPlayTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayGreenPlayTest, "UOpenGolf.Physics.GreenPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayGreenPlayTest::RunTest(const FString& Parameters)
{
    using namespace GreenPlayTestDetail;
    constexpr float Stimp = GreenSpeed::DefaultStimpFeet;

    // A putt at stimpmeter speed goes the stimp distance; faster putts meet more resistance than the square of the speed.
    const float StimpPutt = RollFeet(MakePutt(GreenSpeed::StimpmeterSpeedMps / MpsPerMph), Ball());
    TestEqual(TEXT("A stimpmeter-speed putt rolls the stimp distance"), StimpPutt, Stimp, Stimp * 0.08f);
    for (float SpeedMph : {5.5f, 7.8f, 11.0f})
    {
        const float Feet = RollFeet(MakePutt(SpeedMph), Ball());
        const float Squared = Stimp * FMath::Square(SpeedMph * MpsPerMph / GreenSpeed::StimpmeterSpeedMps);
        AddInfo(FString::Printf(TEXT("%.1f mph putt rolls %.2f ft (%.2f ft at constant deceleration)"), SpeedMph, Feet, Squared));
        TestTrue(FString::Printf(TEXT("%.1f mph putt meets speed-dependent resistance"), SpeedMph), Feet < Squared);
    }
    for (const auto& Roll : {TPair<float, float>(3.9f, 10.0f), TPair<float, float>(6.0f, 20.0f), TPair<float, float>(9.5f, 40.0f), TPair<float, float>(12.5f, 60.0f)})
    {
        AddInfo(FString::Printf(TEXT("%.1f mph putt on stimp 11 rolls %.1f ft (a common putting target: %.0f ft)"), Roll.Key,
            RollFeet(MakePutt(Roll.Key), Ball(11.0f)), Roll.Value));
    }
    const float Standard = RollFeet(MakePutt(6.0f), Ball());
    TestTrue(TEXT("A stimp-8 green rolls a putt under 90% as far"), RollFeet(MakePutt(6.0f), Ball(8.0f)) < Standard * 0.9f);
    TestTrue(TEXT("A stimp-13 green rolls a putt over 115% as far"), RollFeet(MakePutt(6.0f), Ball(13.0f)) > Standard * 1.15f);
    const float Flat = RollFeet(MakePutt(5.5f), Ball());
    TestEqual(TEXT("A few degrees of launch is still a putt"), RollFeet(MakePutt(5.5f, 3.0f), Ball()), Flat, Flat * 0.05f);
    TestEqual(TEXT("Green speed does not move a shot on the fairway"), Simulate(MakeLaunch(Chip), ESurface::Fairway, Ball(15.0f)).TotalM,
        Simulate(MakeLaunch(Chip), ESurface::Fairway, Ball(8.0f)).TotalM);

    // The landing buckets' bite shortens green roll-out; Run-bucket shots keep exactly the neutral bounce.
    const struct
    {
        const TCHAR* Name;
        FShot Shot;
        ELandingBucket Bucket;
    } BucketShots[] = {{TEXT("wedge"), Wedge, ELandingBucket::Check}, {TEXT("approach_mid_iron"), MidIron, ELandingBucket::Release}};
    for (const auto& Shot : BucketShots)
    {
        const FShotSimulationResult Green = Simulate(MakeLaunch(Shot.Shot), ESurface::Green, Ball());
        const float Neutral = RolloutYd(Simulate(MakeLaunch(Shot.Shot), ESurface::Green, Ball(Stimp, true)));
        const float Rollout = RolloutYd(Green);
        TestTrue(FString::Printf(TEXT("%s is in the %s bucket"), Shot.Name, LandingBucket::GetName(Shot.Bucket)), Green.LandingBucket == Shot.Bucket);
        TestTrue(FString::Printf(TEXT("%s bucket bite shortens green roll-out (%.2f vs neutral %.2f yd)"), Shot.Name, Rollout, Neutral), Rollout < Neutral);
    }
    for (const auto& Shot : {TPair<const TCHAR*, FShot>(TEXT("checked"), Checked), TPair<const TCHAR*, FShot>(TEXT("flop"), Flop)})
    {
        const FShotSimulationResult Green = Simulate(MakeLaunch(Shot.Value), ESurface::Green, Ball());
        const FShotSimulationResult Fairway = Simulate(MakeLaunch(Shot.Value), ESurface::Fairway, Ball());
        AddInfo(FString::Printf(TEXT("%s: green roll-out %.2f yd, fairway %.2f yd"), Shot.Key, RolloutYd(Green), RolloutYd(Fairway)));
        TestTrue(FString::Printf(TEXT("%s is in the Spinback bucket"), Shot.Key), Green.LandingBucket == ELandingBucket::Spinback);
        TestTrue(FString::Printf(TEXT("%s spins back on green"), Shot.Key), Green.bFirstImpactSpinback && RolloutYd(Green) < 0.0f);
        TestFalse(FString::Printf(TEXT("%s does not spin back on fairway"), Shot.Key), Fairway.bFirstImpactSpinback);
        TestTrue(FString::Printf(TEXT("%s has no fairway spin-back response"), Shot.Key), RolloutYd(Fairway) >= -0.1f);
        TestTrue(FString::Printf(TEXT("%s leaves green moving back (%.3f m/s) and fairway not (%.3f m/s)"), Shot.Key, Green.FirstImpactTangentOutMps,
                     Fairway.FirstImpactTangentOutMps),
            Green.FirstImpactTangentOutMps < 0.0f && Green.FirstImpactTangentOutMps < Fairway.FirstImpactTangentOutMps);
    }
    for (const FShot& Shot : {Drive, WoodLow, Chip})
    {
        for (const ESurface Surface : {ESurface::Green, ESurface::Fairway})
        {
            const FShotSimulationResult Bucketed = Simulate(MakeLaunch(Shot), Surface, Ball());
            TestTrue(TEXT("Run bucket"), Bucketed.LandingBucket == ELandingBucket::Run);
            TestEqual(FString::Printf(TEXT("%.1f mph Run shot on %s keeps the neutral roll-out"), Shot.Speed, SurfaceCatalog::GetName(Surface)),
                Bucketed.TotalM, Simulate(MakeLaunch(Shot), Surface, Ball(Stimp, true)).TotalM);
        }
    }

    // On a 10 degree green slope the flop still spins back; downhill finishes no shorter and uphill no longer than flat.
    const FShotSimulationResult FlatFlop = Simulate(MakeLaunch(Flop), ESurface::Green, Ball());
    const FShotSimulationResult Downhill = Simulate(MakeLaunch(Flop), ESurface::Green, Ball(), 10.0f);
    const FShotSimulationResult Uphill = Simulate(MakeLaunch(Flop), ESurface::Green, Ball(), -10.0f);
    AddInfo(FString::Printf(TEXT("Flop on green: flat %.2f yd, downhill %.2f yd, uphill %.2f yd total"), FlatFlop.TotalM * YardsPerMeter,
        Downhill.TotalM * YardsPerMeter, Uphill.TotalM * YardsPerMeter));
    TestTrue(TEXT("Flat green flop reverses on first impact"), FlatFlop.bFirstImpactSpinback && FlatFlop.FirstImpactTangentOutMps < 0.0f);
    TestTrue(TEXT("Downhill is not meaningfully shorter than flat"), Downhill.TotalM * YardsPerMeter >= FlatFlop.TotalM * YardsPerMeter - 0.25f);
    TestTrue(TEXT("Uphill is not meaningfully longer than flat"), Uphill.TotalM * YardsPerMeter <= FlatFlop.TotalM * YardsPerMeter + 0.25f);
    TestTrue(TEXT("Downhill travels at least as far after impact as uphill"), RolloutYd(Downhill) >= RolloutYd(Uphill) - 0.10f);

    const float FlatPutt = Simulate(MakePutt(6.0f), ESurface::Green, Ball()).TotalM;
    const FShotSimulationResult DownhillPutt = Simulate(MakePutt(6.0f), ESurface::Green, Ball(), 2.0f);
    const FShotSimulationResult UphillPutt = Simulate(MakePutt(6.0f), ESurface::Green, Ball(), -2.0f);
    AddInfo(FString::Printf(TEXT("6 mph putt: flat %.2f ft, 2 deg downhill %.2f ft, 2 deg uphill %.2f ft"), FlatPutt * FeetPerMeter,
        DownhillPutt.TotalM * FeetPerMeter, UphillPutt.TotalM * FeetPerMeter));
    TestTrue(TEXT("A downhill putt rolls farther and stops"), DownhillPutt.bAtRest && DownhillPutt.TotalM > FlatPutt);
    TestTrue(TEXT("An uphill putt rolls shorter and stops"), UphillPutt.bAtRest && UphillPutt.TotalM < FlatPutt);
    TestTrue(TEXT("Level ground is the default"),
        Simulate(MakePutt(6.0f), ESurface::Green, Ball(), 0.0f).TotalM == ShotSimulator::Simulate(MakePutt(6.0f), ESurface::Green, Ball()).TotalM);
    return true;
}

#endif
