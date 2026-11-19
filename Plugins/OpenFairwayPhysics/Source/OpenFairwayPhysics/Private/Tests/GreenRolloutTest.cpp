// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"
#include "PhysicsTestUtils.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayGreenRolloutTest, "UOpenGolf.Physics.GreenRollout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayGreenRolloutTest::RunTest(const FString& Parameters)
{
    using namespace OpenFairwayTest;
    const FBallPhysicsProfile Profile;
    const FResolvedShotParams Shot;
    const FSurfaceSettings Fairway = Shot.ResolveSurface(ESurface::Fairway, Profile);
    const FSurfaceSettings Green = Shot.ResolveSurface(ESurface::Green, Profile);

    // The fairway is slower than a stimp-10 green after any landing spin.
    for (const float SpinRpm : {0.0f, 1000.0f, 2000.0f, 2500.0f, 6000.0f})
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
        GreenSpeed::DefaultStimpFeet * MetersPerFoot, 0.1f);
    for (float Stimp = GreenSpeed::MinStimpFeet; Stimp <= GreenSpeed::MaxStimpFeet; Stimp += 1.0f)
    {
        FBallPhysicsProfile Selected;
        Selected.GreenStimpFeet = Stimp;
        TestEqual(FString::Printf(TEXT("Stimp %.0f measures within 0.1 m"), Stimp),
            RollMeters(Shot.ResolveSurface(ESurface::Green, Selected), GreenSpeed::StimpmeterSpeedMps, 0.0f), Stimp * MetersPerFoot, 0.1f);
    }

    return true;
}

#endif
