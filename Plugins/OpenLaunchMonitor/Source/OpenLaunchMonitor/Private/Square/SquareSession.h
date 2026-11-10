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
    void ConnectToDevice(const FGattDeviceInfo& Device, float TimeoutSeconds = IGattTransport::DefaultConnectTimeoutSeconds);
    /** Connects straight to the last connected device, without a scan; scans when there is none. */
    void Reconnect();
    bool HasLastDevice() const { return !LastDevice.Id.IsEmpty(); }
    /** Handshake waits and heartbeat watchdog for the next connection, so they can be tuned on hardware. */
    void SetTiming(float ClubSeconds, float ArmSeconds, float HeartbeatSeconds, int32 MissedBeforeLoss);
    /**
     * Reports Disconnected at once. A connected device is first told to stop detecting, so its red
     * lights go off immediately; the link is released when that write completes, within 0.5 s.
     */
    void Disconnect();
    /** For app exit: disarms and releases the device before returning (at most 1.5 s). */
    void Shutdown();
    /** Stops ball detection (DetectBall mode 0) and keeps the link. */
    void Disarm();
    void SetClub(const FString& ClubCode);
    void SetHandedness(int32 Handedness);
    void SetSpinMode(int32 SpinMode);

    ESquareSessionStatus GetStatus() const { return Status; }
    const FString& GetClubCode() const { return ClubCode; }

private:
    template <typename FunctorType>
    auto Guard(FunctorType&& Functor);

    /** Invalidates pending callbacks and clears all per-connection state; the transport is untouched. */
    void ResetSessionState();
    /** ResetSessionState, then stops any scan and releases the device. */
    void ResetConnection();
    /** Second half of Disconnect: releases the link once the disarm write finished or timed out. */
    void FinishDisconnect();
    void FailConnect(const FString& Error);
    void ContinueHandshake();
    void QueueCommand(TArray<uint8> Command, IGattTransport::FCompletion OnComplete = nullptr);
    void PumpWrites();
    void Arm();
    void HandleArmFailed(const FString& Error);
    void ScheduleHeartbeat();
    void HandleDeviceDiscovered(const FGattDeviceInfo& Device);
    void HandleNotification(const FString& CharacteristicUuid, const TArray<uint8>& Data);
    void HandleEventFrame(const TArray<uint8>& Data);
    void HandleClubFrame(const TArray<uint8>& Data, FSquareClubMetrics Club);
    void RequestClubMetrics(int32 ShotId, int32 Attempt);
    void HandleTransportDisconnected();
    /** Drops a connected link that stopped working and reports it lost, so the owner reconnects. */
    void LoseConnection(const TCHAR* Reason, const FString& Message);
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
    /** The device of the last connection attempt, for a reconnect without a scan. */
    FGattDeviceInfo LastDevice;
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
    /** Failed arm commands since the last successful one. */
    int32 ArmFailures = 0;
    /** Failed heartbeats in a row. */
    int32 HeartbeatFailures = 0;
    /** Heartbeat intervals in a row that passed without any notification. */
    int32 MissedHeartbeats = 0;
    bool bNotifiedSinceHeartbeat = false;
    bool bHeartbeatInFlight = false;
    /** Between Disconnect's disarm write and the link release. */
    bool bDisconnecting = false;
    /** The last shot frame was unreadable and filtered repeats are still pending a re-arm. */
    bool bLastFrameRejected = false;
    /** Last battery value logged this connection, so repeated notifications log once; -1 = none. */
    int32 LastLoggedBattery = -1;
};
