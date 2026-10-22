// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwaySandImpactTest, "UOpenGolf.Physics.Sand",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace SandImpactTestDetail
{
    const FVector3f LevelNormal(0.0f, 1.0f, 0.0f);
    constexpr float MaxRollSeconds = 60.0f;

    FLaunchData MakeLaunch(float SpeedMph, float VlaDeg, float BackSpinRpm)
    {
        FLaunchData Launch;
        Launch.SpeedMph = SpeedMph;
        Launch.VerticalLaunchDeg = VlaDeg;
        Launch.HorizontalLaunchDeg = 0.0f;
        Launch.BackSpinRpm = BackSpinRpm;
        Launch.SideSpinRpm = 0.0f;
        return Launch;
    }

    /** Forward speed left after a first impact at this speed and descent angle. */
    float TangentAfterImpact(const FSurfaceSettings& Surface, float SpeedMps, float AngleDeg, float BackSpinRpm)
    {
        const float Angle = FMath::DegreesToRadians(AngleDeg);
        FBallState State;
        State.Velocity = FVector3f(SpeedMps * FMath::Cos(Angle), -SpeedMps * FMath::Sin(Angle), 0.0f);
        State.Spin = FVector3f(0.0f, 0.0f, BackSpinRpm * GolfPhysics::RadPerRpm);
        return GolfPhysics::ResolveImpact(State, LevelNormal, Surface, FBounceProfile()).Velocity.X;
    }

    /** Meters a ball already rolling at SpeedMps travels on level ground of this surface. */
    float RollMeters(const FSurfaceSettings& Surface, float SpeedMps)
    {
        const FRolloutProfile Rollout;
        FBallState State;
        State.Velocity = FVector3f(SpeedMps, 0.0f, 0.0f);
        State.Spin = FVector3f::CrossProduct(LevelNormal, State.Velocity) / GolfPhysics::Radius;
        const int32 Steps = static_cast<int32>(MaxRollSeconds / GolfPhysics::SimulationDt);
        for (int32 Step = 0; Step < Steps && !GolfPhysics::CanRest(State, true, LevelNormal, Surface, Rollout); ++Step)
        {
            State = GolfPhysics::StepGround(State, LevelNormal, Surface, Rollout, 0.0f, GolfPhysics::SimulationDt);
        }
        return State.Position.X;
    }
} // namespace SandImpactTestDetail

bool FOpenFairwaySandImpactTest::RunTest(const FString& Parameters)
{
    using namespace SandImpactTestDetail;
    const FBallPhysicsProfile Profile;
    const FBallPhysicsProfile Reference = FBallPhysicsProfile::HistoricalReference();

    // The low wood that landed in the range bunker at about 30 m/s and 15 degrees.
    const FLaunchData LowWood = MakeLaunch(114.5f, 6.95f, 1932.0f);
    const FShotSimulationResult Sand = ShotSimulator::Simulate(LowWood, ESurface::Sand, Profile);
    const FShotSimulationResult Fairway = ShotSimulator::Simulate(LowWood, ESurface::Fairway, Profile);
    const FShotSimulationResult PortedSand = ShotSimulator::Simulate(LowWood, ESurface::Sand, Reference);
    const float SandRoll = Sand.TotalM - Sand.CarryM;
    const float FairwayRoll = Fairway.TotalM - Fairway.CarryM;
    const float PortedSandRoll = PortedSand.TotalM - PortedSand.CarryM;
    AddInfo(FString::Printf(TEXT("Low wood landing %.1f m/s at %.1f deg: sand %.2f m, ported sand %.2f m, fairway %.2f m after landing"),
        Sand.LandingSpeedMps, Sand.LandingAngleDeg, SandRoll, PortedSandRoll, FairwayRoll));
    TestTrue(TEXT("Low wood comes to rest in sand"), Sand.bValid && Sand.bAtRest);
    TestTrue(TEXT("A shallow full-shot landing stays within 3 m in sand"), SandRoll < 3.0f);
    TestTrue(TEXT("The same landing runs on fairway"), FairwayRoll > 15.0f);
    TestTrue(TEXT("Sand holds the landing better than the ported contact"), SandRoll < PortedSandRoll);

    const FShotSimulationResult MidIron = ShotSimulator::Simulate(MakeLaunch(93.9f, 22.9f, 5375.0f), ESurface::Sand, Profile);
    AddInfo(FString::Printf(TEXT("Mid iron landing %.1f m/s at %.1f deg: %.2f m after landing"), MidIron.LandingSpeedMps, MidIron.LandingAngleDeg,
        MidIron.TotalM - MidIron.CarryM));
    TestTrue(TEXT("A steep landing stops within 1 m in sand"), MidIron.bAtRest && FMath::Abs(MidIron.TotalM - MidIron.CarryM) < 1.0f);

    // The plow loss follows how hard the ball drives in, so a grazing skull keeps more pace.
    const FResolvedShotParams Shot;
    const FSurfaceSettings SandSurface = Shot.ResolveSurface(ESurface::Sand, Profile);
    const float Shallow = TangentAfterImpact(SandSurface, 30.0f, 15.0f, 1900.0f);
    const float Grazing = TangentAfterImpact(SandSurface, 35.0f, 5.0f, 1900.0f);
    AddInfo(FString::Printf(TEXT("Forward speed after a sand impact: 15 deg %.2f m/s, 5 deg %.2f m/s"), Shallow, Grazing));
    TestTrue(TEXT("A grazing contact keeps more forward speed than a shallow landing"), Grazing > Shallow + 3.0f);
    TestTrue(TEXT("The plow never reverses the ball"), Shallow >= 0.0f && Grazing >= 0.0f);

    const float RolledIn = RollMeters(SandSurface, 4.0f);
    AddInfo(FString::Printf(TEXT("A ball rolling into sand at 4 m/s stops after %.2f m"), RolledIn));
    TestTrue(TEXT("A ball rolling into sand stops within 2 m"), RolledIn < 2.0f);

    // Only sand plows, and the historical reference keeps the ported hard-sand contact.
    for (uint8 Index = 0; Index < static_cast<uint8>(ESurface::Count); ++Index)
    {
        const ESurface Surface = static_cast<ESurface>(Index);
        const FString Name = SurfaceCatalog::GetName(Surface);
        TestEqual(Name + TEXT(" reference has no plow"), Shot.ResolveSurface(Surface, Reference).ImpactPlowRatio, 0.0f);
        if (Surface != ESurface::Sand)
        {
            TestEqual(Name + TEXT(" has no plow"), Shot.ResolveSurface(Surface, Profile).ImpactPlowRatio, 0.0f);
        }
    }
    TestEqual(TEXT("Sand plows at the profile ratio"), SandSurface.ImpactPlowRatio, Profile.SandPlowRatio);
    return true;
}

#endif
