// SPDX-License-Identifier: MIT
#if PLATFORM_LINUX && WITH_DEV_AUTOMATION_TESTS
#include "Async/TaskGraphInterfaces.h"
#include "Bluetooth/GattTransport.h"
#include "HAL/PlatformProcess.h"
#include "Linux/BlueZBus.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTransportTeardownTest, "UOpenGolf.LaunchMonitor.TransportTeardown",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/**
 * Destroys the platform transport while jobs are queued or running, as quitting during a
 * connection attempt does. The worker must finish without asserting, and nothing it posted
 * may reach the callbacks of a destroyed transport. The race is timing-dependent, so each
 * round stops at a different point; a passing run is evidence, not proof, of a safe teardown.
 */
bool FTransportTeardownTest::RunTest(const FString&)
{
    FString Error;
    if (!FBlueZBus::Open(Error))
    {
        AddInfo(TEXT("No system bus: ") + Error);
        return true;
    }

    constexpr int32 Rounds = 25;
    const TSharedRef<int32> Callbacks = MakeShared<int32>(0);
    for (int32 Round = 0; Round < Rounds; ++Round)
    {
        TSharedPtr<IGattTransport> Transport = CreatePlatformGattTransport();
        if (!TestNotNull(TEXT("Platform transport"), Transport.Get()))
        {
            return false;
        }
        Transport->OnError = [Callbacks](const FString&) { ++*Callbacks; };
        Transport->OnDisconnected = [Callbacks]() { ++*Callbacks; };
        // An address BlueZ does not know: the connection job fails without touching a device.
        Transport->Connect(TEXT("00:00:00:00:00:00"), {TEXT("00000000-0000-0000-0000-000000000000")}, {},
            [Callbacks](bool, const FString&) { ++*Callbacks; });
        Transport->Read(TEXT("00000000-0000-0000-0000-000000000001"), [Callbacks](const TArray<uint8>&) { ++*Callbacks; });
        FPlatformProcess::Sleep(0.0005f * static_cast<float>(Round % 10));
        Transport.Reset();
    }

    FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
    TestEqual(TEXT("No callback runs after its transport is destroyed"), *Callbacks, 0);
    return true;
}
#endif
