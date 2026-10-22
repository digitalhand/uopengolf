// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/square/SquareConnectionSession.cs
// at 92eb021abe28d8eb1abe43f87767d3eb73e5939e, rewritten as a game-thread state machine.
#pragma once

#include "CoreMinimal.h"
#include "Bluetooth/GattTransport.h"
#include "Square/SquareProtocol.h"

class ISquareScheduler;

enum class ESquareSessionStatus : uint8
{
    Disconnected,
    Scanning,
    Connecting,
    Connected,
    Ready
};

/**
 * Square launch-monitor connection: scan, handshake, heartbeat, arming, shot and
 * club frames. Runs entirely on the game thread; transport and scheduler callbacks
 * that belong to an earlier connection are ignored.
 */
class FSquareSession : public TSharedFromThis<FSquareSession>
{
public:
    FSquareSession(TSharedRef<IGattTransport> InTransport, TSharedRef<ISquareScheduler> InScheduler,
        FSquareConnectionOptions InOptions = FSquareConnectionOptions());

    /** Binds transport events. Call once after construction (needs a shared reference). */
    void Initialize();

    TFunction<void(ESquareSessionStatus Status)> OnStatusChanged;
    TFunction<void(const FString& Message)> OnError;
    TFunction<void(const FGattDeviceInfo& Device)> OnDeviceSelected;
    TFunction<void(int32 Percent)> OnBatteryChanged;
    TFunction<void(const FString& Firmware)> OnFirmwareChanged;
    TFunction<void(bool bReady)> OnReadyChanged;
    /** ShotId increases per accepted shot in this session and ties club data to its shot. */
    TFunction<void(const FSquareShotMetrics& Metrics, int32 ShotId)> OnShot;
    TFunction<void(const FSquareClubMetrics& Metrics, int32 ShotId)> OnClub;
    /** The link dropped while connected (not after Disconnect). */
    TFunction<void()> OnConnectionLost;

    /** Scans for the first device matching the name prefix and connects to it. */
    void Connect();
    void ConnectToDevice(const FGattDeviceInfo& Device);
    void Disconnect();
    void SetClub(const FString& ClubCode);
    void SetHandedness(int32 Handedness);
    void SetSpinMode(int32 SpinMode);

    ESquareSessionStatus GetStatus() const { return Status; }
    const FString& GetClubCode() const { return ClubCode; }

private:
    template <typename FunctorType>
    auto Guard(FunctorType&& Functor);

    void ResetConnection();
    void FailConnect(const FString& Error);
    void ContinueHandshake();
    void QueueCommand(TArray<uint8> Command, IGattTransport::FCompletion OnComplete = nullptr);
    void PumpWrites();
    void Arm();
    void ScheduleHeartbeat();
    void HandleDeviceDiscovered(const FGattDeviceInfo& Device);
    void HandleNotification(const FString& CharacteristicUuid, const TArray<uint8>& Data);
    void HandleEventFrame(const TArray<uint8>& Data);
    void HandleClubFrame(const TArray<uint8>& Data, FSquareClubMetrics Club);
    void RequestClubMetrics(int32 ShotId, int32 Attempt);
    void HandleTransportDisconnected();
    void ReportBattery(int32 Percent);
    void SetStatus(ESquareSessionStatus NewStatus);
    void EmitError(const FString& Message);
    void EmitReady(bool bReady);
    uint8 NextSequence() { return Sequence++; }

    struct FPendingWrite
    {
        TArray<uint8> Command;
        IGattTransport::FCompletion OnComplete;
    };

    TSharedRef<IGattTransport> Transport;
    TSharedRef<ISquareScheduler> Scheduler;
    FSquareConnectionOptions Options;
    ESquareSessionStatus Status = ESquareSessionStatus::Disconnected;
    /** Incremented on every connect/disconnect; stale callbacks compare against it. */
    uint32 Generation = 0;
    TArray<FPendingWrite> WriteQueue;
    bool bWriteInFlight = false;
    uint8 Sequence = 0;
    FString LastPayload;
    int32 ShotCount = 0;
    /** The shot whose club frame is still expected; 0 = none. */
    int32 AwaitingClubForShot = 0;
    FString ClubCode = SquareProtocol::DriverClubCode;
    int32 Handedness = 0;
    int32 SpinMode = 1;
    bool bConnected = false;
    bool bArmed = false;
    bool bShotPendingRearm = false;
    /** Last battery value logged this connection, so repeated notifications log once; -1 = none. */
    int32 LastLoggedBattery = -1;
};
