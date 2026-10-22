// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayCanopyResponseTest, "UOpenGolf.Physics.CanopyResponse",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace CanopyResponseTestDetail
{
    float AngleDeg(const FVector3f& A, const FVector3f& B)
    {
        return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector3f::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()), -1.0f, 1.0f)));
    }
}

// Directions use the OpenFairway frame: X downrange, Y up, Z right.
bool FOpenFairwayCanopyResponseTest::RunTest(const FString& Parameters)
{
    using namespace CanopyResponseTestDetail;
    const FVector3f Drive(40.0f, 6.0f, 1.0f);

    TestTrue(TEXT("No path through the canopy leaves the ball unchanged"), AOpenFairwayBall::ResolveCanopyDrag(Drive, 0.0f, 7) == Drive);
    TestTrue(TEXT("A negative path is no path"), AOpenFairwayBall::ResolveCanopyDrag(Drive, -1.0f, 7) == Drive);
    TestTrue(TEXT("A non-finite path is no path"), AOpenFairwayBall::ResolveCanopyDrag(Drive, NAN, 7) == Drive);
    TestTrue(TEXT("A still ball stays still"), AOpenFairwayBall::ResolveCanopyDrag(FVector3f::ZeroVector, 1.0f, 7).IsNearlyZero());

    float Previous = Drive.Size();
    for (const float PathM : {0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f})
    {
        const FVector3f After = AOpenFairwayBall::ResolveCanopyDrag(Drive, PathM, 11);
        TestTrue(FString::Printf(TEXT("%.2f m of canopy slows the ball"), PathM), After.Size() < Previous);
        TestTrue(FString::Printf(TEXT("%.2f m of canopy keeps exp(-path / decay) of the speed"), PathM),
            FMath::IsNearlyEqual(After.Size(), Drive.Size() * AOpenFairwayBall::CanopySpeedFactor(PathM), 0.001f));
        const float LimitDeg = AOpenFairwayBall::CanopyDeflectionDegPerSqrtM * FMath::Sqrt(PathM);
        TestTrue(FString::Printf(TEXT("%.2f m of canopy turns the ball at most %.1f degrees"), PathM, LimitDeg), AngleDeg(Drive, After) <= LimitDeg + 0.01f);
        Previous = After.Size();
    }
    TestTrue(TEXT("The decay length keeps 1/e of the speed"),
        FMath::IsNearlyEqual(AOpenFairwayBall::CanopySpeedFactor(AOpenFairwayBall::CanopyDecayLengthM), FMath::Exp(-1.0f), 0.0001f));

    // Many short steps lose the same speed as one long one, so the frame rate does not change the result.
    FVector3f Stepped = Drive;
    for (int32 Step = 0; Step < 40; ++Step)
        Stepped = AOpenFairwayBall::ResolveCanopyDrag(Stepped, 0.1f, Step);
    TestTrue(TEXT("Speed loss does not depend on the step size"), FMath::IsNearlyEqual(Stepped.Size(), AOpenFairwayBall::ResolveCanopyDrag(Drive, 4.0f, 3).Size(), 0.001f));

    const FVector3f Once = AOpenFairwayBall::ResolveCanopyDrag(Drive, 1.0f, 1234);
    TestTrue(TEXT("The same seed gives the same deflection"), AOpenFairwayBall::ResolveCanopyDrag(Drive, 1.0f, 1234) == Once);
    bool bVaries = false;
    for (uint32 Seed = 1; Seed < 8 && !bVaries; ++Seed)
        bVaries = !AOpenFairwayBall::ResolveCanopyDrag(Drive, 1.0f, 1234 + Seed).Equals(Once, 0.001f);
    TestTrue(TEXT("Different seeds deflect differently"), bVaries);

    AActor* Owner = NewObject<AActor>(GetTransientPackage());
    UStaticMeshComponent* Crown = NewObject<UStaticMeshComponent>(Owner);
    TestFalse(TEXT("An untagged component is not canopy"), AOpenFairwayBall::IsCanopy(Crown));
    Owner->Tags.Add(AOpenFairwayBall::GetCanopyTag());
    TestTrue(TEXT("An actor tag marks canopy"), AOpenFairwayBall::IsCanopy(Crown));
    Owner->Tags.Reset();
    Crown->ComponentTags.Add(AOpenFairwayBall::GetCanopyTag());
    TestTrue(TEXT("A component tag marks canopy"), AOpenFairwayBall::IsCanopy(Crown));
    TestFalse(TEXT("Canopy is not an obstacle"), AOpenFairwayBall::IsObstacle(Crown));
    TestFalse(TEXT("No component is no canopy"), AOpenFairwayBall::IsCanopy(nullptr));
    return true;
}

#endif
