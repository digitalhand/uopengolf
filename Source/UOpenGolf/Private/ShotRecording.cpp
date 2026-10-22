// SPDX-License-Identifier: MIT
#include "ShotRecording.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "HAL/PlatformTime.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if PLATFORM_LINUX
#include <unistd.h>
#endif

DEFINE_LOG_CATEGORY_STATIC(LogUOpenGolfRecording, Log, All);

namespace
{
    constexpr double RecordingMetersPerYard = 0.9144;
    constexpr double RecordingFeetPerMeter = 3.28084;
    const TCHAR* const RecordingDeviceSource = TEXT("Square Home");

    /** Writes a decimal value rounded to hundredths ("10.01"); whole-number fields use SetNumberField. */
    void SetRecordingDecimal(FJsonObject& Object, const TCHAR* Name, double Value)
    {
        FString Text = FString::Printf(TEXT("%.2f"), Value);
        if (Text == TEXT("-0.00"))
        {
            Text = TEXT("0.00");
        }
        Object.SetField(Name, MakeShared<FJsonValueNumberString>(MoveTemp(Text)));
    }

    bool IsRecordedValue(ELaunchMonitorValueSource Source)
    {
        return Source != ELaunchMonitorValueSource::Missing;
    }

    TSharedRef<FJsonObject> MakeRecordingBallData(const FLaunchMonitorShot& Shot)
    {
        const TSharedRef<FJsonObject> Ball = MakeShared<FJsonObject>();
        if (IsRecordedValue(Shot.BallSpeedSource))
        {
            SetRecordingDecimal(*Ball, TEXT("Speed"), Shot.GetBallSpeedMph());
        }
        if (IsRecordedValue(Shot.VerticalLaunchSource))
        {
            SetRecordingDecimal(*Ball, TEXT("VLA"), Shot.VerticalLaunchDeg);
        }
        if (IsRecordedValue(Shot.HorizontalLaunchSource))
        {
            SetRecordingDecimal(*Ball, TEXT("HLA"), Shot.HorizontalLaunchDeg);
        }
        if (IsRecordedValue(Shot.TotalSpinSource))
        {
            Ball->SetNumberField(TEXT("TotalSpin"), Shot.TotalSpinRpm);
        }
        if (IsRecordedValue(Shot.SpinAxisSource))
        {
            SetRecordingDecimal(*Ball, TEXT("SpinAxis"), Shot.SpinAxisDeg);
        }
        if (IsRecordedValue(Shot.BackSpinSource))
        {
            Ball->SetNumberField(TEXT("BackSpin"), Shot.BackSpinRpm);
        }
        if (IsRecordedValue(Shot.SideSpinSource))
        {
            Ball->SetNumberField(TEXT("SideSpin"), Shot.SideSpinRpm);
        }
        // Field for a device-reported carry; the Square does not report one.
        Ball->SetNumberField(TEXT("CarryDistance"), 0.0);
        return Ball;
    }

    TSharedRef<FJsonObject> MakeRecordingClubData(const FLaunchMonitorClubData& Club)
    {
        const TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
        if (Club.bHasPath)
        {
            SetRecordingDecimal(*Data, TEXT("Path"), Club.PathDeg);
        }
        if (Club.bHasFaceToTarget)
        {
            SetRecordingDecimal(*Data, TEXT("FaceToTarget"), Club.FaceToTargetDeg);
        }
        if (Club.bHasAngleOfAttack)
        {
            SetRecordingDecimal(*Data, TEXT("AngleOfAttack"), Club.AngleOfAttackDeg);
        }
        if (Club.bHasDynamicLoft)
        {
            SetRecordingDecimal(*Data, TEXT("Loft"), Club.DynamicLoftDeg);
        }
        if (Club.bHasImpactHorizontal)
        {
            SetRecordingDecimal(*Data, TEXT("HorizontalFaceImpact"), Club.ImpactHorizontalMm);
        }
        if (Club.bHasImpactVertical)
        {
            SetRecordingDecimal(*Data, TEXT("VerticalFaceImpact"), Club.ImpactVerticalMm);
        }
        return Data;
    }

    TSharedRef<FJsonObject> MakeRecordingEngineResult(const FOpenFairwayShotResult& Result)
    {
        const TSharedRef<FJsonObject> Engine = MakeShared<FJsonObject>();
        SetRecordingDecimal(*Engine, TEXT("CarryYd"), Result.CarryM / RecordingMetersPerYard);
        SetRecordingDecimal(*Engine, TEXT("TotalYd"), Result.TotalM / RecordingMetersPerYard);
        SetRecordingDecimal(*Engine, TEXT("RollYd"), Result.RollM / RecordingMetersPerYard);
        SetRecordingDecimal(*Engine, TEXT("LateralYd"), Result.LateralM / RecordingMetersPerYard);
        SetRecordingDecimal(*Engine, TEXT("ApexFt"), Result.ApexM * RecordingFeetPerMeter);
        SetRecordingDecimal(*Engine, TEXT("HangTimeS"), Result.HangTimeS);
        SetRecordingDecimal(*Engine, TEXT("LandingAngleDeg"), Result.LandingAngleDeg);
        Engine->SetBoolField(TEXT("AtRest"), Result.bAtRest);
        Engine->SetStringField(TEXT("TerminationReason"), Result.TerminationReason == EOpenFairwayShotTermination::SafetyStop ? TEXT("SafetyStop") : TEXT("Rest"));
        if (Result.TerminationReason == EOpenFairwayShotTermination::SafetyStop)
        {
            Engine->SetField(TEXT("TotalYd"), MakeShared<FJsonValueNull>());
            Engine->SetField(TEXT("RollYd"), MakeShared<FJsonValueNull>());
            if (!Result.bLanded)
                Engine->SetField(TEXT("CarryYd"), MakeShared<FJsonValueNull>());
        }
        return Engine;
    }
} // namespace

namespace ShotRecording
{
    FString ClubCode(ELaunchMonitorClub Club)
    {
        switch (Club)
        {
        case ELaunchMonitorClub::Driver:
            return TEXT("driver");
        case ELaunchMonitorClub::Wood3:
            return TEXT("3w");
        case ELaunchMonitorClub::Wood5:
            return TEXT("5w");
        case ELaunchMonitorClub::Wood7:
            return TEXT("7w");
        case ELaunchMonitorClub::Iron4:
            return TEXT("4i");
        case ELaunchMonitorClub::Iron5:
            return TEXT("5i");
        case ELaunchMonitorClub::Iron6:
            return TEXT("6i");
        case ELaunchMonitorClub::Iron7:
            return TEXT("7i");
        case ELaunchMonitorClub::Iron8:
            return TEXT("8i");
        case ELaunchMonitorClub::Iron9:
            return TEXT("9i");
        case ELaunchMonitorClub::PitchingWedge:
            return TEXT("pw");
        case ELaunchMonitorClub::GapWedge:
            return TEXT("gw");
        case ELaunchMonitorClub::SandWedge:
            return TEXT("sw");
        case ELaunchMonitorClub::LobWedge:
            return TEXT("lw");
        case ELaunchMonitorClub::Putter:
            return TEXT("putter");
        }
        return TEXT("other");
    }

    FString DefaultRootDir()
    {
        // UserDir is the Documents folder (Windows) or XDG documents dir (Linux), outside the install.
        return FPaths::Combine(FPlatformProcess::UserDir(), TEXT("UOpenGolf"), TEXT("Saved"), TEXT("ShotRecordings"));
    }

    FString SessionFolderName(const FDateTime& Local)
    {
        return FString::Printf(TEXT("session_%s"), *Local.ToString(TEXT("%Y%m%d-%H%M%S")));
    }

    FString ShotFileName(ELaunchMonitorClub Club, int32 Index)
    {
        return FString::Printf(TEXT("club_%s_shot_%d.json"), *ClubCode(Club), Index);
    }

    bool IsRecordable(const FLaunchMonitorShot& Shot, ELaunchMonitorConnectionState State)
    {
        return Shot.Source == RecordingDeviceSource && (State == ELaunchMonitorConnectionState::Connected || State == ELaunchMonitorConnectionState::Ready);
    }

    FString BuildShotJson(const FRecordedShot& Recorded, int32 Index)
    {
        const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("DeviceID"), Recorded.Shot.Source);
        Root->SetStringField(TEXT("Units"), TEXT("Yards"));
        Root->SetNumberField(TEXT("ShotNumber"), Index);
        Root->SetStringField(TEXT("APIVersion"), TEXT("1"));
        Root->SetObjectField(TEXT("BallData"), MakeRecordingBallData(Recorded.Shot));

        const bool bHasClubData = Recorded.ClubData.IsSet() && Recorded.ClubData->HasReading();
        if (bHasClubData)
        {
            Root->SetObjectField(TEXT("ClubData"), MakeRecordingClubData(*Recorded.ClubData));
        }

        const TSharedRef<FJsonObject> Options = MakeShared<FJsonObject>();
        Options->SetBoolField(TEXT("ContainsBallData"), true);
        Options->SetBoolField(TEXT("ContainsClubData"), bHasClubData);
        Root->SetObjectField(TEXT("ShotDataOptions"), Options);

        const TSharedRef<FJsonObject> Game = MakeShared<FJsonObject>();
        Game->SetStringField(TEXT("Club"), ClubCode(Recorded.SelectedClub));
        Game->SetStringField(TEXT("ClubName"), UOpenLaunchMonitorSubsystem::GetClubDisplayName(Recorded.SelectedClub).ToString());
        Game->SetBoolField(TEXT("LeftHanded"), Recorded.bLeftHanded);
        Game->SetNumberField(TEXT("DeviceShotNumber"), Recorded.Shot.ShotNumber);
        Game->SetStringField(TEXT("RecordedLocal"), Recorded.RecordedLocal.ToString(TEXT("%Y-%m-%dT%H:%M:%S")));
        if (!Recorded.Firmware.IsEmpty())
        {
            Game->SetStringField(TEXT("Firmware"), Recorded.Firmware);
        }
        if (Recorded.Engine.IsSet())
        {
            Game->SetObjectField(TEXT("Engine"), MakeRecordingEngineResult(*Recorded.Engine));
        }
        Root->SetObjectField(TEXT("UOpenGolf"), Game);

        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
        FJsonSerializer::Serialize(Root, Writer);
        return Json;
    }

    /** Moves Temp to Path only when Path does not exist yet. */
    static bool FinalizeWithoutReplacing(const FString& Temp, const FString& Path)
    {
#if PLATFORM_LINUX
        // POSIX rename() silently replaces the target, so link (which fails on an existing path) and then unlink.
        if (link(TCHAR_TO_UTF8(*Temp), TCHAR_TO_UTF8(*Path)) != 0)
        {
            return false;
        }
        unlink(TCHAR_TO_UTF8(*Temp));
        return true;
#else
        return IFileManager::Get().Move(*Path, *Temp, false, false, false, true);
#endif
    }

    bool WriteRecordingFile(const FString& Path, const FString& Json, FString& Error)
    {
        IFileManager& Files = IFileManager::Get();
        if (!Files.MakeDirectory(*FPaths::GetPath(Path), true))
        {
            Error = TEXT("Cannot create recording folder");
            return false;
        }
        const FString Temp = Path + TEXT(".tmp");
        if (!FFileHelper::SaveStringToFile(Json, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            Error = TEXT("Cannot write recording file");
            return false;
        }
        // Never replace an existing final record. Temp and destination share a filesystem.
        if (!FinalizeWithoutReplacing(Temp, Path))
        {
            Error = TEXT("Cannot finalize recording file");
            return false;
        }
        return true;
    }

    FShotRecorder::FShotRecorder(FString InRootDir, TFunction<double()> InClock, FWriter InWriter)
        : RootDir(InRootDir.IsEmpty() ? DefaultRootDir() : MoveTemp(InRootDir)),
          Clock(InClock ? MoveTemp(InClock) : TFunction<double()>([]
                                                  { return FPlatformTime::Seconds(); })),
          Writer(MoveTemp(InWriter))
    {
        if (!Writer)
        {
            Writer = WriteRecordingFile;
        }
    }

    void FShotRecorder::SetEnabled(bool bNewEnabled)
    {
        if (bNewEnabled == bEnabled)
            return;
        if (bNewEnabled)
        {
            CurrentSession = MakeShared<FSession>();
            SessionDir.Reset();
        }
        else
        {
            for (FPending& Item : Pending)
                Item.bFinalized = true;
            Process();
        }
        bEnabled = bNewEnabled;
    }

    void FShotRecorder::OnShot(const FLaunchMonitorShot& Shot, ELaunchMonitorConnectionState State,
        ELaunchMonitorClub Club, bool bLeftHanded, const FString& Firmware)
    {
        CancelSimulation();
        if (!bEnabled || !IsRecordable(Shot, State))
            return;
        if (CurrentSession->Directory.IsEmpty())
        {
            CurrentSession->Directory = FPaths::Combine(RootDir, SessionFolderName(FDateTime::Now()) + TEXT("-") +
                                                                     FGuid::NewGuid().ToString(EGuidFormats::Digits));
        }
        FPending& Item = Pending.AddDefaulted_GetRef();
        Item.Session = CurrentSession;
        Item.Index = ++CurrentSession->NextIndex;
        Item.ClubDeadline = Clock() + 3.0;
        Item.Record.Shot = Shot;
        Item.Record.SelectedClub = Club;
        Item.Record.bLeftHanded = bLeftHanded;
        Item.Record.Firmware = Firmware;
        Item.Record.RecordedLocal = FDateTime::Now();
    }

    void FShotRecorder::BindSimulation(int32 DeviceShot, int32 SimulationShot)
    {
        for (FPending& Item : Pending)
            if (!Item.bFinalized && Item.Record.Shot.ShotNumber == DeviceShot)
                Item.SimulationShot = SimulationShot;
    }

    void FShotRecorder::CancelSimulation()
    {
        for (FPending& Item : Pending)
        {
            Item.bSimulationDone = true;
            Item.SimulationShot = INDEX_NONE;
        }
        Process();
    }

    void FShotRecorder::OnClubData(const FLaunchMonitorClubData& ClubData)
    {
        for (FPending& Item : Pending)
            if (!Item.bFinalized && Item.Record.Shot.ShotNumber == ClubData.ShotNumber)
                Item.Record.ClubData = ClubData;
        Process();
    }

    void FShotRecorder::OnBallAtRest(const FOpenFairwayShotResult& Result)
    {
        for (FPending& Item : Pending)
        {
            if (!Item.bFinalized && !Item.bSimulationDone && Item.SimulationShot == Result.ShotNumber)
            {
                Item.Record.Engine = Result;
                Item.bSimulationDone = true;
            }
        }
        Process();
    }

    int32 FShotRecorder::GetFailedSaveCount() const
    {
        if (LastError.IsEmpty())
            return 0;
        int32 Count = 0;
        for (const FPending& Item : Pending)
            if (Item.bFinalized)
                ++Count;
        return Count;
    }

    void FShotRecorder::Tick()
    {
        Process();
    }

    void FShotRecorder::Process(bool bForceRetry)
    {
        const double Now = Clock();
        for (FPending& Item : Pending)
            if (Item.bSimulationDone && (Item.Record.ClubData.IsSet() || Now >= Item.ClubDeadline))
                Item.bFinalized = true;
        if (!bForceRetry && Now < NextRetry)
            return;
        for (int32 I = 0; I < Pending.Num();)
        {
            FPending& Item = Pending[I];
            if (!Item.bFinalized)
            {
                ++I;
                continue;
            }
            const FString Path = FPaths::Combine(Item.Session->Directory, ShotFileName(Item.Record.SelectedClub, Item.Index));
            FString Error;
            if (!Writer(Path, BuildShotJson(Item.Record, Item.Index), Error))
            {
                if (Error.IsEmpty())
                    Error = TEXT("Cannot save recording");
                if (LastError != Error)
                    UE_LOG(LogUOpenGolfRecording, Warning, TEXT("%s: %s. Retained in memory; retrying during Play."), *Error, *Path);
                LastError = Error;
                NextRetry = Now + 1.0;
                return;
            }
            ++Item.Session->Saved;
            if (Item.Session == CurrentSession)
                SessionDir = Item.Session->Directory;
            UE_LOG(LogUOpenGolfRecording, Log, TEXT("Recorded shot %s"), *Path);
            Pending.RemoveAt(I);
        }
        LastError.Reset();
        NextRetry = 0;
    }

    void FShotRecorder::Flush()
    {
        for (FPending& Item : Pending)
            Item.bFinalized = true;
        Process(true);
    }

    void FShotRecorder::Shutdown()
    {
        Flush();
        if (!Pending.IsEmpty())
            UE_LOG(LogUOpenGolfRecording, Error, TEXT("Play ended with %d unsaved recordings for %s. In-memory records cannot survive shutdown."),
                Pending.Num(), *RootDir);
    }
} // namespace ShotRecording
