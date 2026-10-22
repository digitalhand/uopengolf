// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayBallVisualRollTest, "UOpenGolf.Physics.VisualRoll",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayBallVisualRollTest::RunTest(const FString& Parameters)
{
    constexpr float RadiusCm = 2.1335f;
    constexpr float NoLimit = 100.0f;
    const FVector Up = FVector::UpVector;
    const FVector Top(0.0, 0.0, RadiusCm);

    // Rolling one radius turns the ball one radian and carries its top forward.
    const FQuat Forward = AOpenFairwayBall::ComputeRollRotation(FVector(RadiusCm, 0.0, 0.0), Up, RadiusCm, NoLimit);
    TestEqual(TEXT("Angle is distance over radius"), Forward.GetAngle(), 1.0, 1e-4);
    const FVector ForwardTop = Forward.RotateVector(Top);
    TestEqual(TEXT("Top moves downrange"), ForwardTop.X, RadiusCm * FMath::Sin(1.0), 1e-3);
    TestEqual(TEXT("Top stays on the roll plane"), ForwardTop.Y, 0.0, 1e-4);

    const FVector RightTop = AOpenFairwayBall::ComputeRollRotation(FVector(0.0, 1.0, 0.0), Up, RadiusCm, NoLimit).RotateVector(Top);
    TestTrue(TEXT("Rolling right carries the top right"), RightTop.Y > 0.0 && FMath::Abs(RightTop.X) < 1e-4);

    TestEqual(TEXT("Fast rolls are capped per frame"),
        AOpenFairwayBall::ComputeRollRotation(FVector(100.0 * RadiusCm, 0.0, 0.0), Up, RadiusCm, 0.5f).GetAngle(), 0.5, 1e-4);
    TestTrue(TEXT("No motion, no rotation"), AOpenFairwayBall::ComputeRollRotation(FVector::ZeroVector, Up, RadiusCm, NoLimit).Equals(FQuat::Identity));
    TestTrue(TEXT("Zero radius, no rotation"), AOpenFairwayBall::ComputeRollRotation(FVector(1.0, 0.0, 0.0), Up, 0.0f, NoLimit).Equals(FQuat::Identity));

    // On a slope only motion along the ground turns the ball.
    const FVector Slope = FVector(-0.3, 0.0, 1.0).GetSafeNormal();
    TestTrue(TEXT("Motion along the slope normal does not roll"),
        AOpenFairwayBall::ComputeRollRotation(Slope * 5.0, Slope, RadiusCm, NoLimit).Equals(FQuat::Identity));
    const FVector Along = FVector::CrossProduct(FVector::RightVector, Slope).GetSafeNormal() * RadiusCm;
    const FQuat Downhill = AOpenFairwayBall::ComputeRollRotation(Along + Slope * 3.0, Slope, RadiusCm, NoLimit);
    TestEqual(TEXT("Normal motion is ignored on a slope"), Downhill.GetAngle(), 1.0, 1e-4);
    return true;
}

#endif
