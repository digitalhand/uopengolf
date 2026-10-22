// SPDX-License-Identifier: MIT
#if PLATFORM_LINUX && WITH_DEV_AUTOMATION_TESTS
#include "HAL/PlatformTime.h"
#include "Linux/BlueZBus.h"
#include "Linux/SdBusApi.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueZBusTest, "UOpenGolf.LaunchMonitor.BlueZBus",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueZBusTest::RunTest(const FString&)
{
    FString Error;
    TestFalse(TEXT("Missing library is reported, not loaded"), FSdBusApi::Load(TEXT("libuopengolf-missing.so.0"), Error).IsValid());
    TestTrue(TEXT("Missing library error names libsystemd"), Error.Contains(TEXT("libsystemd")));

    const TUniquePtr<FBlueZBus> Bus = FBlueZBus::Open(Error);
    if (!Bus)
    {
        AddInfo(TEXT("No system bus: ") + Error);
        return true;
    }

    const double CancelStart = FPlatformTime::Seconds();
    const FBlueZReply Cancelled = Bus->GetManagedObjects({[] { return true; }, CancelStart + 10});
    TestEqual(TEXT("Cancelled call reports cancellation"), Cancelled.ErrorName, FString(BlueZModel::CancelledError));
    TestTrue(TEXT("Cancelled call returns without waiting"), FPlatformTime::Seconds() - CancelStart < 0.05);

    const FBlueZWait Wait{[] { return false; }, FPlatformTime::Seconds() + 10};
    const FBlueZReply Objects = Bus->GetManagedObjects(Wait);
    if (!TestTrue(TEXT("GetManagedObjects succeeds: ") + Objects.ErrorName + TEXT(" ") + Objects.ErrorMessage, Objects.Ok()))
    {
        return true;
    }
    // Cleanup calls are queued without waiting for BlueZ to answer.
    const double PostStart = FPlatformTime::Seconds();
    TestTrue(TEXT("Post queues a call"), Bus->Post(TEXT("/"), TEXT("org.freedesktop.DBus.Peer"), TEXT("Ping")));
    TestTrue(TEXT("Post returns without waiting"), FPlatformTime::Seconds() - PostStart < 0.05);
    Bus->Pump();

    const FString Adapter = BlueZModel::FindAdapterPath(Objects.Objects);
    if (Adapter.IsEmpty())
    {
        AddInfo(TEXT("No Bluetooth adapter"));
        return true;
    }

    const FBlueZReply Powered = Bus->GetProperty(Adapter, BlueZModel::AdapterInterface, TEXT("Powered"), Wait);
    TestTrue(TEXT("Adapter Powered property read"), Powered.Ok());
    TestTrue(TEXT("Adapter Powered is a bool"), Powered.Value.IsType<bool>());

    const FBlueZReply Missing = Bus->Call(Adapter, BlueZModel::AdapterInterface, TEXT("NoSuchMethod"), {}, Wait);
    TestFalse(TEXT("Unknown method reports a D-Bus error"), Missing.ErrorName.IsEmpty());
    return true;
}
#endif
