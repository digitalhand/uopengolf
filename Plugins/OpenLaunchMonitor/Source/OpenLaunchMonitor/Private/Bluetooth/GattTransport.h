// SPDX-License-Identifier: MIT
// Shape follows open-lm-connector (MIT) addons/common/bluetooth/IBluetoothGattClient.cs
// at 92eb021abe28d8eb1abe43f87767d3eb73e5939e, adapted to callbacks and a disconnect event.
#pragma once

#include "CoreMinimal.h"

struct FGattDeviceInfo
{
    /** Platform device identifier used to connect (a Bluetooth address or a device id). */
    FString Id;
    FString Name;
    int32 SignalStrengthDbm = 0;
};

/**
 * Minimal Bluetooth LE GATT client. Characteristic UUIDs are lowercase hyphenated
 * strings. Every callback, including the event handlers, runs on the game thread.
 */
class IGattTransport
{
public:
    using FCompletion = TFunction<void(bool bSuccess, const FString& Error)>;

    /** Whole-connection deadline for a first connect, which may wait for the device to wake. */
    static constexpr float DefaultConnectTimeoutSeconds = 30.0f;
    /**
     * Reads, writes and notification setup take milliseconds on a healthy link; longer means the
     * link is gone, or half-connected after a power cycle, and the attempt should fail fast.
     */
    static constexpr double ShortOperationTimeoutSeconds = 3.0;
    /** Opening the device and service discovery can be slow on a busy radio. */
    static constexpr double OperationTimeoutSeconds = 10.0;

    virtual ~IGattTransport() = default;

    TFunction<void(const FGattDeviceInfo& Device)> OnDeviceDiscovered;
    TFunction<void(const FString& CharacteristicUuid, const TArray<uint8>& Value)> OnNotification;
    /** The link to a connected device was lost without Disconnect being called. */
    TFunction<void()> OnDisconnected;
    /** Fatal scan/worker failure, delivered on the game thread. */
    TFunction<void(const FString&)> OnError;

    /** Reports advertising devices whose name starts with NamePrefix (case-insensitive). */
    virtual void StartScan(const FString& NamePrefix) = 0;
    virtual void StopScan() = 0;
    /**
     * Opens the device and resolves the characteristics; fails if a required one is missing or the
     * whole connection takes longer than TimeoutSeconds.
     */
    virtual void Connect(const FString& DeviceId, const TArray<FString>& RequiredCharacteristicUuids,
        const TArray<FString>& OptionalCharacteristicUuids, float TimeoutSeconds, FCompletion OnComplete) = 0;
    virtual void Disconnect() = 0;
    /** Completes with an empty array when the characteristic is unavailable, unreadable or slower than ShortOperationTimeoutSeconds. */
    virtual void Read(const FString& CharacteristicUuid, TFunction<void(const TArray<uint8>& Value)> OnComplete) = 0;
    /** Fails after ShortOperationTimeoutSeconds. */
    virtual void Subscribe(const FString& CharacteristicUuid, FCompletion OnComplete) = 0;
    /**
     * For app exit, when no game-thread callback will run again: writes FinalWrite (unless empty) to
     * Uuid, then releases the device, blocking the caller until both finish or MaxWaitSeconds pass.
     * Work already queued runs first.
     */
    virtual void WriteThenRelease(const FString& Uuid, const TArray<uint8>& FinalWrite, double MaxWaitSeconds) = 0;
    /** Writes with response, failing after ShortOperationTimeoutSeconds. */
    virtual void Write(const FString& CharacteristicUuid, const TArray<uint8>& Value, FCompletion OnComplete) = 0;
};

/** The platform's Bluetooth LE transport, or null where none is implemented yet. */
TSharedPtr<IGattTransport> CreatePlatformGattTransport();
