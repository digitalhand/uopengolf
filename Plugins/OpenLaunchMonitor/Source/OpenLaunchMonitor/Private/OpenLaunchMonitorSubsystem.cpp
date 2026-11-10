// SPDX-License-Identifier: MIT
#include "OpenLaunchMonitorSubsystem.h"

#include "Bluetooth/GattTransport.h"
#include "HAL/IConsoleManager.h"
#include "IPAddress.h"
#include "OpenConnect/OpenConnectProtocol.h"
#include "OpenConnect/OpenConnectServer.h"
#include "OpenLaunchMonitorLog.h"
#include "Square/SquareProtocol.h"
#include "Square/SquareScheduler.h"
#include "Square/SquareSession.h"
#include "SocketSubsystem.h"

namespace
{
    /** About three minutes of retries: long enough for a battery swap or a slow power cycle. */
    constexpr int32 MaxReconnectAttempts = 12;
    /** Every this many attempts scans instead of connecting to the known device. */
    constexpr int32 ScanEveryAttempts = 4;
    /** Backoff 0.5, 1, 2, 4, then 8 s: a dropped link usually comes back at once. */
    constexpr float FirstReconnectDelaySeconds = 0.5f;
    constexpr float MaxReconnectDelaySeconds = 8.0f;

    TAutoConsoleVariable<float> CVarSquareClubDelay(TEXT("olm.Square.ClubDelay"), FSquareConnectionOptions().ConnectionClubDelaySeconds,
        TEXT("Seconds between the first heartbeat and the club command while connecting to a Square Home. Read at each connect."));
    TAutoConsoleVariable<float> CVarSquareArmDelay(TEXT("olm.Square.ArmDelay"), FSquareConnectionOptions().ConnectionArmDelaySeconds,
        TEXT("Seconds between the club command and the first arm while connecting to a Square Home. Read at each connect."));
    TAutoConsoleVariable<float> CVarSquareHeartbeatInterval(TEXT("olm.Square.HeartbeatInterval"), FSquareConnectionOptions().HeartbeatIntervalSeconds,
        TEXT("Seconds between Square Home heartbeats (minimum 0.5). Read at each connect."));
    TAutoConsoleVariable<int32> CVarSquareMissedHeartbeats(TEXT("olm.Square.MissedHeartbeats"), FSquareConnectionOptions().MissedHeartbeatsBeforeLinkLoss,
        TEXT("Heartbeat intervals in a row without any notification before a Square Home link is treated as lost. Read at each connect."));
    /** Rejected Open Connect messages logged at Warning per connection; later ones log at Verbose. */
    constexpr int32 MaxWarnedRejections = 5;
    /** Characters of a rejected message kept in the log. */
    constexpr int32 RejectionExcerptChars = 160;
    const TCHAR* SquareHomeSource = TEXT("Square Home");
    const TCHAR* ReplaySource = TEXT("Replay");

    struct FReplayShot
    {
        const TCHAR* Name;
        const TCHAR* BallFrame;
        const TCHAR* ClubFrame;
        const TCHAR* Note;
    };

    // Hardware captures come from open-lm-connector's Square Home test fixtures;
    // synthesized presets exercise the same decoder and events as hardware shots.
    // New frames omit back/side components so the decoder derives them from total/axis.
    const FReplayShot ReplayShots[] = {
        {TEXT("Low wood - 114.5 mph (23:05 screenshot)"), TEXT("110237FD13B702C1FF8D078E008C073000"), TEXT("11070FA000C4FFCAFECA08"), TEXT("FlightScope carry: 119.8 yd; apex: 14.6 ft. Synthetic replay of screenshot inputs.")},
        {TEXT("Square Home capture - 57.5 mph"), TEXT("1102370A0A9A09C8001A16E8FF7C15D0FF"), TEXT("11070FC0FB1A00A1FEFB0C"), TEXT("Recorded Square Home ball and club frames.")},
        {TEXT("Square Home putt capture"), TEXT("1102138101E6FF91010000000000000000"), TEXT("110700FFFFFFFFFFFFFFFF"), TEXT("Recorded putt with untracked club data.")},
        {TEXT("Mid iron - 75.7 mph (22:05 screenshot)"), TEXT("110237380D1E089FFF6F11F2F700800080"), TEXT(""), TEXT("FlightScope carry: 87.6 yd; apex: 35.0 ft. Screenshot used HLA right; replay retains original left launch.")},
        {TEXT("Driver - 150 mph"), TEXT("110237321AE20496001F09DE0300800080"), TEXT(""), TEXT("Synthetic driver with left spin axis; no FlightScope reference.")},
        {TEXT("Iron - 102.1 mph"), TEXT("110237D411CC0696000F1532FB00800080"), TEXT(""), TEXT("Synthetic iron with right spin axis; no FlightScope reference.")},
        {TEXT("Wedge - 66.4 mph"), TEXT("110237980B100974FF31193AFD00800080"), TEXT(""), TEXT("Synthetic wedge; no FlightScope reference.")},
        {TEXT("Chip - 24.7 mph"), TEXT("1102374F0402070000840C000000800080"), TEXT(""), TEXT("Synthetic straight chip; no FlightScope reference.")},
        {TEXT("High flop - 68.1 mph"), TEXT("110237E20B94112D00E02EACFF00800080"), TEXT(""), TEXT("Synthetic 45-degree, 12000 rpm flop; no FlightScope reference.")},
        {TEXT("Straight putt - 5 mph"), TEXT("110213E000000000000000000000800080"), TEXT("110700FFFFFFFFFFFFFFFF"), TEXT("Synthetic straight putt: zero launch angles and spin; 2.24 m/s (5 mph rounded) at Square packet precision.")},
    };

    const TCHAR* ClubCode(ELaunchMonitorClub Club)
    {
        switch (Club)
        {
        case ELaunchMonitorClub::Wood3:
            return TEXT("0305");
        case ELaunchMonitorClub::Wood5:
            return TEXT("0505");
        case ELaunchMonitorClub::Wood7:
            return TEXT("0705");
        case ELaunchMonitorClub::Iron4:
            return TEXT("0406");
        case ELaunchMonitorClub::Iron5:
            return TEXT("0506");
        case ELaunchMonitorClub::Iron6:
            return TEXT("0606");
        case ELaunchMonitorClub::Iron7:
            return TEXT("0706");
        case ELaunchMonitorClub::Iron8:
            return TEXT("0806");
        case ELaunchMonitorClub::Iron9:
            return TEXT("0906");
        case ELaunchMonitorClub::PitchingWedge:
            return TEXT("0a06");
        case ELaunchMonitorClub::GapWedge:
            return TEXT("0b06");
        case ELaunchMonitorClub::SandWedge:
            return TEXT("0c06");
        case ELaunchMonitorClub::LobWedge:
            // Extrapolated from PW/GW/SW (0a/0b/0c). The Square Home arms and measures with it (2026-10-15), but
            // nothing shows it treats it differently from the sand wedge; open-lm-connector reports no LW code.
            return TEXT("0d06");
        case ELaunchMonitorClub::Putter:
            return SquareProtocol::PutterClubCode;
        case ELaunchMonitorClub::Driver:
        default:
            return SquareProtocol::DriverClubCode;
        }
    }

    ELaunchMonitorValueSource ToValueSource(ESquareValueSource Source)
    {
        switch (Source)
        {
        case ESquareValueSource::Packet:
            return ELaunchMonitorValueSource::Measured;
        case ESquareValueSource::Derived:
            return ELaunchMonitorValueSource::Derived;
        default:
            return ELaunchMonitorValueSource::Missing;
        }
    }

    ELaunchMonitorConnectionState ToConnectionState(ESquareSessionStatus Status)
    {
        switch (Status)
        {
        case ESquareSessionStatus::Scanning:
            return ELaunchMonitorConnectionState::Scanning;
        case ESquareSessionStatus::Connecting:
            return ELaunchMonitorConnectionState::Connecting;
        case ESquareSessionStatus::Connected:
            return ELaunchMonitorConnectionState::Connected;
        case ESquareSessionStatus::Ready:
            return ELaunchMonitorConnectionState::Ready;
        default:
            return ELaunchMonitorConnectionState::Disconnected;
        }
    }

    void CopyOptional(const TOptional<float>& Value, bool& bOutHas, float& OutValue)
    {
        bOutHas = Value.IsSet();
        OutValue = Value.Get(0.0f);
    }
} // namespace

void UOpenLaunchMonitorSubsystem::Deinitialize()
{
    CancelReconnect();
    if (OpenConnect)
    {
        OpenConnect->Stop();
        OpenConnect.Reset();
    }
    if (Session)
    {
        // Disarm and release before the transport is destroyed, so the Square's lights go off on exit too.
        Session->Shutdown();
        Session.Reset();
    }
    Super::Deinitialize();
}

void UOpenLaunchMonitorSubsystem::Connect()
{
    CancelReconnect();
    ReconnectAttempts = 0;
    LastError.Reset();
    if (Provider == ELaunchMonitorProvider::OpenConnect)
    {
        StartOpenConnect();
        return;
    }
    if (EnsureSession())
    {
        ApplyTiming();
        Session->Connect();
    }
}

void UOpenLaunchMonitorSubsystem::ApplyTiming()
{
    if (Session)
    {
        Session->SetTiming(CVarSquareClubDelay.GetValueOnGameThread(), CVarSquareArmDelay.GetValueOnGameThread(),
            CVarSquareHeartbeatInterval.GetValueOnGameThread(), CVarSquareMissedHeartbeats.GetValueOnGameThread());
    }
}

void UOpenLaunchMonitorSubsystem::Disconnect()
{
    CancelReconnect();
    StopOpenConnect();
    if (Session)
    {
        Session->Disconnect();
    }
}

void UOpenLaunchMonitorSubsystem::SetClub(ELaunchMonitorClub NewClub)
{
    Club = NewClub;
    if (Session)
    {
        Session->SetClub(ClubCode(Club));
    }
    SendOpenConnectPlayer();
}

void UOpenLaunchMonitorSubsystem::SetSpinMode(ESquareSpinMode Mode)
{
    SpinMode = Mode == ESquareSpinMode::Calculated ? Mode : ESquareSpinMode::Measured;
    if (Session) Session->SetSpinMode(SpinMode == ESquareSpinMode::Calculated ? 0 : 1);
}

void UOpenLaunchMonitorSubsystem::SetLeftHanded(bool bNewLeftHanded)
{
    bLeftHanded = bNewLeftHanded;
    if (Session)
    {
        Session->SetHandedness(bLeftHanded ? 1 : 0);
        Session->SetClub(ClubCode(Club));
    }
    SendOpenConnectPlayer();
}

bool UOpenLaunchMonitorSubsystem::InjectReplayShot()
{
    const int32 Index = ReplayIndex;
    ReplayIndex = (ReplayIndex + 1) % UE_ARRAY_COUNT(ReplayShots);
    return InjectReplayShotAt(Index);
}

TArray<FString> UOpenLaunchMonitorSubsystem::GetReplayShotNames()
{
    TArray<FString> Names;
    for (const FReplayShot& Replay : ReplayShots)
    {
        Names.Add(Replay.Name);
    }
    return Names;
}

FString UOpenLaunchMonitorSubsystem::GetReplayShotDescription(int32 Index)
{
    if (Index < 0 || Index >= static_cast<int32>(UE_ARRAY_COUNT(ReplayShots)))
    {
        return FString();
    }
    const FReplayShot& Replay = ReplayShots[Index];
    TArray<uint8> Bytes;
    FSquareShotMetrics Metrics;
    if (!SquareProtocol::FromHex(Replay.BallFrame, Bytes) || !SquareProtocol::TryParseShot(Bytes, Metrics))
    {
        return FString();
    }
    return FString::Printf(TEXT("%.2f mph   |   VLA %.2f deg   |   HLA %.2f deg\n%d rpm   |   Spin axis %.2f deg\nNegative HLA / axis = left; positive = right.\n\n%s"),
        Metrics.BallSpeedMps * LaunchMonitorMphPerMps, Metrics.VerticalAngleDeg, Metrics.HorizontalAngleDeg,
        Metrics.TotalSpinRpm, Metrics.SpinAxisDeg, Replay.Note);
}

bool UOpenLaunchMonitorSubsystem::InjectReplayShotAt(int32 Index)
{
    if (Index < 0 || Index >= static_cast<int32>(UE_ARRAY_COUNT(ReplayShots)))
    {
        return false;
    }
    const FReplayShot& Replay = ReplayShots[Index];
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Replaying: %s."), Replay.Name);
    return InjectReplayFrames(Replay.BallFrame, Replay.ClubFrame);
}

bool UOpenLaunchMonitorSubsystem::InjectReplayFrames(const FString& BallFrameHex, const FString& ClubFrameHex)
{
    TArray<uint8> BallFrame;
    FSquareShotMetrics Metrics;
    if (!SquareProtocol::FromHex(BallFrameHex, BallFrame) || !SquareProtocol::TryParseShot(BallFrame, Metrics))
    {
        const FString Message = FString::Printf(TEXT("Replay ball frame is not a plausible Square shot: %s"), *BallFrameHex);
        UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("%s"), *Message);
        ReportError(Message);
        return false;
    }
    DeliverShot(Metrics, ReplaySource, 0);

    TArray<uint8> ClubFrame;
    FSquareClubMetrics ClubMetrics;
    if (!ClubFrameHex.IsEmpty() && SquareProtocol::FromHex(ClubFrameHex, ClubFrame) && SquareProtocol::TryParseClub(ClubFrame, ClubMetrics))
    {
        DeliverClubData(ClubMetrics, ShotCounter);
    }
    return true;
}

bool UOpenLaunchMonitorSubsystem::GetLastShot(FLaunchMonitorShot& OutShot) const
{
    if (LastShot.IsSet())
    {
        OutShot = LastShot.GetValue();
        return true;
    }
    return false;
}

bool UOpenLaunchMonitorSubsystem::GetLastClubData(FLaunchMonitorClubData& OutClubData) const
{
    if (LastClubData.IsSet())
    {
        OutClubData = LastClubData.GetValue();
        return true;
    }
    return false;
}

FText UOpenLaunchMonitorSubsystem::GetClubDisplayName(ELaunchMonitorClub InClub)
{
    return StaticEnum<ELaunchMonitorClub>()->GetDisplayNameTextByValue(static_cast<int64>(InClub));
}

bool UOpenLaunchMonitorSubsystem::EnsureSession()
{
    if (Session)
    {
        return true;
    }
    const TSharedPtr<IGattTransport> Transport = CreatePlatformGattTransport();
    if (!Transport)
    {
        ReportError(TEXT("Bluetooth launch monitors are not supported on this platform yet; replay shots still work."));
        return false;
    }

    Session = MakeShared<FSquareSession>(Transport.ToSharedRef(), MakeShared<FTickerSquareScheduler>());
    Session->Initialize();
    Session->SetSpinMode(SpinMode == ESquareSpinMode::Calculated ? 0 : 1);
    Session->SetHandedness(bLeftHanded ? 1 : 0);
    Session->SetClub(ClubCode(Club));

    // After a switch to another provider, late events from the Square session are ignored.
    TWeakObjectPtr<UOpenLaunchMonitorSubsystem> WeakThis(this);
    Session->OnStatusChanged = [WeakThis](ESquareSessionStatus Status)
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->SetConnectionState(ToConnectionState(Status));
        }
    };
    Session->OnError = [WeakThis](const FString& Message)
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->ReportError(Message);
        }
    };
    Session->OnDeviceSelected = [WeakThis](const FGattDeviceInfo& Device)
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->DeviceName = Device.Name;
        }
    };
    Session->OnBatteryChanged = [WeakThis](int32 Percent)
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->BatteryPercent = Percent;
            WeakThis->OnBatteryChanged.Broadcast(Percent);
        }
    };
    Session->OnFirmwareChanged = [WeakThis](const FString& InFirmware)
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->Firmware = InFirmware;
        }
    };
    Session->OnReadyChanged = [WeakThis](bool bReady)
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->SetBallReady(bReady);
        }
    };
    Session->OnShot = [WeakThis](const FSquareShotMetrics& Metrics, int32 SessionShotId)
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->DeliverShot(Metrics, SquareHomeSource, SessionShotId);
        }
    };
    Session->OnClub = [WeakThis](const FSquareClubMetrics& Metrics, int32 SessionShotId)
    {
        if (!WeakThis.IsValid() || !WeakThis->IsSquareActive())
        {
            return;
        }
        int32 ShotNumber = 0;
        if (WeakThis->SessionShotNumbers.RemoveAndCopyValue(SessionShotId, ShotNumber))
        {
            WeakThis->DeliverClubData(Metrics, ShotNumber);
        }
    };
    Session->OnConnectionLost = [WeakThis]()
    {
        if (WeakThis.IsValid() && WeakThis->IsSquareActive())
        {
            WeakThis->ScheduleReconnect();
        }
    };
    return true;
}

void UOpenLaunchMonitorSubsystem::DeliverShot(const FSquareShotMetrics& Metrics, const FString& Source, int32 SessionShotId)
{
    FLaunchMonitorShot Shot;
    Shot.Source = Source;
    Shot.ShotType = Metrics.ShotType == ESquareShotType::Full   ? ELaunchMonitorShotType::Full
                    : Metrics.ShotType == ESquareShotType::Putt ? ELaunchMonitorShotType::Putt
                                                                : ELaunchMonitorShotType::Unknown;
    Shot.BallSpeedMps = Metrics.BallSpeedMps;
    Shot.VerticalLaunchDeg = Metrics.VerticalAngleDeg;
    Shot.HorizontalLaunchDeg = Metrics.HorizontalAngleDeg;
    Shot.TotalSpinRpm = Metrics.TotalSpinRpm;
    Shot.SpinAxisDeg = Metrics.SpinAxisDeg;
    Shot.BackSpinRpm = Metrics.BackSpinRpm;
    Shot.SideSpinRpm = Metrics.SideSpinRpm;
    Shot.BallSpeedSource = ToValueSource(Metrics.BallSpeedSource);
    Shot.VerticalLaunchSource = ToValueSource(Metrics.VerticalAngleSource);
    Shot.HorizontalLaunchSource = ToValueSource(Metrics.HorizontalAngleSource);
    Shot.TotalSpinSource = ToValueSource(Metrics.TotalSpinSource);
    Shot.SpinAxisSource = ToValueSource(Metrics.SpinAxisSource);
    Shot.BackSpinSource = ToValueSource(Metrics.BackSpinSource);
    Shot.SideSpinSource = ToValueSource(Metrics.SideSpinSource);

    if (Source == SquareHomeSource && SpinMode == ESquareSpinMode::Calculated)
    {
        for (ELaunchMonitorValueSource* Value : {&Shot.TotalSpinSource, &Shot.SpinAxisSource, &Shot.BackSpinSource, &Shot.SideSpinSource})
            if (*Value != ELaunchMonitorValueSource::Missing) *Value = ELaunchMonitorValueSource::Derived;
    }
    DeliverShot(MoveTemp(Shot), SessionShotId);
}

void UOpenLaunchMonitorSubsystem::DeliverShot(FLaunchMonitorShot Shot, int32 SessionShotId)
{
    Shot.ShotNumber = ++ShotCounter;
    Shot.ReceivedUtc = FDateTime::UtcNow();
    if (SessionShotId > 0)
    {
        // Club data only ever follows the newest shot, so older entries could never be claimed.
        SessionShotNumbers.Reset();
        SessionShotNumbers.Add(SessionShotId, Shot.ShotNumber);
    }
    LastShot = Shot;
    LastClubData.Reset();

    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Shot %d (%s): %.2f mph (%.2f m/s), VLA %.2f, HLA %.2f, spin %d rpm, axis %.2f, back %d, side %d."),
        Shot.ShotNumber, *Shot.Source, Shot.GetBallSpeedMph(), Shot.BallSpeedMps, Shot.VerticalLaunchDeg, Shot.HorizontalLaunchDeg,
        Shot.TotalSpinRpm, Shot.SpinAxisDeg, Shot.BackSpinRpm, Shot.SideSpinRpm);
    // Handlers run synchronously; one that refuses the shot calls RejectShot during the broadcast.
    BroadcastingShotNumber = Shot.ShotNumber;
    BroadcastRejection.Reset();
    OnShotReceivedNative.Broadcast(Shot);
    OnShotReceived.Broadcast(Shot);
    BroadcastingShotNumber = 0;
}

void UOpenLaunchMonitorSubsystem::DeliverClubData(const FSquareClubMetrics& Metrics, int32 ShotNumber)
{
    FLaunchMonitorClubData ClubData;
    ClubData.ShotNumber = ShotNumber;
    CopyOptional(Metrics.PathDeg, ClubData.bHasPath, ClubData.PathDeg);
    CopyOptional(Metrics.FaceDeg, ClubData.bHasFaceToTarget, ClubData.FaceToTargetDeg);
    CopyOptional(Metrics.AttackDeg, ClubData.bHasAngleOfAttack, ClubData.AngleOfAttackDeg);
    CopyOptional(Metrics.DynamicLoftDeg, ClubData.bHasDynamicLoft, ClubData.DynamicLoftDeg);
    CopyOptional(Metrics.ImpactHorizontalMm, ClubData.bHasImpactHorizontal, ClubData.ImpactHorizontalMm);
    CopyOptional(Metrics.ImpactVerticalMm, ClubData.bHasImpactVertical, ClubData.ImpactVerticalMm);
    DeliverClubData(ClubData);
}

void UOpenLaunchMonitorSubsystem::DeliverClubData(const FLaunchMonitorClubData& ClubData)
{
    if (LastShot.IsSet() && LastShot->ShotNumber == ClubData.ShotNumber)
    {
        LastClubData = ClubData;
    }
    OnClubDataReceivedNative.Broadcast(ClubData);
    OnClubDataReceived.Broadcast(ClubData);
}

void UOpenLaunchMonitorSubsystem::SetConnectionState(ELaunchMonitorConnectionState NewState)
{
    if (ConnectionState == NewState)
    {
        return;
    }
    ConnectionState = NewState;
    if (NewState == ELaunchMonitorConnectionState::Ready)
    {
        ReconnectAttempts = 0;
        LastError.Reset();
    }
    else if (NewState == ELaunchMonitorConnectionState::Disconnected && ReconnectAttempts > 0 && !ReconnectHandle.IsValid())
    {
        // A reconnect attempt failed; keep trying until the limit.
        ScheduleReconnect();
    }
    OnConnectionStateChanged.Broadcast(NewState);
}

void UOpenLaunchMonitorSubsystem::SetBallReady(bool bNewBallReady)
{
    if (bBallReady != bNewBallReady)
    {
        bBallReady = bNewBallReady;
        OnBallReadyChanged.Broadcast(bBallReady);
    }
}

void UOpenLaunchMonitorSubsystem::ReportError(const FString& Message)
{
    LastError = Message;
    OnError.Broadcast(Message);
}

void UOpenLaunchMonitorSubsystem::SetAutoReconnect(bool bEnabled)
{
    bAutoReconnect = bEnabled;
    if (!bEnabled) CancelReconnect();
}

void UOpenLaunchMonitorSubsystem::ScheduleReconnect()
{
    // A link lost during a reconnect's handshake reports both a state change and a lost
    // connection. One pending attempt is enough, and CancelReconnect can only remove one.
    if (!bAutoReconnect || !Session || !IsSquareActive() || ReconnectAttempts >= MaxReconnectAttempts || ReconnectHandle.IsValid())
    {
        return;
    }
    const float Delay = FMath::Min(FirstReconnectDelaySeconds * FMath::Pow(2.0f, static_cast<float>(ReconnectAttempts)), MaxReconnectDelaySeconds);
    ++ReconnectAttempts;
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Reconnecting in %.1f s (attempt %d of %d)."), Delay, ReconnectAttempts, MaxReconnectAttempts);
    TWeakObjectPtr<UOpenLaunchMonitorSubsystem> WeakThis(this);
    ReconnectHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
                                                               {
        if (WeakThis.IsValid())
        {
            WeakThis->ReconnectHandle.Reset();
            if (WeakThis->bAutoReconnect && WeakThis->IsSquareActive() && WeakThis->Session && WeakThis->ConnectionState == ELaunchMonitorConnectionState::Disconnected)
            {
                WeakThis->ApplyTiming();
                // Connect straight to the known device: after a power cycle it may not advertise, so a scan
                // finds nothing (measured on a Square Home). Every fourth attempt scans, in case it changed.
                if (WeakThis->ReconnectAttempts % ScanEveryAttempts != 0 && WeakThis->Session->HasLastDevice())
                {
                    WeakThis->Session->Reconnect();
                }
                else
                {
                    WeakThis->Session->Connect();
                }
            }
        }
        return false; }),
        Delay);
}

void UOpenLaunchMonitorSubsystem::CancelReconnect()
{
    if (ReconnectHandle.IsValid())
    {
        FTSTicker::RemoveTicker(ReconnectHandle);
        ReconnectHandle.Reset();
    }
    ReconnectAttempts = 0;
}

void UOpenLaunchMonitorSubsystem::SetProvider(ELaunchMonitorProvider NewProvider)
{
    if (Provider == NewProvider)
    {
        return;
    }
    Disconnect();
    Provider = NewProvider;
    bOpenConnectClient = false;
    DeviceName.Reset();
    Firmware.Reset();
    BatteryPercent = -1;
    LastError.Reset();
    SetBallReady(false);
    SetConnectionState(ELaunchMonitorConnectionState::Disconnected);
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Launch-monitor provider: %s."),
        *StaticEnum<ELaunchMonitorProvider>()->GetDisplayNameTextByValue(static_cast<int64>(Provider)).ToString());
}

void UOpenLaunchMonitorSubsystem::SetOpenConnectEndpoint(int32 Port, bool bAllowLan)
{
    Port = FMath::Clamp(Port, 1, 65535);
    if (Port == OpenConnectPort && bAllowLan == bOpenConnectAllowLan)
    {
        return;
    }
    OpenConnectPort = Port;
    bOpenConnectAllowLan = bAllowLan;
    if (OpenConnect)
    {
        StartOpenConnect();
    }
}

FString UOpenLaunchMonitorSubsystem::GetOpenConnectAddress() const
{
    const FString Host = bOpenConnectAllowLan && !OpenConnectLanHost.IsEmpty() ? OpenConnectLanHost : FString(TEXT("127.0.0.1"));
    return FString::Printf(TEXT("%s:%d"), *Host, OpenConnectPort);
}

bool UOpenLaunchMonitorSubsystem::IsOpenConnectListening() const
{
    return OpenConnect.IsValid() && OpenConnect->IsRunning();
}

bool UOpenLaunchMonitorSubsystem::GetOpenConnectLastMessageUtc(FDateTime& OutUtc) const
{
    if (OpenConnectLastMessageUtc.IsSet())
    {
        OutUtc = OpenConnectLastMessageUtc.GetValue();
        return true;
    }
    return false;
}

void UOpenLaunchMonitorSubsystem::RejectShot(int32 ShotNumber, const FString& Reason)
{
    if (ShotNumber != 0 && ShotNumber == BroadcastingShotNumber && !BroadcastRejection.IsSet())
    {
        BroadcastRejection = Reason;
    }
}

void UOpenLaunchMonitorSubsystem::StartOpenConnect()
{
    StopOpenConnect();
    LastError.Reset();
    if (bOpenConnectAllowLan)
    {
        bool bCanBindAll = false;
        OpenConnectLanHost = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->GetLocalHostAddr(*GLog, bCanBindAll)->ToString(false);
    }
    OpenConnect = MakeShared<FOpenConnectServer>();
    TWeakObjectPtr<UOpenLaunchMonitorSubsystem> WeakThis(this);
    OpenConnect->OnListening = [WeakThis]()
    {
        if (WeakThis.IsValid())
        {
            UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Open Connect listening on %s%s."), *WeakThis->GetOpenConnectAddress(),
                WeakThis->bOpenConnectAllowLan ? TEXT(" (all network interfaces)") : TEXT(""));
            WeakThis->SetConnectionState(ELaunchMonitorConnectionState::Connecting);
        }
    };
    OpenConnect->OnListenFailed = [WeakThis](const FString& Error)
    {
        if (WeakThis.IsValid())
        {
            UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("%s"), *Error);
            WeakThis->StopOpenConnect();
            WeakThis->ReportError(Error);
        }
    };
    OpenConnect->OnClientConnected = [WeakThis](const FString& Peer)
    {
        if (WeakThis.IsValid())
        {
            UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Open Connect connector attached from %s."), *Peer);
            WeakThis->bOpenConnectClient = true;
            WeakThis->OpenConnectRejections = 0;
            WeakThis->DeviceName = Peer;
            WeakThis->LastError.Reset();
            WeakThis->SetConnectionState(ELaunchMonitorConnectionState::Connected);
            WeakThis->SendOpenConnectPlayer();
        }
    };
    OpenConnect->OnClientDisconnected = [WeakThis]()
    {
        if (WeakThis.IsValid())
        {
            UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Open Connect connector detached; still listening."));
            WeakThis->bOpenConnectClient = false;
            WeakThis->DeviceName.Reset();
            WeakThis->SetBallReady(false);
            WeakThis->SetConnectionState(ELaunchMonitorConnectionState::Connecting);
        }
    };
    OpenConnect->OnMessage = [WeakThis](const FString& Json)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->HandleOpenConnectMessage(Json);
        }
    };
    OpenConnect->OnOverflow = [WeakThis]()
    {
        if (WeakThis.IsValid() && WeakThis->OpenConnect)
        {
            WeakThis->OpenConnect->Send(OpenConnectProtocol::Response(413, TEXT("Payload too large")));
        }
    };
    if (!OpenConnect->Start(OpenConnectPort, bOpenConnectAllowLan))
    {
        OpenConnect.Reset();
        ReportError(TEXT("Could not start the Open Connect listener thread."));
    }
}

void UOpenLaunchMonitorSubsystem::StopOpenConnect()
{
    if (!OpenConnect)
    {
        return;
    }
    // Stop joins the worker, which closes the connector and listening sockets before it returns,
    // so the port is free as soon as this call (and any provider switch) completes.
    OpenConnect->Stop();
    OpenConnect.Reset();
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Open Connect stopped; port %d closed."), OpenConnectPort);
    bOpenConnectClient = false;
    if (Provider == ELaunchMonitorProvider::OpenConnect)
    {
        DeviceName.Reset();
        SetBallReady(false);
        SetConnectionState(ELaunchMonitorConnectionState::Disconnected);
    }
}

void UOpenLaunchMonitorSubsystem::HandleOpenConnectMessage(const FString& Json)
{
    OpenConnectLastMessageUtc = FDateTime::UtcNow();
    FOpenConnectMessage Message;
    FString Error;
    if (!OpenConnectProtocol::Decode(Json, Message, Error))
    {
        // The message is untrusted: log a short excerpt without control characters, and only the first few loudly.
        const FString Excerpt = OpenConnectProtocol::SanitizeText(Json, RejectionExcerptChars);
        if (++OpenConnectRejections <= MaxWarnedRejections)
        {
            UE_LOG(LogOpenLaunchMonitor, Warning, TEXT("Open Connect message rejected (%s): %s%s"), *Error, *Excerpt,
                OpenConnectRejections == MaxWarnedRejections ? TEXT(" (further rejections log at Verbose)") : TEXT(""));
        }
        else
        {
            UE_LOG(LogOpenLaunchMonitor, Verbose, TEXT("Open Connect message rejected (%s): %s"), *Error, *Excerpt);
        }
        if (OpenConnect)
        {
            OpenConnect->Send(OpenConnectProtocol::Response(501, Error));
        }
        return;
    }
    if (!Message.DeviceId.IsEmpty())
    {
        DeviceName = Message.DeviceId;
    }
    if (Message.bReady.IsSet())
    {
        SetBallReady(Message.bReady.GetValue());
        SetConnectionState(Message.bReady.GetValue() ? ELaunchMonitorConnectionState::Ready : ELaunchMonitorConnectionState::Connected);
    }
    if (!Message.Shot.IsSet())
    {
        // The readiness broadcasts above may have stopped the listener.
        if (OpenConnect)
            OpenConnect->Send(OpenConnectProtocol::Response(200, Message.bHeartbeat ? TEXT("Heartbeat received") : TEXT("Status received")));
        return;
    }

    // A connector that sends shots without readiness flags is treated as ready once it shoots.
    SetConnectionState(ELaunchMonitorConnectionState::Ready);
    FLaunchMonitorShot Shot = Message.Shot.GetValue();
    Shot.Source = Message.DeviceId.IsEmpty() ? FString(LaunchMonitorOpenConnectSource)
                                             : FString::Printf(TEXT("%s: %s"), LaunchMonitorOpenConnectSource, *Message.DeviceId);
    DeliverShot(MoveTemp(Shot), 0);
    const TOptional<FString> Rejection = BroadcastRejection;
    BroadcastRejection.Reset();
    if (Message.Club.IsSet())
    {
        FLaunchMonitorClubData ClubData = Message.Club.GetValue();
        ClubData.ShotNumber = ShotCounter;
        DeliverClubData(ClubData);
    }
    // A handler may have stopped the listener while the shot was broadcast.
    if (OpenConnect)
    {
        OpenConnect->Send(Rejection.IsSet() ? OpenConnectProtocol::Response(501, Rejection.GetValue())
                                            : OpenConnectProtocol::Response(200, TEXT("Shot received successfully")));
    }
}

void UOpenLaunchMonitorSubsystem::SendOpenConnectPlayer()
{
    if (OpenConnect && bOpenConnectClient)
    {
        OpenConnect->Send(OpenConnectProtocol::PlayerInformation(bLeftHanded, Club));
    }
}
