// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/common/bluetooth/linux/BlueZMapper.cs at 655fd96
// (the Windows port pinned 92eb021, which is not in the local checkout).
#pragma once

#include "CoreMinimal.h"
#include "Bluetooth/GattTransport.h"
#include "Misc/TVariant.h"

/**
 * Platform-independent BlueZ D-Bus data mapping, kept free of sd-bus so it is tested on every OS.
 * D-Bus values are decoded to bool (b), int32 (y/n/q/i/u), FString (s/o) or bytes (ay).
 */
namespace BlueZModel
{
    using FValue = TVariant<bool, int32, FString, TArray<uint8>>;
    using FProperties = TMap<FString, FValue>;
    using FInterfaces = TMap<FString, FProperties>;
    using FManagedObjects = TMap<FString, FInterfaces>;

    inline const TCHAR* AdapterInterface = TEXT("org.bluez.Adapter1");
    inline const TCHAR* DeviceInterface = TEXT("org.bluez.Device1");
    inline const TCHAR* CharacteristicInterface = TEXT("org.bluez.GattCharacteristic1");
    inline const TCHAR* BatteryInterface = TEXT("org.bluez.Battery1");
    inline const TCHAR* BatteryLevelUuid = TEXT("00002a19-0000-1000-8000-00805f9b34fb");
    /** Local outcomes reported with the same shape as D-Bus errors. */
    inline const TCHAR* TimeoutError = TEXT("org.uopengolf.Error.Timeout");
    inline const TCHAR* CancelledError = TEXT("org.uopengolf.Error.Cancelled");

    FProperties CreateDiscoveryFilter(const FString& NamePrefix);
    FProperties CreateWriteOptions();

    bool IsDeviceNameMatch(const FString& Name, const FString& Prefix);
    bool TryCreateDevice(const FString& Path, const FInterfaces& Interfaces, const FString& Prefix, FGattDeviceInfo& Out);
    /** The first adapter's object path, or empty when BlueZ has none. */
    FString FindAdapterPath(const FManagedObjects& Objects);
    /** Accepts a device object path or a Bluetooth address in any common spelling. */
    bool TryFindDevicePath(const FManagedObjects& Objects, const FString& DeviceId, FString& OutPath);
    /** The characteristic's object path under DevicePath, or empty. */
    FString FindCharacteristicPath(const FManagedObjects& Objects, const FString& DevicePath, const FString& Uuid);
    bool HasBattery(const FManagedObjects& Objects, const FString& DevicePath);
    FString NormalizeAddress(const FString& Value);

    bool IsTransientConnectFailure(const FString& ErrorName, const FString& ErrorMessage);
    bool IsObjectGoneError(const FString& ErrorName);
    bool IsIgnorableStopDiscoveryError(const FString& ErrorName);
    bool IsIgnorableStopNotifyError(const FString& ErrorName);
    bool IsIgnorableDisconnectError(const FString& ErrorName);

    bool IsLinkLost(const FString& Interface, const FProperties& Changed);
    /** Non-empty characteristic values only, matching upstream. */
    TOptional<TArray<uint8>> NotificationValue(const FProperties& Changed);
    TOptional<uint8> BatteryPercentage(const FProperties& Changed);

    /** A short message for the HUD; unknown errors keep BlueZ's message. */
    FString FriendlyError(const FString& ErrorName, const FString& Message);
    /** FriendlyError for Device1.Connect and the services wait, where a timeout usually means another host holds the device. */
    FString FriendlyConnectError(const FString& ErrorName, const FString& Message);
    /** True for outcomes that happen in normal use (cancellation on D, reconnect or quit), logged without a warning. */
    bool IsExpectedOutcome(const FString& ErrorName);
    /**
     * True while a connect to Path must wait for an earlier Disconnect of PendingPath to take effect:
     * BlueZ keeps the link up for its 2 s disconnect timer and answers Connect on it as already done.
     */
    bool IsDisconnectPending(const FString& PendingPath, double PendingUntil, const FString& Path, double Now);
}
