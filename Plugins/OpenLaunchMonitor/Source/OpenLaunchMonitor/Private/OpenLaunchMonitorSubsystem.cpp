// SPDX-License-Identifier: MIT
#include "OpenLaunchMonitorSubsystem.h"

#include "Bluetooth/GattTransport.h"
#include "OpenLaunchMonitorLog.h"
#include "Square/SquareProtocol.h"
#include "Square/SquareScheduler.h"
#include "Square/SquareSession.h"

namespace
{
    constexpr int32 MaxReconnectAttempts = 5;
    constexpr float MaxReconnectDelaySeconds = 30.0f;
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
    if (Session)
    {
        Session->Disconnect();
        Session.Reset();
    }
    Super::Deinitialize();
}

void UOpenLaunchMonitorSubsystem::Connect()
{
    CancelReconnect();
    ReconnectAttempts = 0;
    LastError.Reset();
    if (EnsureSession())
    {
        Session->Connect();
    }
}

void UOpenLaunchMonitorSubsystem::Disconnect()
{
    CancelReconnect();
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
}

void UOpenLaunchMonitorSubsystem::SetLeftHanded(bool bNewLeftHanded)
{
    bLeftHanded = bNewLeftHanded;
    if (Session)
    {
        Session->SetHandedness(bLeftHanded ? 1 : 0);
        Session->SetClub(ClubCode(Club));
    }
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
        Metrics.BallSpeedMps / 0.44704f, Metrics.VerticalAngleDeg, Metrics.HorizontalAngleDeg,
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
    Session->SetHandedness(bLeftHanded ? 1 : 0);
    Session->SetClub(ClubCode(Club));

    TWeakObjectPtr<UOpenLaunchMonitorSubsystem> WeakThis(this);
    Session->OnStatusChanged = [WeakThis](ESquareSessionStatus Status)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->SetConnectionState(ToConnectionState(Status));
        }
    };
    Session->OnError = [WeakThis](const FString& Message)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->ReportError(Message);
        }
    };
    Session->OnDeviceSelected = [WeakThis](const FGattDeviceInfo& Device)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->DeviceName = Device.Name;
        }
    };
    Session->OnBatteryChanged = [WeakThis](int32 Percent)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->BatteryPercent = Percent;
            WeakThis->OnBatteryChanged.Broadcast(Percent);
        }
    };
    Session->OnFirmwareChanged = [WeakThis](const FString& InFirmware)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->Firmware = InFirmware;
        }
    };
    Session->OnReadyChanged = [WeakThis](bool bReady)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->SetBallReady(bReady);
        }
    };
    Session->OnShot = [WeakThis](const FSquareShotMetrics& Metrics, int32 SessionShotId)
    {
        if (WeakThis.IsValid())
        {
            WeakThis->DeliverShot(Metrics, SquareHomeSource, SessionShotId);
        }
    };
    Session->OnClub = [WeakThis](const FSquareClubMetrics& Metrics, int32 SessionShotId)
    {
        if (!WeakThis.IsValid())
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
        if (WeakThis.IsValid())
        {
            WeakThis->ScheduleReconnect();
        }
    };
    return true;
}

void UOpenLaunchMonitorSubsystem::DeliverShot(const FSquareShotMetrics& Metrics, const FString& Source, int32 SessionShotId)
{
    FLaunchMonitorShot Shot;
    Shot.ShotNumber = ++ShotCounter;
    Shot.ReceivedUtc = FDateTime::UtcNow();
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

    if (SessionShotId > 0)
    {
        SessionShotNumbers.Add(SessionShotId, Shot.ShotNumber);
    }
    LastShot = Shot;
    LastClubData.Reset();

    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Shot %d (%s): %.2f mph (%.2f m/s), VLA %.2f, HLA %.2f, spin %d rpm, axis %.2f, back %d, side %d."),
        Shot.ShotNumber, *Shot.Source, Shot.GetBallSpeedMph(), Shot.BallSpeedMps, Shot.VerticalLaunchDeg, Shot.HorizontalLaunchDeg,
        Shot.TotalSpinRpm, Shot.SpinAxisDeg, Shot.BackSpinRpm, Shot.SideSpinRpm);
    OnShotReceivedNative.Broadcast(Shot);
    OnShotReceived.Broadcast(Shot);
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

    if (LastShot.IsSet() && LastShot->ShotNumber == ShotNumber)
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

void UOpenLaunchMonitorSubsystem::ScheduleReconnect()
{
    if (!bAutoReconnect || !Session || ReconnectAttempts >= MaxReconnectAttempts)
    {
        return;
    }
    const float Delay = FMath::Min(2.0f * FMath::Pow(2.0f, static_cast<float>(ReconnectAttempts)), MaxReconnectDelaySeconds);
    ++ReconnectAttempts;
    UE_LOG(LogOpenLaunchMonitor, Log, TEXT("Reconnecting in %.0f s (attempt %d of %d)."), Delay, ReconnectAttempts, MaxReconnectAttempts);
    TWeakObjectPtr<UOpenLaunchMonitorSubsystem> WeakThis(this);
    ReconnectHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakThis](float)
                                                               {
        if (WeakThis.IsValid())
        {
            WeakThis->ReconnectHandle.Reset();
            if (WeakThis->Session && WeakThis->ConnectionState == ELaunchMonitorConnectionState::Disconnected)
            {
                WeakThis->Session->Connect();
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
