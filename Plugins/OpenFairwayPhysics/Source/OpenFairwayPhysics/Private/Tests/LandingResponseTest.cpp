// SPDX-License-Identifier: MIT
// The landing buckets and the first-impact bite: classification, interpolation and continuity across every bucket edge.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLandingResponseTest, "UOpenGolf.Physics.LandingResponse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLandingResponseTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Low spin runs"), LandingBucket::Classify(10.0f, 2000.0f) == ELandingBucket::Run);
    TestTrue(TEXT("Mid spin releases"), LandingBucket::Classify(20.0f, 5000.0f) == ELandingBucket::Release);
    TestTrue(TEXT("High spin checks"), LandingBucket::Classify(30.0f, 7000.0f) == ELandingBucket::Check);
    TestTrue(TEXT("Very high spin, high launch spins back"), LandingBucket::Classify(40.0f, 10000.0f) == ELandingBucket::Spinback);
    TestTrue(TEXT("Very high spin, low launch checks"), LandingBucket::Classify(12.0f, 10000.0f) == ELandingBucket::Check);

    const FBallPhysicsProfile Profile;
    ELandingBucket Bucket;
    const FLandingResponse Midpoint = Profile.ResolveLandingResponse(21.5f, 5625.0f, Bucket);
    TestEqual(TEXT("Landing bite midpoint"), Midpoint.SpinBiteScale, 0.675f, 1e-6f);
    TestEqual(TEXT("Landing speed ramp preserved"), Midpoint.SpeedRampEndMps, 20.0f, 1e-6f);
    const FLandingResponse Low = Profile.ResolveLandingResponse(-20.0f, 0.0f, Bucket);
    TestEqual(TEXT("Below the lowest launch and spin the Run response holds"), Low.SpinBiteScale, 0.0f, 1e-6f);
    const FLandingResponse High = Profile.ResolveLandingResponse(80.0f, 15000.0f, Bucket);
    TestEqual(TEXT("Above the highest launch and spin the Spinback response holds"), High.MaxSpinbackMps, 1.2f, 1e-6f);

    // Cross each bucket edge, their intersections and each interpolation knot: the response has no step.
    for (float Vla : {5.0f, 10.0f, 14.0f, 18.0f, 21.5f, 25.0f, 27.5f, 30.0f, 31.5f, 33.0f, 36.5f})
    {
        for (float Rpm : {1250.0f, 2500.0f, 3250.0f, 4000.0f, 4750.0f, 5500.0f, 6500.0f, 7500.0f, 8500.0f})
        {
            const auto Below = FResolvedShotParams::Resolve(Profile, FAirConditions(), Vla - 0.001f, Rpm - 0.01f);
            const auto Above = FResolvedShotParams::Resolve(Profile, FAirConditions(), Vla + 0.001f, Rpm + 0.01f);
            const FString Label = FString::Printf(TEXT("Boundary %.2f deg / %.0f rpm"), Vla, Rpm);
            TestEqual(Label + TEXT(" bite continuous"), Below.Landing.SpinBiteScale, Above.Landing.SpinBiteScale, 0.001f);
            TestEqual(Label + TEXT(" spinback continuous"), Below.Landing.MaxSpinbackMps, Above.Landing.MaxSpinbackMps, 0.001f);
            TestEqual(Label + TEXT(" ramp start continuous"), Below.Landing.SpeedRampStartMps, Above.Landing.SpeedRampStartMps, 0.001f);
            TestEqual(Label + TEXT(" ramp end continuous"), Below.Landing.SpeedRampEndMps, Above.Landing.SpeedRampEndMps, 0.001f);
        }
    }

    // Only backspin bites: a launch with heavy sidespin is bucketed by its backspin.
    const auto Sidespin = FResolvedShotParams::Resolve(Profile, FAirConditions(), 30.0f, 9000.0f, 3000.0f);
    TestTrue(TEXT("Backspin, not total spin, picks the bucket"), Sidespin.LandingBucket == ELandingBucket::Run);
    return true;
}

#endif
