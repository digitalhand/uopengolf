// SPDX-License-Identifier: MIT
// Rollout regressions from recorded range shots (November 2026): spin about the contact normal checked a
// tilted-axis shot, a ball launched along the ground ran with no landing loss, and full shots stopped dead on
// fairway. Later checks: the turf's backspin loss needs load, the ground step reads only the ball's state (wind at its
// own height, drag without the launch regime), and a full shot struck along the ground keeps its struck grass drag.
// Cases are named by their launch values; the club labels on those recordings are not reliable.
// "Previous" is the gameplay profile before the turf recalibration and axis-aware spin.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayRolloutRegressionTest, "UOpenGolf.Physics.RolloutRegression",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace RolloutRegressionTestDetail
{
    const FVector3f LevelNormal(0.0f, 1.0f, 0.0f);

    FBallPhysicsProfile Previous()
    {
        FBallPhysicsProfile Profile;
        Profile.bRecalibratedTurf = false;
        Profile.Bounce.bAxisAwareSpin = false;
        Profile.Rollout.bAxisAwareSpin = false;
        Profile.Rollout.bCoupledContact = false;
        return Profile;
    }

    FLaunchData MakeLaunch(float SpeedMph, float VlaDeg, float HlaDeg, float BackSpinRpm, float SideSpinRpm)
    {
        FLaunchData Launch;
        Launch.SpeedMph = SpeedMph;
        Launch.VerticalLaunchDeg = VlaDeg;
        Launch.HorizontalLaunchDeg = HlaDeg;
        Launch.BackSpinRpm = BackSpinRpm;
        Launch.SideSpinRpm = SideSpinRpm;
        return Launch;
    }

    float Yards(float Meters)
    {
        return Meters * YardsPerMeter;
    }

    float RollYd(const FShotSimulationResult& Result)
    {
        return Yards(Result.TotalM - Result.CarryM);
    }

    /** Meters a ball already rolling at SpeedMps travels on level ground of this surface. */
    float RollMeters(const FSurfaceSettings& Surface, float SpeedMps)
    {
        const FRolloutProfile Rollout;
        FBallState State;
        State.Velocity = FVector3f(SpeedMps, 0.0f, 0.0f);
        State.Spin = FVector3f::CrossProduct(LevelNormal, State.Velocity) / GolfPhysics::Radius;
        const int32 Steps = static_cast<int32>(60.0f / GolfPhysics::SimulationDt);
        for (int32 Step = 0; Step < Steps && !GolfPhysics::CanRest(State, true, LevelNormal, Surface, Rollout); ++Step)
        {
            State = GolfPhysics::StepGround(State, LevelNormal, Surface, Rollout, 0.0f, GolfPhysics::SimulationDt);
        }
        return State.Position.X;
    }
} // namespace RolloutRegressionTestDetail

bool FOpenFairwayRolloutRegressionTest::RunTest(const FString& Parameters)
{
    using namespace RolloutRegressionTestDetail;
    const FBallPhysicsProfile Current;
    const FResolvedShotParams Shot;
    const FSurfaceSettings Fairway = Shot.ResolveSurface(ESurface::Fairway, Current);

    // Aerodynamic resistance remains present after landing, independently of grass drag.
    {
        auto Bare = Fairway;
        Bare.KineticFriction = Bare.RollingFriction = Bare.GrassViscosity = Bare.GrassFormDrag = 0;
        FBallState Moving;
        Moving.Velocity = FVector3f(30, 0, 0);
        FFlightEnvironment Vacuum;
        Vacuum.AirDensity = 0;
        FFlightEnvironment Headwind;
        Headwind.Wind = FVector3f(-10, 0, 0);
        const auto WithoutAir = GolfPhysics::StepGround(Moving, LevelNormal, Bare, Current.Rollout, 0, 0.01f, Vacuum);
        const auto InAir = GolfPhysics::StepGround(Moving, LevelNormal, Bare, Current.Rollout, 0, 0.01f);
        const auto IntoWind = GolfPhysics::StepGround(Moving, LevelNormal, Bare, Current.Rollout, 0, 0.01f, Headwind);
        TestEqual(TEXT("Vacuum without turf resistance preserves speed"), WithoutAir.Velocity.X, 30.0f);
        TestTrue(TEXT("Air slows a supported ball without turf resistance"), InAir.Velocity.X < WithoutAir.Velocity.X);
        TestTrue(TEXT("Ground drag uses relative air velocity"), IntoWind.Velocity.X < InAir.Velocity.X);

        // A ball on the ground is one radius above its lie wherever that lie is: the same rolling state above,
        // below or level with the launch point meets the same wind.
        FFlightEnvironment Profiled = Headwind;
        Profiled.WindReferenceHeightM = WindProfile::StandardReferenceHeightM;
        const auto At = [&](float HeightM, const FFlightEnvironment& Environment)
        {
            FBallState State = Moving;
            State.Position = FVector3f(5.0f, HeightM, 0.0f);
            return GolfPhysics::StepGround(State, LevelNormal, Bare, Current.Rollout, 0, 0.01f, Environment).Velocity;
        };
        const FVector3f Level = At(GolfPhysics::Radius, Profiled);
        AddInfo(FString::Printf(TEXT("Ground step in a profiled 10 m/s headwind: %.4f m/s level, %.4f m/s 5 m above the launch, %.4f m/s 5 m below"),
            Level.X, At(5.0f, Profiled).X, At(-5.0f, Profiled).X));
        TestTrue(TEXT("Ground wind does not depend on height above the launch point"),
            Level.Equals(At(5.0f, Profiled), 1e-6f) && Level.Equals(At(-5.0f, Profiled), 1e-6f));
        TestTrue(TEXT("A profiled wind is below the roughness at the ball's own height"), Level.Equals(At(GolfPhysics::Radius, FFlightEnvironment()), 1e-6f));
        TestTrue(TEXT("Uniform wind still reaches a ball on the ground at any height"), At(5.0f, Headwind).Equals(IntoWind.Velocity, 1e-6f));
    }

    // Air drag on the ground reads the ball, not the launch: environments resolved for a 150 mph, 11 deg drive and a
    // 75 mph, 35 deg pitch (different regime drag multipliers and high-launch drag) give the same ground step.
    {
        const FResolvedShotParams Driven = FResolvedShotParams::Resolve(Current, {}, 150.0f, 11.0f, 2300.0f, 2300.0f);
        const FResolvedShotParams Pitched = FResolvedShotParams::Resolve(Current, {}, 75.0f, 35.0f, 8000.0f, 8000.0f);
        TestTrue(TEXT("The two launches resolve to different flight environments"),
            Driven.Environment.DragScale != Pitched.Environment.DragScale || Driven.Environment.InitialLaunchAngleDeg != Pitched.Environment.InitialLaunchAngleDeg);
        FBallState Rolling;
        Rolling.Velocity = FVector3f(25.0f, 0.0f, 0.0f);
        Rolling.Spin = FVector3f::CrossProduct(LevelNormal, Rolling.Velocity) / GolfPhysics::Radius;
        const auto A = GolfPhysics::StepGround(Rolling, LevelNormal, Fairway, Current.Rollout, 0, GolfPhysics::SimulationDt, Driven.Environment, Driven.Flight);
        const auto B = GolfPhysics::StepGround(Rolling, LevelNormal, Fairway, Current.Rollout, 0, GolfPhysics::SimulationDt, Pitched.Environment, Pitched.Flight);
        TestTrue(TEXT("Same ground state, same ground step, whatever the launch"), A.Velocity.Equals(B.Velocity, 1e-6f) && A.Spin.Equals(B.Spin, 1e-4f));
    }

    // Resistance must finish at no-slip rolling, without creating energy. Include surfaces
    // outside the launch-monitor reference and both signs of spin rather than selected club names.
    for (int32 Index = 0; Index < static_cast<int32>(ESurface::Count); ++Index)
    {
        const auto Surface = Shot.ResolveSurface(static_cast<ESurface>(Index), Current);
        for (float Speed : {0.1f, 2.0f, 15.0f, 45.0f})
        {
            for (float SpinSign : {-1.0f, 0.0f, 1.0f})
            {
                FBallState State;
                State.Velocity = FVector3f(Speed, 0, 0);
                State.Spin = SpinSign * FVector3f::CrossProduct(LevelNormal, State.Velocity) / GolfPhysics::Radius;
                for (int32 Step = 0; Step < 240; ++Step)
                {
                    const auto Next = GolfPhysics::StepGround(State, LevelNormal, Surface, Current.Rollout, 0, GolfPhysics::SimulationDt);
                    if (!TestTrue(TEXT("Passive contact does not add energy"), GolfPhysics::KineticEnergy(Next) <= GolfPhysics::KineticEnergy(State) + 1e-5f))
                        break;
                    State = Next;
                }
            }
        }
        FBallState Rolling;
        Rolling.Velocity = FVector3f(2, 0, 0);
        Rolling.Spin = FVector3f::CrossProduct(LevelNormal, Rolling.Velocity) / GolfPhysics::Radius;
        const auto Next = GolfPhysics::StepGround(Rolling, LevelNormal, Surface, Current.Rollout, 0, GolfPhysics::SimulationDt);
        TestTrue(TEXT("Rolling contact has no residual slip"),
            (Next.Velocity + FVector3f::CrossProduct(Next.Spin, -GolfPhysics::Radius * LevelNormal)).Size() < 1e-4f);
    }

    for (const auto& Launch : {MakeLaunch(67.47f, -0.14f, 16.22f, 1406, 4599),
             MakeLaunch(100.17f, 0.78f, -6.69f, 2368, -1011), MakeLaunch(113.9f, 0.6f, -3.8f, 2068, 0),
             MakeLaunch(109, 13.8f, 0, 4242, 0), MakeLaunch(78, 23.5f, 0, 7528, 0)})
    {
        FShotSimulationOptions Fine;
        Fine.Dt = 1.0f / 240.0f;
        const auto A = ShotSimulator::Simulate(Launch, ESurface::Fairway, Current, {}, 60, LevelNormal, Fine);
        Fine.Dt = 1.0f / 480.0f;
        const auto B = ShotSimulator::Simulate(Launch, ESurface::Fairway, Current, {}, 60, LevelNormal, Fine);
        TestTrue(TEXT("Fine timesteps converge within 1 percent or 0.25 yd"),
            FMath::Abs(A.TotalM - B.TotalM) <= FMath::Max(0.25f / YardsPerMeter, 0.01f * B.TotalM));
        TestTrue(TEXT("Fine simulations rest"), A.bAtRest && B.bAtRest);
    }

    // Spin about the normal (all the sidespin on flat ground) never touches the turf.
    {
        FBallState Landing;
        Landing.Velocity = FVector3f(20.0f * FMath::Cos(FMath::DegreesToRadians(30.0f)), -20.0f * FMath::Sin(FMath::DegreesToRadians(30.0f)), 0.0f);
        Landing.Spin = FVector3f(0.0f, 0.0f, 4000.0f * GolfPhysics::RadPerRpm);
        FBallState Sidespin = Landing;
        Sidespin.Spin.Y = -3000.0f * GolfPhysics::RadPerRpm;
        const FLandingResponse Bite{0.5f, 0.0f, 8.0f, 20.0f};
        const FSurfaceSettings PreviousFairway = Shot.ResolveSurface(ESurface::Fairway, Previous());
        const auto Out = [&](const FBallState& In, const FSurfaceSettings& Surface, const FBounceProfile& Bounce)
        { return GolfPhysics::ResolveImpact(In, LevelNormal, Surface, Bounce, true, Bite).Velocity; };
        const FVector3f Plain = Out(Landing, Fairway, Current.Bounce);
        const FVector3f WithSide = Out(Sidespin, Fairway, Current.Bounce);
        AddInfo(FString::Printf(TEXT("Impact out: backspin only %s, with 3000 rpm about the normal %s"), *Plain.ToString(), *WithSide.ToString()));
        TestTrue(TEXT("Spin about the normal does not change the impact"), Plain.Equals(WithSide, 1e-5f));
        TestTrue(TEXT("The previous model read it as backspin"),
            FMath::Abs(Out(Landing, PreviousFairway, Previous().Bounce).X - Out(Sidespin, PreviousFairway, Previous().Bounce).X) > 0.1f);

        FBallState Pivoting;
        Pivoting.Spin = FVector3f(0.0f, 50.0f, 0.0f);
        TestTrue(TEXT("A ball that only pivots can rest"), GolfPhysics::CanRest(Pivoting, true, LevelNormal, Fairway, Current.Rollout));
        TestFalse(TEXT("The previous rest check waited for it"), GolfPhysics::CanRest(Pivoting, true, LevelNormal, Fairway, Previous().Rollout));
    }

    // Turf takes its landing loss from the impact alone: the same impact state leaves the same way whatever
    // launch (and so whatever landing bucket) produced it.
    {
        const auto Resolved = [&](float SpeedMph, float VlaDeg, float SpinRpm)
        {
            const FResolvedShotParams Params = FResolvedShotParams::Resolve(Current, {}, SpeedMph, VlaDeg, SpinRpm, SpinRpm);
            return TPair<FSurfaceSettings, FLandingResponse>(Params.ResolveSurface(ESurface::Fairway, Current), Params.Landing);
        };
        const auto Driven = Resolved(150.0f, 11.0f, 2300.0f);
        const auto Pitched = Resolved(75.0f, 28.0f, 8000.0f);
        TestTrue(TEXT("The two launches resolve to different landing responses"), Driven.Value.SpinBiteScale != Pitched.Value.SpinBiteScale);
        FBallState Landing;
        Landing.Velocity = FVector3f(15.0f, -12.0f, 1.0f);
        Landing.Spin = FVector3f(0.0f, 0.0f, 5000.0f * GolfPhysics::RadPerRpm);
        const FBallState A = GolfPhysics::ResolveImpact(Landing, LevelNormal, Driven.Key, Current.Bounce, true, Driven.Value);
        const FBallState B = GolfPhysics::ResolveImpact(Landing, LevelNormal, Pitched.Key, Current.Bounce, true, Pitched.Value);
        // The two resolved surfaces differ only by float rounding in the blended regime multipliers (all neutral for turf).
        AddInfo(FString::Printf(TEXT("Same landing from two launches: velocity %s / %s, spin %s / %s"), *A.Velocity.ToString(), *B.Velocity.ToString(),
            *A.Spin.ToString(), *B.Spin.ToString()));
        TestTrue(TEXT("Same impact state, same outgoing state on fairway"), A.Velocity.Equals(B.Velocity, 1e-4f) && A.Spin.Equals(B.Spin, 1e-2f));
    }

    // The landing loss itself, with the friction impulse switched off so nothing else changes the tangential
    // speed: it grows with impact angle and with backspin, and a ball skimming the turf keeps its pace.
    {
        FSurfaceSettings Frictionless = Fairway;
        Frictionless.KineticFriction = 0.0f;
        const FLandingResponse NoBite;
        const auto Kept = [&](float AngleDeg, float BackspinRpm)
        {
            const float Angle = FMath::DegreesToRadians(AngleDeg);
            FBallState Landing;
            Landing.Velocity = FVector3f(20.0f * FMath::Cos(Angle), -20.0f * FMath::Sin(Angle), 0.0f);
            Landing.Spin = FVector3f(0.0f, 0.0f, BackspinRpm * GolfPhysics::RadPerRpm);
            return GolfPhysics::ResolveImpact(Landing, LevelNormal, Frictionless, Current.Bounce, true, NoBite).Velocity.X / Landing.Velocity.X;
        };
        AddInfo(FString::Printf(TEXT("Fairway landing keeps %.3f of a 0.5 deg skim, %.3f at 15 deg, %.3f at 45 deg"),
            Kept(0.5f, 0.0f), Kept(15.0f, 0.0f), Kept(45.0f, 0.0f)));
        TestTrue(TEXT("A steeper landing keeps less of its tangential speed"), Kept(45.0f, 0.0f) < Kept(15.0f, 0.0f));
        TestTrue(TEXT("A ball skimming the turf keeps nearly all of it"), Kept(0.5f, 0.0f) > 0.95f);
        TestTrue(TEXT("Without friction there is no grip, so backspin takes nothing"), Kept(30.0f, 3000.0f) == Kept(30.0f, 0.0f));
    }

    // The backspin loss, with friction on: the same landing on fairway with and without LandingSpinLoss, so only
    // the spin term differs.
    {
        FSurfaceSettings NoSpinLoss = Fairway;
        NoSpinLoss.LandingSpinLoss = 0.0f;
        const FLandingResponse NoBite;
        const auto Out = [&](const FSurfaceSettings& Surface, float BackspinRpm)
        {
            const float Angle = FMath::DegreesToRadians(30.0f);
            FBallState Landing;
            Landing.Velocity = FVector3f(20.0f * FMath::Cos(Angle), -20.0f * FMath::Sin(Angle), 0.0f);
            Landing.Spin = FVector3f(0.0f, 0.0f, BackspinRpm * GolfPhysics::RadPerRpm);
            return GolfPhysics::ResolveImpact(Landing, LevelNormal, Surface, Current.Bounce, true, NoBite).Velocity.X;
        };
        AddInfo(FString::Printf(TEXT("30 deg, 20 m/s fairway landing with 3000 rpm backspin leaves at %.2f m/s (%.2f m/s without the spin loss)"),
            Out(Fairway, 3000.0f), Out(NoSpinLoss, 3000.0f)));
        TestTrue(TEXT("Backspin removes tangential speed at landing"),
            Fairway.LandingSpinLoss > 0.0f ? Out(Fairway, 3000.0f) < Out(NoSpinLoss, 3000.0f) : Out(Fairway, 3000.0f) == Out(NoSpinLoss, 3000.0f));
        TestTrue(TEXT("Topspin does not grip like backspin"), Out(Fairway, -3000.0f) == Out(NoSpinLoss, -3000.0f));
    }

    // A grazing contact: the backspin loss needs load like friction does, so it fades out as the normal speed goes to
    // zero instead of switching on in full at the first touch (slow, high-spin chips and terrain seams).
    for (const float TangentSpeed : {2.0f, 8.0f})
    {
        for (const float BackspinRpm : {2000.0f, 8000.0f})
        {
            const auto Kept = [&](float NormalSpeed)
            {
                FBallState Contact;
                Contact.Velocity = FVector3f(TangentSpeed, -NormalSpeed, 0.0f);
                Contact.Spin = FVector3f(0.0f, 0.0f, BackspinRpm * GolfPhysics::RadPerRpm);
                return GolfPhysics::ResolveImpact(Contact, LevelNormal, Fairway, Current.Bounce, true, FLandingResponse()).Velocity.X / TangentSpeed;
            };
            const FString Case = FString::Printf(TEXT("%.0f m/s, %.0f rpm"), TangentSpeed, BackspinRpm);
            TestEqual(Case + TEXT(": a contact with no normal speed changes nothing"), Kept(0.0f), 1.0f);
            TestTrue(Case + TEXT(": a 0.1 mm/s touch keeps 99.9% of its speed"), Kept(1e-4f) >= 0.999f);
            float Last = Kept(1e-4f);
            FString Trend = FString::Printf(TEXT("%.4f"), Last);
            bool bMonotonic = true;
            for (const float NormalSpeed : {1e-3f, 1e-2f, 0.1f, 0.25f, 0.5f, 1.0f, 2.0f})
            {
                const float Now = Kept(NormalSpeed);
                bMonotonic &= Now <= Last + 1e-5f;
                Trend += FString::Printf(TEXT(" %.4f"), Now);
                Last = Now;
            }
            AddInfo(Case + TEXT(", kept at normal speed 1e-4 to 2 m/s: ") + Trend);
            TestTrue(Case + TEXT(": a harder contact never keeps more"), bMonotonic);
        }
    }

    // 107.8 mph at 8.4 deg with an 85 degree spin axis (-401 rpm back, 4581 rpm side): it was checked as if it were backspin.
    {
        const FLaunchData Launch = MakeLaunch(107.82f, 8.36f, -10.65f, -401.0f, 4581.0f);
        const FShotSimulationResult Now = ShotSimulator::Simulate(Launch, ESurface::Fairway, Current);
        const FShotSimulationResult Before = ShotSimulator::Simulate(Launch, ESurface::Fairway, Previous());
        AddInfo(FString::Printf(TEXT("Tilted-axis 107.8 mph roll: %.1f yd (previous %.1f yd)"), RollYd(Now), RollYd(Before)));
        TestTrue(TEXT("Tilted-axis shot rolls at least 2.5 times farther"), RollYd(Now) >= 2.5f * RollYd(Before));
        TestTrue(TEXT("Its landing bucket reads backspin, not total spin"), Now.LandingBucket == ELandingBucket::Run);
        TestEqual(TEXT("Carry is unchanged"), Now.CarryM, Before.CarryM, 0.001f);
    }

    // 67.5 mph at -0.14 deg and 100.2 mph at 0.78 deg: launched along the ground, they used to run 220 and 396 yd on flat fairway.
    {
        const FShotSimulationResult Slow = ShotSimulator::Simulate(MakeLaunch(67.47f, -0.14f, 16.22f, 1406.0f, 4599.0f), ESurface::Fairway, Current);
        const FShotSimulationResult Fast = ShotSimulator::Simulate(MakeLaunch(100.17f, 0.78f, -6.69f, 2368.0f, -1011.0f), ESurface::Fairway, Current);
        AddInfo(FString::Printf(TEXT("Ground-launched totals: 67.5 mph %.1f yd, 100.2 mph %.1f yd"), Yards(Slow.TotalM), Yards(Fast.TotalM)));
        // No reference distance exists for these recordings. Validate the solver, not an invented target.
        TestTrue(TEXT("Ground-launched 67.5 mph shot has finite positive travel"), FMath::IsFinite(Slow.TotalM) && Slow.TotalM > 0.0f);
        TestEqual(TEXT("Supported negative launch has no invented carry"), Slow.CarryM, 0.0f);
        TestEqual(TEXT("Supported negative launch contacts at time zero"), Slow.HangTimeS, 0.0f);
        TestTrue(TEXT("Ground-launched 100.2 mph shot has finite positive travel"), FMath::IsFinite(Fast.TotalM) && Fast.TotalM > 0.0f);
        TestTrue(TEXT("Both come to rest"), Slow.bAtRest && Fast.bAtRest);
    }

    // A full shot struck into or along the ground (launch at or below 0 deg) rolls with the turf's struck grass drag,
    // as the launch-monitor export's six rows launched below 0 deg do; one that leaves the ground at all does not. So the total steps up
    // at 0 deg at full speed, and only there: each side keeps its own continuity, and a putt-speed launch has none.
    for (const float Speed : {60.0f, 80.0f, 100.0f})
    {
        const float Struck = Yards(ShotSimulator::Simulate(MakeLaunch(Speed, 0.0f, 0, 2000, 0), ESurface::Fairway, Current).TotalM);
        const float Flown = Yards(ShotSimulator::Simulate(MakeLaunch(Speed, 0.1f, 0, 2000, 0), ESurface::Fairway, Current).TotalM);
        AddInfo(FString::Printf(TEXT("%.0f mph, 2000 rpm: %.1f yd struck along the ground at 0 deg, %.1f yd launched at 0.1 deg"), Speed, Struck, Flown));
        TestTrue(FString::Printf(TEXT("%.0f mph struck along the ground rolls farther than a 0.1 deg launch"), Speed), Struck > Flown);
    }
    for (const float SpeedMps : {5.0f, 10.0f})
    {
        float PreviousTotal = 0.0f;
        for (int32 Sample = -5; Sample <= 5; ++Sample)
        {
            const float Total = ShotSimulator::Simulate(MakeLaunch(SpeedMps / 0.44704f, Sample * 0.1f, 0, 0, 0), ESurface::Fairway, Current).TotalM;
            if (PreviousTotal > 0.0f)
                TestTrue(FString::Printf(TEXT("%.0f m/s putt-speed launch: no jump above 3 percent per 0.1 degree across 0 deg"), SpeedMps),
                    FMath::Abs(Total - PreviousTotal) <= 0.03f * FMath::Max(Total, PreviousTotal));
            PreviousTotal = Total;
        }
    }

    // Fine samples around support/flight transition must not hide a discontinuity between the existing one-degree
    // samples. The 0 to 0.1 deg pair is the struck-along-the-ground step above, by design.
    for (float Speed : {60.0f, 80.0f, 100.0f})
    {
        float PreviousTotal = 0;
        for (int32 Sample = -10; Sample <= 10; ++Sample)
        {
            const auto Result = ShotSimulator::Simulate(MakeLaunch(Speed, Sample * 0.1f, 0, 2000, 0), ESurface::Fairway, Current);
            if (PreviousTotal > 0 && Sample != 1)
                TestTrue(TEXT("No support transition jump above 3 percent per 0.1 degree"),
                    FMath::Abs(Result.TotalM - PreviousTotal) <= 0.03f * FMath::Max(Result.TotalM, PreviousTotal));
            PreviousTotal = Result.TotalM;
        }
    }

    // Total distance follows launch angle without a jump where the ball first leaves the ground, on each side of the
    // struck-along-the-ground step (0 to 1 deg is that step).
    for (const TPair<float, float>& Case : {TPair<float, float>(60.0f, 1500.0f), TPair<float, float>(80.0f, 2000.0f), TPair<float, float>(100.0f, 2400.0f)})
    {
        float LastTotal = -1.0f, WorstStep = 0.0f, WorstVla = 0.0f;
        FString Totals;
        for (int32 Vla = -3; Vla <= 12; ++Vla)
        {
            const float Total = Yards(ShotSimulator::Simulate(MakeLaunch(Case.Key, static_cast<float>(Vla), 0.0f, Case.Value, 0.0f), ESurface::Fairway, Current).TotalM);
            Totals += FString::Printf(TEXT(" %.0f"), Total);
            if (LastTotal > 0.0f && Vla != 1)
            {
                const float Step = FMath::Abs(Total - LastTotal) / FMath::Max(Total, LastTotal);
                if (Step > WorstStep)
                {
                    WorstStep = Step;
                    WorstVla = static_cast<float>(Vla);
                }
            }
            LastTotal = Total;
        }
        AddInfo(FString::Printf(TEXT("%.0f mph, %.0f rpm, -3 to 12 deg:%s yd"), Case.Key, Case.Value, *Totals));
        TestTrue(FString::Printf(TEXT("%.0f mph: no 1 degree step changes total by more than 15%% (worst %.0f%% at %.0f deg)"), Case.Key, WorstStep * 100.0f, WorstVla),
            WorstStep <= 0.15f);
    }

    // A mid launch releases on fairway and a high, high-spin one holds; both used to stop dead.
    {
        const FShotSimulationResult Mid = ShotSimulator::Simulate(MakeLaunch(109.0f, 13.8f, 0.0f, 4242.0f, 0.0f), ESurface::Fairway, Current);
        const FShotSimulationResult High = ShotSimulator::Simulate(MakeLaunch(78.0f, 23.5f, 0.0f, 7528.0f, 0.0f), ESurface::Fairway, Current);
        AddInfo(FString::Printf(TEXT("Roll: 109 mph / 13.8 deg / 4242 rpm %.1f yd, 78 mph / 23.5 deg / 7528 rpm %.1f yd"), RollYd(Mid), RollYd(High)));
        TestTrue(TEXT("109 mph, 13.8 deg, 4,242 rpm releases 8-16 yd"), RollYd(Mid) > 8.0f && RollYd(Mid) < 16.0f);
        TestTrue(TEXT("78 mph, 23.5 deg, 7,528 rpm holds within 0.5-5 yd"), RollYd(High) > 0.5f && RollYd(High) < 5.0f);
    }

    // Turf speed: fairway is slower than a 10 ft green on the stimpmeter, and rough slower again.
    {
        const float FairwayFeet = RollMeters(Fairway, GreenSpeed::StimpmeterSpeedMps) / 0.3048f;
        const float GreenFeet = RollMeters(Shot.ResolveSurface(ESurface::Green, Current), GreenSpeed::StimpmeterSpeedMps) / 0.3048f;
        const float RoughFeet = RollMeters(Shot.ResolveSurface(ESurface::Rough, Current), GreenSpeed::StimpmeterSpeedMps) / 0.3048f;
        const float PreviousFeet = RollMeters(Shot.ResolveSurface(ESurface::Fairway, Previous()), GreenSpeed::StimpmeterSpeedMps) / 0.3048f;
        AddInfo(FString::Printf(TEXT("Stimpmeter: green %.1f ft, fairway %.1f ft (previous %.1f ft), rough %.1f ft"), GreenFeet, FairwayFeet, PreviousFeet, RoughFeet));
        TestTrue(TEXT("Fairway is slower than the green"), FairwayFeet < GreenFeet);
        TestTrue(TEXT("Rough is slower than fairway"), RoughFeet < FairwayFeet);
    }

    // Green and sand are not part of the turf recalibration.
    for (const ESurface Surface : {ESurface::Green, ESurface::Sand})
    {
        const FSurfaceSettings Now = Shot.ResolveSurface(Surface, Current);
        const FSurfaceSettings Before = Shot.ResolveSurface(Surface, Previous());
        const FString Name = SurfaceCatalog::GetName(Surface);
        TestEqual(Name + TEXT(" grass drag unchanged"), Now.GrassViscosity, Before.GrassViscosity);
        TestEqual(Name + TEXT(" has no form drag"), Now.GrassFormDrag, 0.0f);
        TestTrue(Name + TEXT(" keeps the ported landing (retention and bite)"),
            Now.LandingAngleLoss == 0.0f && Now.LandingSpinLoss == 0.0f && Before.LandingAngleLoss == 0.0f && Before.LandingSpinLoss == 0.0f);
        TestTrue(Name + TEXT(" has no struck-along-the-ground drag"), Now.StruckViscosityScale == 1.0f && Now.StruckFormDragScale == 1.0f);
    }
    // Every turf surface takes the state-based landing; the ported catalog keeps none.
    for (const ESurface Surface : {ESurface::Fairway, ESurface::FairwaySoft, ESurface::Rough, ESurface::Firm, ESurface::Fringe})
    {
        const FString Name = SurfaceCatalog::GetName(Surface);
        TestTrue(Name + TEXT(" uses the state-based landing"), Shot.ResolveSurface(Surface, Current).LandingAngleLoss > 0.0f);
        TestEqual(Name + TEXT(" keeps the ported landing on the previous profile"), Shot.ResolveSurface(Surface, Previous()).LandingAngleLoss, 0.0f);
        const FSurfaceSettings Before = Shot.ResolveSurface(Surface, Previous());
        TestTrue(Name + TEXT(" has no struck-along-the-ground drag on the previous profile"), Before.StruckViscosityScale == 1.0f && Before.StruckFormDragScale == 1.0f);
    }
    TestEqual(TEXT("A putt-speed launch gets none of the struck drag"), GolfPhysics::StruckAlongGroundWeight(10.0f), 0.0f);
    TestEqual(TEXT("A full-speed launch gets all of it"), GolfPhysics::StruckAlongGroundWeight(20.0f), 1.0f);
    return true;
}

#endif
