// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayObstacleResponseTest, "UOpenGolf.Physics.ObstacleResponse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// Directions use the OpenFairway frame: X downrange, Y up, Z right.
bool FOpenFairwayObstacleResponseTest::RunTest(const FString& Parameters)
{
    constexpr float Restitution = AOpenFairwayBall::ObstacleRestitution;

    // A trunk face toward the tee sends a line drive back, damped, still falling.
    const FVector3f Drive(40.0f, -5.0f, 0.0f);
    const FVector3f Back = AOpenFairwayBall::ResolveObstacleHit(Drive, FVector3f(-1.0f, 0.0f, 0.0f), FVector3f(-1.0f, 0.0f, 0.0f));
    TestTrue(TEXT("A face-on hit rebounds toward the tee"), Back.Equals(FVector3f(-40.0f * Restitution, -5.0f * Restitution, 0.0f), 0.0001f));

    // A glancing hit reflects to the open side of the trunk.
    const FVector3f Normal = FVector3f(-1.0f, 0.0f, 1.0f).GetSafeNormal();
    const FVector3f Glance = AOpenFairwayBall::ResolveObstacleHit(FVector3f(30.0f, 0.0f, 0.0f), Normal, Normal);
    TestTrue(TEXT("A glancing hit deflects to the open side"), Glance.Z > 0.0f && FVector3f::DotProduct(Glance, Normal) > 0.0f);

    // A ball falling straight onto a collider's rim is pushed off it, hit after hit, and keeps falling.
    const FVector3f Edge = FVector3f(-0.3f, 0.95f, 0.0f).GetSafeNormal();
    FVector3f OnRim(0.0f, -10.0f, 0.0f);
    for (int32 Hit = 0; Hit < 20; ++Hit)
    {
        OnRim = AOpenFairwayBall::ResolveObstacleHit(OnRim, Edge, FVector3f(-1.0f, 0.0f, 0.0f));
        if (!TestTrue(FString::Printf(TEXT("Rim hit %d pushes the ball off"), Hit), OnRim.X <= -AOpenFairwayBall::ObstacleEscapeMps + 0.0001f))
            break;
        TestTrue(FString::Printf(TEXT("Rim hit %d is not a landing"), Hit), OnRim.Y <= 0.0f);
        OnRim.Y -= 0.1f;
    }
    const FVector3f Side = AOpenFairwayBall::ResolveObstacleHit(FVector3f(0.5f, -2.0f, 0.0f), FVector3f(-1.0f, 0.0f, 0.0f), FVector3f(-1.0f, 0.0f, 0.0f));
    TestTrue(TEXT("A slow hit on an upright face is not given extra speed"), FMath::IsNearlyEqual(Side.X, -0.5f * AOpenFairwayBall::ObstacleRestitution, 0.0001f));

    // Straight down onto a flat top: the ball leaves along its offset from the axis and never settles.
    const FVector3f Away(0.0f, 0.0f, 0.1f);
    FVector3f Velocity(0.0f, -12.0f, 0.0f);
    for (int32 Hit = 0; Hit < 20; ++Hit)
    {
        Velocity = AOpenFairwayBall::ResolveObstacleHit(Velocity, FVector3f(0.0f, 1.0f, 0.0f), Away);
        if (!TestTrue(FString::Printf(TEXT("Top hit %d leaves outward"), Hit), Velocity.Z >= AOpenFairwayBall::ObstacleEscapeMps - 0.0001f))
            break;
        TestTrue(FString::Printf(TEXT("Top hit %d does not bounce upward"), Hit), Velocity.Y <= 0.0f);
        Velocity.Y -= 0.1f;
    }

    const FVector3f Centred = AOpenFairwayBall::ResolveObstacleHit(FVector3f(5.0f, -3.0f, 0.0f), FVector3f(0.0f, 1.0f, 0.0f), FVector3f::ZeroVector);
    TestTrue(TEXT("A ball centred on the axis leaves backward along its path"), Centred.X <= -AOpenFairwayBall::ObstacleEscapeMps + 0.0001f);

    AActor* Owner = NewObject<AActor>(GetTransientPackage());
    UStaticMeshComponent* Trunk = NewObject<UStaticMeshComponent>(Owner);
    TestFalse(TEXT("An untagged component is not an obstacle"), AOpenFairwayBall::IsObstacle(Trunk));
    Owner->Tags.Add(AOpenFairwayBall::GetObstacleTag());
    TestTrue(TEXT("An actor tag marks an obstacle"), AOpenFairwayBall::IsObstacle(Trunk));
    Owner->Tags.Reset();
    Trunk->ComponentTags.Add(AOpenFairwayBall::GetObstacleTag());
    TestTrue(TEXT("A component tag marks an obstacle"), AOpenFairwayBall::IsObstacle(Trunk));
    TestFalse(TEXT("No component is no obstacle"), AOpenFairwayBall::IsObstacle(nullptr));
    return true;
}

#endif
