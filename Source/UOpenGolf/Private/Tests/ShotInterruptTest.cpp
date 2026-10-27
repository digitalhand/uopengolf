// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "Tests/AutomationEditorCommon.h"

namespace ShotInterruptTestDetail
{
    FOpenFairwayLaunch IronShot()
    {
        FOpenFairwayLaunch Launch;
        Launch.BallSpeedMph = 100.0f;
        Launch.VerticalLaunchDeg = 20.0f;
        Launch.bHasBackSpin = true;
        Launch.BackSpinRpm = 6000.0f;
        return Launch;
    }
} // namespace ShotInterruptTestDetail

// A shot that never comes to rest, such as a ball trapped between contacts, still ends, and a
// ball re-teed mid-shot leaves a finished result instead of one stuck "in flight".
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotInterruptTest, "UOpenGolf.Physics.ShotInterrupt",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShotInterruptTest::RunTest(const FString& Parameters)
{
    using namespace ShotInterruptTestDetail;
    // A blank map has no ground, so only the watchdog or the fall-through limit can end a shot.
    UWorld* World = FAutomationEditorCommonUtils::CreateNewMap();
    if (!World)
    {
        AddError(TEXT("Could not create a blank map."));
        return false;
    }
    auto* Ball = World->SpawnActor<AOpenFairwayBall>();
    if (!TestNotNull(TEXT("Ball"), Ball))
    {
        return false;
    }
    constexpr float FrameSeconds = 1.0f / 60.0f;

    Ball->MaxShotSeconds = 0.5f;
    TestTrue(TEXT("Watchdog shot launches"), Ball->Launch(IronShot()));
    for (int32 Frame = 0; Ball->IsMoving() && Frame < 600; ++Frame)
    {
        Ball->Tick(FrameSeconds);
    }
    FOpenFairwayShotResult Result;
    TestTrue(TEXT("Watchdog shot has a result"), Ball->GetShotResult(Result));
    TestTrue(TEXT("Watchdog ends motion"), Result.bAtRest && !Ball->IsMoving());
    TestEqual(TEXT("Watchdog stop is a safety stop"), Result.TerminationReason, EOpenFairwayShotTermination::SafetyStop);
    TestTrue(TEXT("Watchdog stops at its limit, before the fall-through limit"), Result.ElapsedS <= 0.5f + FrameSeconds);
    TestEqual(TEXT("Watchdog does not fabricate a landing"), Result.CarryM, 0.0f);

    Ball->MaxShotSeconds = 30.0f;
    Ball->PlaceAt(FVector::ZeroVector);
    TestTrue(TEXT("Second shot launches"), Ball->Launch(IronShot()));
    for (int32 Frame = 0; Frame < 30; ++Frame)
    {
        Ball->Tick(FrameSeconds);
    }
    TestTrue(TEXT("Second shot still in flight"), Ball->IsMoving());
    Ball->GetShotResult(Result);
    const int32 InterruptedShot = Result.ShotNumber;
    Ball->PlaceAt(FVector::ZeroVector);
    TestTrue(TEXT("Re-teed shot has a result"), Ball->GetShotResult(Result));
    TestEqual(TEXT("Result belongs to the interrupted shot"), Result.ShotNumber, InterruptedShot);
    TestTrue(TEXT("Re-teed shot is finished"), Result.bAtRest && !Ball->IsMoving());
    TestEqual(TEXT("Re-teed shot is marked interrupted"), Result.TerminationReason, EOpenFairwayShotTermination::SafetyStop);
    TestTrue(TEXT("Interrupted result keeps its progress"), Result.TotalM > 0.0f);

    Ball->Destroy();
    return true;
}
#endif
