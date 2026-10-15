// SPDX-License-Identifier: MIT
#include "ShotRecording.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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
        // GSPro's field for a device-reported carry; the Square does not report one.
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

    FShotRecorder::FShotRecorder(FString InRootDir)
        : RootDir(InRootDir.IsEmpty() ? DefaultRootDir() : MoveTemp(InRootDir))
    {
    }

    void FShotRecorder::SetEnabled(bool bNewEnabled)
    {
        if (bNewEnabled == bEnabled)
        {
            return;
        }
        if (!bNewEnabled)
        {
            Flush();
        }
        else
        {
            // A new session folder is made on the first shot, so toggling without shooting leaves nothing behind.
            SessionDir.Reset();
            ShotCount = 0;
        }
        bEnabled = bNewEnabled;
    }

    void FShotRecorder::OnShot(const FLaunchMonitorShot& Shot, ELaunchMonitorConnectionState State, ELaunchMonitorClub Club, bool bLeftHanded,
        const FString& Firmware)
    {
        Flush();
        if (!bEnabled || !IsRecordable(Shot, State))
        {
            return;
        }
        FRecordedShot& Recorded = Pending.Emplace();
        Recorded.Shot = Shot;
        Recorded.SelectedClub = Club;
        Recorded.bLeftHanded = bLeftHanded;
        Recorded.Firmware = Firmware;
        Recorded.RecordedLocal = FDateTime::Now();
    }

    void FShotRecorder::OnClubData(const FLaunchMonitorClubData& ClubData)
    {
        if (Pending.IsSet() && Pending->Shot.ShotNumber == ClubData.ShotNumber)
        {
            Pending->ClubData = ClubData;
        }
    }

    void FShotRecorder::OnBallAtRest(const FOpenFairwayShotResult& Result)
    {
        if (Pending.IsSet())
        {
            Pending->Engine = Result;
            Flush();
        }
    }

    void FShotRecorder::Flush()
    {
        if (!Pending.IsSet())
        {
            return;
        }
        const FRecordedShot Recorded = MoveTemp(Pending.GetValue());
        Pending.Reset();

        IFileManager& FileManager = IFileManager::Get();
        if (SessionDir.IsEmpty())
        {
            // Two sessions in the same second get -2, -3... rather than sharing a folder.
            const FString Base = FPaths::Combine(RootDir, SessionFolderName(Recorded.RecordedLocal));
            FString Candidate = Base;
            for (int32 Suffix = 2; FileManager.DirectoryExists(*Candidate); ++Suffix)
            {
                Candidate = FString::Printf(TEXT("%s-%d"), *Base, Suffix);
            }
            if (!FileManager.MakeDirectory(*Candidate, true))
            {
                UE_LOG(LogUOpenGolfRecording, Error, TEXT("Could not create shot recording folder %s"), *Candidate);
                return;
            }
            SessionDir = Candidate;
        }

        const int32 Index = ShotCount + 1;
        const FString Path = FPaths::Combine(SessionDir, ShotFileName(Recorded.SelectedClub, Index));
        if (!FFileHelper::SaveStringToFile(BuildShotJson(Recorded, Index), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            UE_LOG(LogUOpenGolfRecording, Error, TEXT("Could not write recorded shot %s"), *Path);
            return;
        }
        ShotCount = Index;
        UE_LOG(LogUOpenGolfRecording, Log, TEXT("Recorded shot %s"), *FPaths::ConvertRelativePathToFull(Path));
    }
} // namespace ShotRecording
