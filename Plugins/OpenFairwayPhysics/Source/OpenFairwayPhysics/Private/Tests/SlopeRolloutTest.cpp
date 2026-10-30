// SPDX-License-Identifier: MIT
// Slope hold and roll, after ogolf-physics SlopeRollingResistanceTests at
// e6d793ae9477efaf55dc9078d7260f76bb532a7d. The reference checks BallPhysics.GetSlopeDriveForce, a
// Godot-side force query; its simulation runs GolfPhysics.StepGround, so these check that instead.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwaySlopeRolloutTest, "UOpenGolf.Physics.SlopeRollout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace SlopeRolloutTestDetail
{
    constexpr float SettleSeconds = 1.0f;

    /** Ground tilted downhill along +X by SlopeDeg. */
    FVector3f DownhillNormal(float SlopeDeg)
    {
        const float Slope = FMath::DegreesToRadians(SlopeDeg);
        return FVector3f(FMath::Sin(Slope), FMath::Cos(Slope), 0.0f);
    }

    /** Largest slope the surface holds a resting ball on: atan(rolling friction * slope resistance ratio). */
    float HoldAngleDeg(const FSurfaceSettings& Surface)
    {
        return FMath::RadiansToDegrees(FMath::Atan(Surface.RollingFriction * FRolloutProfile().SlopeResistanceRatio));
    }

    /** Speed after a ball set down at rest on the slope has had SettleSeconds to move. */
    float SpeedAfterSettling(const FSurfaceSettings& Surface, float SlopeDeg)
    {
        const FRolloutProfile Rollout;
        const FVector3f Normal = DownhillNormal(SlopeDeg);
        FBallState State;
        const int32 Steps = static_cast<int32>(SettleSeconds / GolfPhysics::SimulationDt);
        for (int32 Step = 0; Step < Steps; ++Step)
        {
            State = GolfPhysics::StepGround(State, Normal, Surface, Rollout, 0.0f, GolfPhysics::SimulationDt);
        }
        return State.Velocity.Size();
    }
} // namespace SlopeRolloutTestDetail

bool FOpenFairwaySlopeRolloutTest::RunTest(const FString& Parameters)
{
    using namespace SlopeRolloutTestDetail;
    const FResolvedShotParams Shot;
    for (const FBallPhysicsProfile& Profile : {FBallPhysicsProfile(), FBallPhysicsProfile::HistoricalReference()})
    {
        const FSurfaceSettings Fairway = Shot.ResolveSurface(ESurface::Fairway, Profile);
        const FSurfaceSettings Rough = Shot.ResolveSurface(ESurface::Rough, Profile);
        const FSurfaceSettings Green = Shot.ResolveSurface(ESurface::Green, Profile);
        const float FairwayHold = HoldAngleDeg(Fairway);
        const float GreenHold = HoldAngleDeg(Green);
        const float RoughHold = HoldAngleDeg(Rough);
        AddInfo(FString::Printf(TEXT("Hold angles: fairway %.2f, green %.2f, rough %.2f deg"), FairwayHold, GreenHold, RoughHold));

        TestEqual(TEXT("Level ground leaves a resting ball at rest"), SpeedAfterSettling(Fairway, 0.0f), 0.0f);
        TestEqual(TEXT("Fairway holds a ball below its hold angle"), SpeedAfterSettling(Fairway, FairwayHold * 0.7f), 0.0f);

        const float Steep = FairwayHold + 5.0f;
        const float SteepSpeed = SpeedAfterSettling(Fairway, Steep);
        const float Unresisted = GolfPhysics::Gravity * FMath::Sin(FMath::DegreesToRadians(Steep)) * SettleSeconds;
        TestTrue(FString::Printf(TEXT("A steeper slope rolls the ball (%.3f m/s after %.0f s)"), SteepSpeed, SettleSeconds), SteepSpeed > 0.0f);
        TestTrue(FString::Printf(TEXT("Turf resists the roll (%.3f m/s, unresisted %.3f m/s)"), SteepSpeed, Unresisted), SteepSpeed < Unresisted);

        TestTrue(TEXT("Rough holds steeper slopes than green"), RoughHold > GreenHold);
        const float Moderate = (GreenHold + RoughHold) * 0.5f;
        TestEqual(TEXT("Rough holds a moderate slope"), SpeedAfterSettling(Rough, Moderate), 0.0f);
        TestTrue(TEXT("Green rolls on the same slope"), SpeedAfterSettling(Green, Moderate) > 0.002f);
    }
    return true;
}

#endif
