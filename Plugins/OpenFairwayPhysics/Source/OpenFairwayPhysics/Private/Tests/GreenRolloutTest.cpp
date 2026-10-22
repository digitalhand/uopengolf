// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayGreenRolloutTest, "UOpenGolf.Physics.GreenRollout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace GreenRolloutTestDetail
{
    const FVector3f LevelNormal(0.0f, 1.0f, 0.0f);
    constexpr float MaxRollSeconds = 60.0f;

    /** Meters a ball already rolling at SpeedMps travels on level ground of this surface. */
    float RollMeters(const FSurfaceSettings& Surface, float SpeedMps, float ImpactSpinRpm)
    {
        const FRolloutProfile Rollout;
        FBallState State;
        State.Velocity = FVector3f(SpeedMps, 0.0f, 0.0f);
        State.Spin = FVector3f::CrossProduct(LevelNormal, State.Velocity) / GolfPhysics::Radius;
        const int32 Steps = static_cast<int32>(MaxRollSeconds / GolfPhysics::SimulationDt);
        for (int32 Step = 0; Step < Steps && !GolfPhysics::CanRest(State, true, LevelNormal, Surface, Rollout); ++Step)
        {
            State = GolfPhysics::StepGround(State, LevelNormal, Surface, Rollout, ImpactSpinRpm, GolfPhysics::SimulationDt);
        }
        return State.Position.X;
    }
} // namespace GreenRolloutTestDetail

bool FOpenFairwayGreenRolloutTest::RunTest(const FString& Parameters)
{
    using namespace GreenRolloutTestDetail;
    const FBallPhysicsProfile Profile;
    const FResolvedShotParams Shot;
    const FSurfaceSettings Fairway = Shot.ResolveSurface(ESurface::Fairway, Profile);
    const FSurfaceSettings Green = Shot.ResolveSurface(ESurface::Green, Profile);

    // Full shots land with at least this much spin. Below about 1,250 rpm the fairway's low-spin
    // calibration still rolls a slow ball farther than a stimp-10 green.
    for (const float SpinRpm : {2000.0f, 2500.0f, 6000.0f})
    {
        for (const float SpeedMps : {2.0f, 3.0f, 5.0f, 8.0f, 12.0f})
        {
            const float OnGreen = RollMeters(Green, SpeedMps, SpinRpm);
            const float OnFairway = RollMeters(Fairway, SpeedMps, SpinRpm);
            TestTrue(FString::Printf(TEXT("%.0f m/s after a %.0f rpm landing: green %.2f m, fairway %.2f m"), SpeedMps, SpinRpm, OnGreen, OnFairway),
                OnGreen >= OnFairway);
        }
    }

    FBallPhysicsProfile Faster;
    Faster.GreenStimpFeet = 13.0f;
    TestTrue(TEXT("A faster green rolls farther"), RollMeters(Shot.ResolveSurface(ESurface::Green, Faster), 5.0f, 2500.0f) > RollMeters(Green, 5.0f, 2500.0f));
    TestEqual(TEXT("A stimpmeter ball stops at the stimp distance"), RollMeters(Green, GreenSpeed::StimpmeterSpeedMps, 0.0f),
        GreenSpeed::DefaultStimpFeet * 0.3048f, 0.1f);

    // The historical reference keeps the ported green, which stops a fast roll short of the fairway.
    const FBallPhysicsProfile Reference = FBallPhysicsProfile::HistoricalReference();
    const FSurfaceSettings ReferenceGreen = Shot.ResolveSurface(ESurface::Green, Reference);
    TestEqual(TEXT("Reference green keeps the ported drag"), ReferenceGreen.GrassViscosity, GreenSpeed::ReferenceGrassViscosity);
    TestTrue(TEXT("Reference green rolls a fast ball shorter than fairway"),
        RollMeters(ReferenceGreen, 8.0f, 2500.0f) < RollMeters(Shot.ResolveSurface(ESurface::Fairway, Reference), 8.0f, 2500.0f));
    return true;
}

#endif
