// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Bluetooth/GattOperationWait.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGattOperationWaitTest, "UOpenGolf.LaunchMonitor.OperationWait",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGattOperationWaitTest::RunTest(const FString&)
{
    using namespace GattOperationWait;
    double Now = 0;
    double FinishAt = 0.075;
    bool bStopped = false;
    int32 Generation = 1;
    const auto Sleep = [&](double Seconds)
    {
        TestTrue(TEXT("Wait quantum at most 25 ms"), Seconds <= 0.025);
        Now += Seconds;
    };
    auto WaitForOperation = [&](double Deadline)
    {
        return Wait([&] { return Now >= FinishAt; }, [&] { return bStopped || Generation != 1; },
            [&] { return Now; }, Sleep, Deadline);
    };
    TestTrue(TEXT("Successful asynchronous completion"), WaitForOperation(10) == EResult::Completed);
    TestTrue(TEXT("Completes promptly"), Now < 0.101);
    Now = 0; FinishAt = 100;
    TestTrue(TEXT("Unresponsive operation times out"), WaitForOperation(10) == EResult::TimedOut);
    TestEqual(TEXT("Timeout bounded"), Now, 10.0, 1e-6);
    Now = 0; Generation = 2;
    TestTrue(TEXT("Obsolete connection cancels immediately"), WaitForOperation(10) == EResult::Cancelled);
    TestEqual(TEXT("No sleep for cancelled job"), Now, 0.0);
    Generation = 1;
    TestTrue(TEXT("Shutdown interrupts a pending operation"), Wait([&] { return false; }, [&] { return bStopped; },
        [&] { return Now; }, [&](double Seconds) { Sleep(Seconds); bStopped = true; }, 10) == EResult::Cancelled);
    TestTrue(TEXT("Shutdown observed within quantum"), Now <= 0.025);
    bStopped = false; Now = 29.995;
    TestTrue(TEXT("Connection deadline truncates per-operation wait"), WaitForOperation(30) == EResult::TimedOut);
    TestEqual(TEXT("Overall deadline honored"), Now, 30.0, 1e-6);
    return true;
}
#endif
