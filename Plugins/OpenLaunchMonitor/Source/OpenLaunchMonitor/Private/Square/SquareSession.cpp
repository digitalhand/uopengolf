// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/square/SquareConnectionSession.cs
// at 92eb021abe28d8eb1abe43f87767d3eb73e5939e.
#include "Square/SquareSession.h"

#include "OpenLaunchMonitorLog.h"
#include "Square/SquareScheduler.h"

namespace
{
    const TCHAR* DeviceNotReadyMessage =
        TEXT("Square device was detected but is not ready yet. Wait a moment and try connecting again.");
    constexpr int32 ClubRequestAttempts = 3;
    constexpr float ClubRequestRetryDelaySeconds = 0.15f;
    /** Arm commands tried, a ready delay apart, before the link is dropped and reconnected. */
    constexpr int32 ArmAttempts = 3;
    /** Heartbeats that may fail in a row before the device is treated as gone. */
    constexpr int32 HeartbeatFailuresBeforeLinkLoss = 3;
} // namespace

FSquareSession::FSquareSession(
    TSharedRef<IGattTransport> InTransport, TSharedRef<ISquareScheduler> InScheduler, FSquareConnectionOptions InOptions)
    : Transport(MoveTemp(InTransport)), Scheduler(MoveTemp(InScheduler)), Options(MoveTemp(InOptions))
{
}

void FSquareSession::Initialize()
{
    const TWeakPtr<FSquareSession> WeakSelf = AsWeak();
    Transport->OnDeviceDiscovered = [WeakSelf](const FGattDeviceInfo& Device)
    {
        if (const TSharedPtr<FSquareSession> Self = WeakSelf.Pin())
        {
            Self->HandleDeviceDiscovered(Device);
        }
    };
    Transport->OnNotification = [WeakSelf](const FString& CharacteristicUuid, const TArray<uint8>& Value)
    {
        if (const TSharedPtr<FSquareSession> Self = WeakSelf.Pin())
        {
            Self->HandleNotification(CharacteristicUuid, Value);
        }
    };
    Transport->OnError = [WeakSelf](const FString& Error)
    {
        if (const TSharedPtr<FSquareSession> Self = WeakSelf.Pin()) Self->FailConnect(Error);
    };
    Transport->OnDisconnected = [WeakSelf]()
    {
        if (const TSharedPtr<FSquareSession> Self = WeakSelf.Pin())
        {
            Self->HandleTransportDisconnected();
        }
    };
}

template <typename FunctorType>
auto FSquareSession::Guard(FunctorType&& Functor)
{
    return [WeakSelf = AsWeak(), ExpectedGeneration = Generation, Functor = Forward<FunctorType>(Functor)](auto&&... Args) mutable
    {
        const TSharedPtr<FSquareSession> Self = WeakSelf.Pin();
        if (Self && Self->Generation == ExpectedGeneration)
        {
            Functor(Forward<decltype(Args)>(Args)...);
        }
    };
}

void FSquareSession::Connect()
{
    ResetConnection();
    SetStatus(ESquareSessionStatus::Scanning);
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Scanning for devices named %s*."), *Options.DeviceNamePrefix);
    Transport->StartScan(Options.DeviceNamePrefix);
    Scheduler->Schedule(Options.ScanTimeoutSeconds, Guard([this]()
                                                        {
        if (Status == ESquareSessionStatus::Scanning)
        {
            Transport->StopScan();
            EmitError(TEXT("No Square device found. Check that it is powered on, nearby, and not connected to another app."));
            SetStatus(ESquareSessionStatus::Disconnected);
        } }));
}

void FSquareSession::ConnectToDevice(const FGattDeviceInfo& Device)
{
    if (Device.Id.IsEmpty())
    {
        EmitError(TEXT("No Square device was selected."));
        return;
    }

    ResetConnection();
    SetStatus(ESquareSessionStatus::Connecting);
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Connecting to %s (%s)."), *Device.Name, *Device.Id);
    if (OnDeviceSelected)
    {
        OnDeviceSelected(Device);
    }
    Transport->Connect(Device.Id, {Options.CommandCharacteristicUuid, Options.EventCharacteristicUuid},
        {Options.BatteryCharacteristicUuid, Options.FirmwareCharacteristicUuid}, Guard([this](bool bSuccess, const FString& Error)
                                                                                     {
        if (!bSuccess)
        {
            FailConnect(Error);
            return;
        }
        bConnected = true;
        ContinueHandshake(); }));
}

void FSquareSession::ContinueHandshake()
{
    // Device info, notifications, heartbeat, club after 2 s, arm after 3 s, then the heartbeat timer.
    Transport->Read(Options.BatteryCharacteristicUuid, Guard([this](const TArray<uint8>& Battery)
                                                           {
        if (Battery.Num() > 0)
        {
            ReportBattery(Battery[0]);
        }
        Transport->Read(Options.FirmwareCharacteristicUuid, Guard([this](const TArray<uint8>& Firmware)
        {
            if (Firmware.Num() > 0)
            {
                const FString Version = SquareProtocol::ParseFirmware(Firmware);
                UE_LOG(LogOpenLaunchMonitor, Display, TEXT("Square Home firmware %s."), *Version);
                if (OnFirmwareChanged)
                {
                    OnFirmwareChanged(Version);
                }
            }
            Transport->Subscribe(Options.EventCharacteristicUuid, Guard([this](bool bSuccess, const FString& Error)
            {
                if (!bSuccess)
                {
                    FailConnect(Error);
                    return;
                }
                // Battery notifications are best effort.
                Transport->Subscribe(Options.BatteryCharacteristicUuid, Guard([this](bool, const FString&)
                {
                    SetStatus(ESquareSessionStatus::Connected);
                    QueueCommand(SquareProtocol::Heartbeat(NextSequence()), Guard([this](bool bSuccess, const FString& Error)
                    {
                        if (!bSuccess)
                        {
                            FailConnect(Error);
                            return;
                        }
                        Scheduler->Schedule(Options.ConnectionClubDelaySeconds, Guard([this]()
                        {
                            QueueCommand(SquareProtocol::Club(NextSequence(), ClubCode, Handedness), Guard([this](bool bSuccess, const FString& Error)
                            {
                                if (!bSuccess)
                                {
                                    FailConnect(Error);
                                    return;
                                }
                                Scheduler->Schedule(Options.ConnectionReadyDelaySeconds, Guard([this]()
                                {
                                    Arm();
                                    ScheduleHeartbeat();
                                    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Connection sequence complete."));
                                }));
                            }));
                        }));
                    }));
                }));
            }));
        })); }));
}

void FSquareSession::Disconnect()
{
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Disconnect requested."));
    ResetConnection();
    SetStatus(ESquareSessionStatus::Disconnected);
    EmitReady(false);
}

void FSquareSession::SetClub(const FString& InClubCode)
{
    TArray<uint8> CodeBytes;
    if (!InClubCode.IsEmpty() && (InClubCode.Len() != 4 || !SquareProtocol::FromHex(InClubCode, CodeBytes)))
    {
        UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Ignoring club code '%s': it must be four hex digits such as 0c06."), *InClubCode);
        return;
    }
    ClubCode = InClubCode.IsEmpty() ? FString(SquareProtocol::DriverClubCode) : InClubCode;
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Club set to %s."), *ClubCode);
    if (!bConnected)
    {
        return;
    }
    QueueCommand(SquareProtocol::Club(NextSequence(), ClubCode, Handedness), Guard([this](bool bSuccess, const FString& Error)
                                                                                 {
        if (!bSuccess)
        {
            EmitError(FString::Printf(TEXT("Could not set the club: %s"), *Error));
        } }));
    // The club is the device's shot-mode signal and is always followed by arming;
    // a shot still waiting to re-arm sends its own.
    if (bArmed && !bShotPendingRearm)
    {
        Arm();
    }
}

void FSquareSession::SetHandedness(int32 InHandedness)
{
    Handedness = InHandedness == 1 ? 1 : 0;
}

void FSquareSession::SetSpinMode(int32 InSpinMode)
{
    const int32 Normalized = InSpinMode == 0 ? 0 : 1;
    if (SpinMode == Normalized)
    {
        return;
    }
    SpinMode = Normalized;
    if (bConnected && bArmed && !bShotPendingRearm)
    {
        Arm();
    }
}

void FSquareSession::ResetConnection()
{
    ++Generation;
    Transport->StopScan();
    Transport->Disconnect();
    WriteQueue.Reset();
    bWriteInFlight = false;
    LastPayload.Reset();
    AwaitingClubForShot = 0;
    bConnected = false;
    bArmed = false;
    bShotPendingRearm = false;
    ArmFailures = 0;
    HeartbeatFailures = 0;
    bLastFrameRejected = false;
    LastLoggedBattery = -1;
}

void FSquareSession::FailConnect(const FString& Error)
{
    UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Connection failed: %s"), *Error);
    EmitError(Error.Contains(TEXT("not ready")) ? FString(DeviceNotReadyMessage) : FString::Printf(TEXT("Square connection failed: %s"), *Error));
    ResetConnection();
    SetStatus(ESquareSessionStatus::Disconnected);
    EmitReady(false);
}

void FSquareSession::QueueCommand(TArray<uint8> Command, IGattTransport::FCompletion OnComplete)
{
    if (!bConnected)
    {
        if (OnComplete)
        {
            OnComplete(false, TEXT("Square command channel is not available."));
        }
        return;
    }
    WriteQueue.Add({MoveTemp(Command), MoveTemp(OnComplete)});
    PumpWrites();
}

void FSquareSession::PumpWrites()
{
    if (bWriteInFlight || WriteQueue.IsEmpty())
    {
        return;
    }
    FPendingWrite Next = MoveTemp(WriteQueue[0]);
    WriteQueue.RemoveAt(0);
    bWriteInFlight = true;
    const FString Hex = SquareProtocol::ToHex(Next.Command);
    Transport->Write(Options.CommandCharacteristicUuid, Next.Command, Guard([this, Hex, Done = MoveTemp(Next.OnComplete)](bool bSuccess, const FString& Error)
                                                                          {
        bWriteInFlight = false;
        if (bSuccess)
        {
            UE_LOG(LogOpenLaunchMonitor, Verbose, TEXT("Wrote command %s."), *Hex);
        }
        else
        {
            UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Command %s failed: %s"), *Hex, *Error);
        }
        if (Done)
        {
            Done(bSuccess, Error);
        }
        PumpWrites(); }));
}

void FSquareSession::Arm()
{
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Arming ball detection (spin mode %s)."), SpinMode == 0 ? TEXT("calculated") : TEXT("measured"));
    QueueCommand(SquareProtocol::DetectBall(NextSequence(), 1, SpinMode), Guard([this](bool bSuccess, const FString& Error)
                                                                              {
        if (!bSuccess)
        {
            HandleArmFailed(Error);
            return;
        }
        ArmFailures = 0;
        if (bLastFrameRejected)
        {
            // A second unreadable strike can repeat the last frame exactly and still needs its own
            // re-arm. Readable frames stay filtered, so a late repeat never becomes a phantom shot.
            LastPayload.Reset();
            bLastFrameRejected = false;
        }
        bShotPendingRearm = false;
        bArmed = true;
        EmitReady(true);
        SetStatus(ESquareSessionStatus::Ready); }));
}

void FSquareSession::HandleArmFailed(const FString& Error)
{
    // An unarmed device detects nothing while the link still looks healthy, so never stop here.
    const FString Message = FString::Printf(TEXT("Could not arm the Square device: %s"), *Error);
    if (++ArmFailures >= ArmAttempts)
    {
        LoseConnection(TEXT("Arming failed repeatedly; dropping the link to reconnect."), Message);
        return;
    }
    EmitError(Message);
    Scheduler->Schedule(Options.ConnectionReadyDelaySeconds, Guard([this]()
                                                                 { Arm(); }));
}

void FSquareSession::ScheduleHeartbeat()
{
    if (Options.HeartbeatIntervalSeconds <= 0.0f)
    {
        return;
    }
    Scheduler->Schedule(Options.HeartbeatIntervalSeconds, Guard([this]()
                                                              {
        // Schedule the next one first: a failure that drops the link must also cancel it.
        ScheduleHeartbeat();
        QueueCommand(SquareProtocol::Heartbeat(NextSequence()), Guard([this](bool bSuccess, const FString& Error)
        {
            if (bSuccess)
            {
                HeartbeatFailures = 0;
                return;
            }
            EmitError(FString::Printf(TEXT("Square heartbeat failed: %s"), *Error));
            if (++HeartbeatFailures >= HeartbeatFailuresBeforeLinkLoss)
            {
                LoseConnection(TEXT("Heartbeats failed repeatedly; dropping the link to reconnect."), TEXT("The Square device stopped answering."));
            }
        })); }));
}

void FSquareSession::HandleDeviceDiscovered(const FGattDeviceInfo& Device)
{
    if (Status == ESquareSessionStatus::Scanning)
    {
        ConnectToDevice(Device);
    }
}

void FSquareSession::HandleNotification(const FString& CharacteristicUuid, const TArray<uint8>& Data)
{
    if (!bConnected)
    {
        return;
    }
    if (CharacteristicUuid.Equals(Options.EventCharacteristicUuid, ESearchCase::IgnoreCase))
    {
        HandleEventFrame(Data);
    }
    else if (CharacteristicUuid.Equals(Options.BatteryCharacteristicUuid, ESearchCase::IgnoreCase) && Data.Num() > 0)
    {
        ReportBattery(Data[0]);
    }
}

void FSquareSession::ReportBattery(int32 Percent)
{
    if (Percent != LastLoggedBattery)
    {
        LastLoggedBattery = Percent;
        UE_LOG(LogOpenLaunchMonitor, Display, TEXT("Battery %d%%."), Percent);
    }
    if (OnBatteryChanged)
    {
        OnBatteryChanged(Percent);
    }
}

void FSquareSession::HandleEventFrame(const TArray<uint8>& Data)
{
    FSquareSensorData Sensor;
    if (SquareProtocol::TryParseSensor(Data, Sensor))
    {
        EmitReady(Sensor.bBallReady && Sensor.bBallDetected);
        return;
    }

    FSquareClubMetrics Club;
    if (SquareProtocol::TryParseClub(Data, Club))
    {
        HandleClubFrame(Data, MoveTemp(Club));
        return;
    }

    if (!SquareProtocol::IsShotFrame(Data))
    {
        // Heartbeat acknowledgements (0x03), clock ticks (0x71) and the like.
        return;
    }

    // Every notification arrives at least twice; drop exact repeats.
    const FString Payload = SquareProtocol::ToHex(Data);
    if (Payload == LastPayload)
    {
        return;
    }
    LastPayload = Payload;

    FSquareShotMetrics Metrics;
    if (!SquareProtocol::TryParseShot(Data, Metrics))
    {
        // The device stops detecting after any strike, readable or not: re-arm.
        UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Ball frame rejected as implausible: %s"), *Payload);
        bLastFrameRejected = true;
        EmitReady(false);
        Scheduler->Schedule(Options.ConnectionReadyDelaySeconds, Guard([this]()
                                                                     { Arm(); }));
        return;
    }

    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Ball frame %s: %.2f m/s, VLA %.2f, HLA %.2f, spin %d rpm, axis %.2f, back %d, side %d."), *Payload,
        Metrics.BallSpeedMps, Metrics.VerticalAngleDeg, Metrics.HorizontalAngleDeg, Metrics.TotalSpinRpm, Metrics.SpinAxisDeg,
        Metrics.BackSpinRpm, Metrics.SideSpinRpm);
    bShotPendingRearm = true;
    bLastFrameRejected = false;
    EmitReady(false);
    const int32 ShotId = ++ShotCount;
    const uint32 ShotGeneration = Generation;
    if (OnShot)
    {
        OnShot(Metrics, ShotId);
    }
    if (Generation != ShotGeneration)
    {
        return;
    }

    // The next club frame belongs to this shot until the session re-arms.
    AwaitingClubForShot = ShotId;
    RequestClubMetrics(ShotId, 1);
    Scheduler->Schedule(Options.ConnectionReadyDelaySeconds, Guard([this, ShotId]()
                                                                 {
        if (AwaitingClubForShot == ShotId)
        {
            AwaitingClubForShot = 0;
        }
        Arm(); }));
}

void FSquareSession::HandleClubFrame(const TArray<uint8>& Data, FSquareClubMetrics Club)
{
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Club frame %s."), *SquareProtocol::ToHex(Data));
    if (AwaitingClubForShot == 0)
    {
        // Unsolicited or duplicate frame: it cannot be tied to a shot.
        return;
    }
    const int32 ShotId = AwaitingClubForShot;
    AwaitingClubForShot = 0;

    if (ClubCode == SquareProtocol::PutterClubCode)
    {
        // Putter swings are not measured for club angles.
        Club.PathDeg.Reset();
        Club.FaceDeg.Reset();
        Club.AttackDeg.Reset();
        Club.DynamicLoftDeg.Reset();
    }
    if (!Club.HasReading())
    {
        UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Shot %d club not tracked (no club sticker, or the device declined the strike)."), ShotId);
    }
    if (OnClub)
    {
        OnClub(Club, ShotId);
    }
}

void FSquareSession::RequestClubMetrics(int32 ShotId, int32 Attempt)
{
    // A failed request never stops the wait: the club frame may still arrive until re-arm.
    QueueCommand(SquareProtocol::RequestClubMetrics(NextSequence()), Guard([this, ShotId, Attempt](bool bSuccess, const FString& Error)
                                                                         {
        if (bSuccess || AwaitingClubForShot != ShotId)
        {
            return;
        }
        if (Attempt >= ClubRequestAttempts)
        {
            UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Club data request failed after %d attempts: %s"), Attempt, *Error);
            return;
        }
        Scheduler->Schedule(ClubRequestRetryDelaySeconds, Guard([this, ShotId, Attempt]()
        {
            if (AwaitingClubForShot == ShotId)
            {
                RequestClubMetrics(ShotId, Attempt + 1);
            }
        })); }));
}

void FSquareSession::HandleTransportDisconnected()
{
    LoseConnection(TEXT("Bluetooth link lost."), TEXT("Lost connection to the Square device."));
}

void FSquareSession::LoseConnection(const TCHAR* Reason, const FString& Message)
{
    if (!bConnected)
    {
        return;
    }
    UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("%s"), Reason);
    ResetConnection();
    EmitReady(false);
    EmitError(Message);
    SetStatus(ESquareSessionStatus::Disconnected);
    if (OnConnectionLost)
    {
        OnConnectionLost();
    }
}

void FSquareSession::SetStatus(ESquareSessionStatus NewStatus)
{
    if (Status == NewStatus)
    {
        return;
    }
    Status = NewStatus;
    if (OnStatusChanged)
    {
        OnStatusChanged(Status);
    }
}

void FSquareSession::EmitError(const FString& Message)
{
    UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("%s"), *Message);
    if (OnError)
    {
        OnError(Message);
    }
}

void FSquareSession::EmitReady(bool bReady)
{
    if (OnReadyChanged)
    {
        OnReadyChanged(bReady);
    }
}
