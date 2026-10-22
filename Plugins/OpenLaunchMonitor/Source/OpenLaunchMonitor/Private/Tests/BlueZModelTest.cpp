// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Bluetooth/BlueZModel.h"
#include "Misc/AutomationTest.h"

namespace BlueZModelTestDetail
{
    using namespace BlueZModel;

    template <typename T>
    FValue Make(T Value)
    {
        return FValue(TInPlaceType<T>(), MoveTemp(Value));
    }

    const FString FirstDevice = TEXT("/org/bluez/hci0/dev_AA_BB_CC_DD_EE_FF");
    const FString SecondDevice = TEXT("/org/bluez/hci0/dev_11_22_33_44_55_66");
    const FString EventUuid = TEXT("86602102-6b7e-439a-bdd1-489a3213e9bb");

    FManagedObjects MakeObjects()
    {
        FManagedObjects Objects;
        Objects.Add(TEXT("/org/bluez/hci0")).Add(AdapterInterface).Add(TEXT("Powered"), Make(true));

        FInterfaces& First = Objects.Add(FirstDevice);
        FProperties& FirstProperties = First.Add(DeviceInterface);
        FirstProperties.Add(TEXT("Name"), Make(FString(TEXT(" SquareGolf ABC "))));
        FirstProperties.Add(TEXT("Address"), Make(FString(TEXT("AA:BB:CC:DD:EE:FF"))));
        FirstProperties.Add(TEXT("RSSI"), Make(int32(-61)));
        First.Add(BatteryInterface).Add(TEXT("Percentage"), Make(int32(77)));
        Objects.Add(FirstDevice + TEXT("/service0010/char0011"))
            .Add(CharacteristicInterface)
            .Add(TEXT("UUID"), Make(FString(TEXT("86602102-6B7E-439A-BDD1-489A3213E9BB"))));

        Objects.Add(SecondDevice).Add(DeviceInterface).Add(TEXT("Address"), Make(FString(TEXT("11:22:33:44:55:66"))));
        Objects.Add(SecondDevice + TEXT("/service0010/char0011"))
            .Add(CharacteristicInterface)
            .Add(TEXT("UUID"), Make(EventUuid));
        return Objects;
    }

    const FString* FindString(const FProperties& Properties, const TCHAR* Name)
    {
        const FValue* Value = Properties.Find(Name);
        return Value ? Value->TryGet<FString>() : nullptr;
    }
} // namespace BlueZModelTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlueZModelTest, "UOpenGolf.LaunchMonitor.BlueZModel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlueZModelTest::RunTest(const FString&)
{
    using namespace BlueZModel;
    using namespace BlueZModelTestDetail;
    const FProperties Filter = CreateDiscoveryFilter(TEXT("SquareGolf"));
    const FString* Transport = FindString(Filter, TEXT("Transport"));
    const FString* Pattern = FindString(Filter, TEXT("Pattern"));
    const bool* Duplicates = Filter.Contains(TEXT("DuplicateData")) ? Filter[TEXT("DuplicateData")].TryGet<bool>() : nullptr;
    TestTrue(TEXT("Filter transport is LE"), Transport && *Transport == TEXT("le"));
    TestTrue(TEXT("Filter pattern is the name prefix"), Pattern && *Pattern == TEXT("SquareGolf"));
    TestTrue(TEXT("Filter drops duplicate data"), Duplicates && !*Duplicates);
    const FProperties WriteOptions = CreateWriteOptions();
    const FString* WriteType = FindString(WriteOptions, TEXT("type"));
    TestTrue(TEXT("Writes request a response"), WriteType && *WriteType == TEXT("request"));

    TestTrue(TEXT("Name match trims and ignores case"), IsDeviceNameMatch(TEXT(" squaregolf x "), TEXT("SquareGolf")));
    TestFalse(TEXT("Empty name never matches"), IsDeviceNameMatch(TEXT(""), TEXT("SquareGolf")));
    TestFalse(TEXT("Other names do not match"), IsDeviceNameMatch(TEXT("Other"), TEXT("SquareGolf")));

    const FManagedObjects Objects = MakeObjects();
    FGattDeviceInfo Device;
    TestTrue(TEXT("Matching device is reported"), TryCreateDevice(FirstDevice, Objects[FirstDevice], TEXT("SquareGolf"), Device));
    TestEqual(TEXT("Device id is the object path"), Device.Id, FirstDevice);
    TestEqual(TEXT("Device name is trimmed"), Device.Name, FString(TEXT("SquareGolf ABC")));
    TestEqual(TEXT("RSSI is carried"), Device.SignalStrengthDbm, -61);
    FInterfaces AliasOnly;
    AliasOnly.Add(DeviceInterface).Add(TEXT("Alias"), Make(FString(TEXT("SquareGolf Alias"))));
    TestTrue(TEXT("Alias is used without a name"), TryCreateDevice(SecondDevice, AliasOnly, TEXT("SquareGolf"), Device));
    TestEqual(TEXT("Alias becomes the name"), Device.Name, FString(TEXT("SquareGolf Alias")));
    TestFalse(TEXT("Objects without Device1 are not devices"),
        TryCreateDevice(TEXT("/org/bluez/hci0"), Objects[TEXT("/org/bluez/hci0")], TEXT(""), Device));

    TestEqual(TEXT("Adapter path found"), FindAdapterPath(Objects), FString(TEXT("/org/bluez/hci0")));
    TestEqual(TEXT("No adapter gives empty"), FindAdapterPath(FManagedObjects()), FString());

    for (const TCHAR* Id : {*FirstDevice, TEXT("aa:bb:cc:dd:ee:ff"), TEXT("AABBCCDDEEFF"), TEXT("AA-BB-CC-DD-EE-FF")})
    {
        FString Path;
        TestTrue(FString::Printf(TEXT("Device found from %s"), Id), TryFindDevicePath(Objects, Id, Path));
        TestEqual(FString::Printf(TEXT("Device path from %s"), Id), Path, FirstDevice);
    }
    FString Unknown;
    TestFalse(TEXT("Unknown device path not found"), TryFindDevicePath(Objects, TEXT("/org/bluez/hci0/dev_00_00_00_00_00_00"), Unknown));

    TestEqual(TEXT("Characteristic resolved under the selected device only"),
        FindCharacteristicPath(Objects, FirstDevice, EventUuid), FirstDevice + TEXT("/service0010/char0011"));
    TestEqual(TEXT("Missing characteristic gives empty"),
        FindCharacteristicPath(Objects, FirstDevice, TEXT("86602101-6b7e-439a-bdd1-489a3213e9bb")), FString());
    TestTrue(TEXT("Battery1 detected"), HasBattery(Objects, FirstDevice));
    TestFalse(TEXT("No Battery1 on second device"), HasBattery(Objects, SecondDevice));

    TestTrue(TEXT("InProgress is transient"), IsTransientConnectFailure(TEXT("org.bluez.Error.InProgress"), TEXT("")));
    TestTrue(TEXT("Local LE abort is transient"),
        IsTransientConnectFailure(TEXT("org.bluez.Error.Failed"), TEXT("le-connection-abort-by-local")));
    TestFalse(TEXT("Other failures are not transient"), IsTransientConnectFailure(TEXT("org.bluez.Error.Failed"), TEXT("other")));
    TestTrue(TEXT("Unknown object is gone"), IsObjectGoneError(TEXT("org.freedesktop.DBus.Error.UnknownObject")));
    TestTrue(TEXT("Unknown method is gone"), IsObjectGoneError(TEXT("org.freedesktop.DBus.Error.UnknownMethod")));

    const TCHAR* InProgress = TEXT("org.bluez.Error.InProgress");
    for (const TCHAR* Name : {TEXT("org.bluez.Error.NotReady"), TEXT("org.bluez.Error.Failed"), TEXT("org.bluez.Error.NotAuthorized")})
    {
        TestTrue(FString::Printf(TEXT("StopDiscovery ignores %s"), Name), IsIgnorableStopDiscoveryError(Name));
    }
    TestFalse(TEXT("StopDiscovery keeps other errors"), IsIgnorableStopDiscoveryError(InProgress));
    for (const TCHAR* Name : {TEXT("org.bluez.Error.Failed"), TEXT("org.bluez.Error.NotPermitted"), TEXT("org.bluez.Error.NotSupported"),
             TEXT("org.freedesktop.DBus.Error.UnknownObject")})
    {
        TestTrue(FString::Printf(TEXT("StopNotify ignores %s"), Name), IsIgnorableStopNotifyError(Name));
    }
    TestFalse(TEXT("StopNotify keeps other errors"), IsIgnorableStopNotifyError(InProgress));
    for (const TCHAR* Name : {TEXT("org.bluez.Error.NotConnected"), TEXT("org.bluez.Error.Failed"), TEXT("org.freedesktop.DBus.Error.UnknownMethod")})
    {
        TestTrue(FString::Printf(TEXT("Disconnect ignores %s"), Name), IsIgnorableDisconnectError(Name));
    }
    TestFalse(TEXT("Disconnect keeps other errors"), IsIgnorableDisconnectError(InProgress));

    FProperties Disconnected, Connected;
    Disconnected.Add(TEXT("Connected"), Make(false));
    Connected.Add(TEXT("Connected"), Make(true));
    TestTrue(TEXT("Connected=false is link loss"), IsLinkLost(DeviceInterface, Disconnected));
    TestFalse(TEXT("Connected=true is not link loss"), IsLinkLost(DeviceInterface, Connected));
    TestFalse(TEXT("Other interfaces are not link loss"), IsLinkLost(AdapterInterface, Disconnected));

    FProperties EmptyValue, Value;
    EmptyValue.Add(TEXT("Value"), Make(TArray<uint8>()));
    Value.Add(TEXT("Value"), Make(TArray<uint8>({1, 2})));
    TestFalse(TEXT("Empty notifications are ignored"), NotificationValue(EmptyValue).IsSet());
    const TOptional<TArray<uint8>> Notified = NotificationValue(Value);
    TestTrue(TEXT("Notification bytes are carried"), Notified.IsSet() && Notified.GetValue() == TArray<uint8>({1, 2}));
    FProperties Percentage;
    Percentage.Add(TEXT("Percentage"), Make(int32(64)));
    const TOptional<uint8> Battery = BatteryPercentage(Percentage);
    TestTrue(TEXT("Battery percentage read"), Battery.IsSet() && Battery.GetValue() == 64);

    TestTrue(TEXT("NotReady explained"),
        FriendlyError(TEXT("org.bluez.Error.NotReady"), TEXT("Resource Not Ready")).Contains(TEXT("Bluetooth is turned off or blocked")));
    TestTrue(TEXT("Timeout explained"), FriendlyError(TimeoutError, TEXT("")).Contains(TEXT("timed out")));
    TestEqual(TEXT("Evicted device explained"), FriendlyError(TEXT("org.freedesktop.DBus.Error.UnknownObject"), TEXT("x")),
        FString(TEXT("The selected Bluetooth device is no longer known to BlueZ. Scan again before connecting.")));
    TestEqual(TEXT("Unknown error keeps its message"), FriendlyError(TEXT("org.example.Error"), TEXT("Plain message")),
        FString(TEXT("Plain message")));
    TestEqual(TEXT("Unknown error without message gives its name"), FriendlyError(TEXT("org.example.Error"), TEXT("")),
        FString(TEXT("org.example.Error")));

    const FString ServiceDown = TEXT("The Bluetooth service isn't running. Start it with: sudo systemctl start bluetooth");
    TestEqual(TEXT("Missing bluetoothd explained"), FriendlyError(TEXT("org.freedesktop.DBus.Error.ServiceUnknown"), TEXT("x")), ServiceDown);
    TestEqual(TEXT("Unowned org.bluez explained"), FriendlyError(TEXT("org.freedesktop.DBus.Error.NameHasNoOwner"), TEXT("x")), ServiceDown);
    // Ubuntu starts bluetoothd on demand; a disabled or masked unit fails activation instead.
    const FString ServiceDisabled = TEXT("The Bluetooth service is disabled. Enable it with: sudo systemctl enable --now bluetooth");
    TestEqual(TEXT("Missing unit explained"), FriendlyError(TEXT("org.freedesktop.systemd1.NoSuchUnit"), TEXT("Unit dbus-org.bluez.service not found.")),
        ServiceDisabled);
    TestEqual(TEXT("Masked unit explained"), FriendlyError(TEXT("org.freedesktop.systemd1.UnitMasked"), TEXT("x")), ServiceDisabled);
    TestEqual(TEXT("Connect timeout hints at another host"), FriendlyConnectError(TimeoutError, TEXT("")),
        FString(TEXT("The Square didn't answer. If another computer or app is connected to it, disconnect it there and press C again.")));
    TestEqual(TEXT("Other connect errors match FriendlyError"), FriendlyConnectError(TEXT("org.bluez.Error.NotReady"), TEXT("")),
        FriendlyError(TEXT("org.bluez.Error.NotReady"), TEXT("")));
    TestTrue(TEXT("Cancellation is an expected outcome"), IsExpectedOutcome(CancelledError));
    TestFalse(TEXT("Timeout is not an expected outcome"), IsExpectedOutcome(TimeoutError));
    TestFalse(TEXT("Failures are not expected outcomes"), IsExpectedOutcome(TEXT("org.bluez.Error.Failed")));

    // BlueZ drops a disconnected link only after its 2 s disconnect timer, so a reconnect waits.
    TestTrue(TEXT("Reconnect to the released device waits"), IsDisconnectPending(FirstDevice, 10.0, FirstDevice, 9.0));
    TestFalse(TEXT("Wait ends after the settle window"), IsDisconnectPending(FirstDevice, 10.0, FirstDevice, 10.0));
    TestFalse(TEXT("Other devices do not wait"), IsDisconnectPending(FirstDevice, 10.0, SecondDevice, 9.0));
    TestFalse(TEXT("Nothing released means no wait"), IsDisconnectPending(FString(), 10.0, FirstDevice, 9.0));
    return true;
}
#endif
