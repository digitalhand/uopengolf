// SPDX-License-Identifier: MIT
// The shot library: every <session>/shots.csv in the repository's recordings (Assets/Data/ShotLibrary) and, when present,
// in the local reference data folder (UOPENGOLF_DATA, by default ~/data/uopengolf; see Assets/Data/README.md). Shared by
// UOpenGolf.Physics.ShotLibrary and UOpenGolf.Calibration.Rollout; Assets/Scripts/shot_library.py reads the same files.
#pragma once

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenFairway/ShotSimulator.h"

namespace OpenFairwayShotLibrary
{
    using namespace OpenFairway;

    struct FShotLibraryShot
    {
        FString Session;
        FString ShotName;
        FLaunchData Launch;
        /** The reference's air; 75 F, dry, sea level unless the session gives temperature_f, humidity or pressure_psi. */
        FAirConditions Air;
        float ReferenceCarryYd = -1.0f;
        float ReferenceTotalYd = -1.0f;
    };

    /** Third-party reference data lives outside the repository: UOPENGOLF_DATA, by default ~/data/uopengolf. */
    inline FString LocalDataDir()
    {
        FString Dir = FPlatformMisc::GetEnvironmentVariable(TEXT("UOPENGOLF_DATA"));
        if (Dir.IsEmpty())
        {
            FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
            if (Home.IsEmpty())
            {
                Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
            }
            Dir = Home / TEXT("data/uopengolf");
        }
        return FPaths::ConvertRelativePathToFull(Dir);
    }

    inline FString RepoShotLibraryDir()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Assets/Data/ShotLibrary"));
    }

    inline FString LocalShotLibraryDir()
    {
        return LocalDataDir() / TEXT("shot_library");
    }

    // Launch-monitor inputs of every shots.csv row, in the order ParseShotLibraryCsv assigns them.
    inline const TCHAR* const ShotLibraryInputColumns[] = {TEXT("speed_mph"), TEXT("vla_deg"), TEXT("hla_deg"), TEXT("backspin_rpm"),
        TEXT("sidespin_rpm"), TEXT("total_spin_rpm"), TEXT("spin_axis_deg")};
    constexpr int32 ShotLibraryInputCount = UE_ARRAY_COUNT(ShotLibraryInputColumns);

    inline TArray<FString> SplitShotLibraryLine(const FString& Line)
    {
        TArray<FString> Cells;
        Line.ParseIntoArray(Cells, TEXT(","), false);
        for (FString& Cell : Cells)
        {
            Cell.TrimStartAndEndInline();
        }
        return Cells;
    }

    /** Reads one session's shots.csv. Every row needs a shot name and the seven numeric launch inputs. */
    inline bool ParseShotLibraryCsv(const FString& Path, const FString& Session, TArray<FShotLibraryShot>& OutShots, FString& OutError)
    {
        TArray<FString> Lines;
        if (!FFileHelper::LoadFileToStringArray(Lines, *Path) || Lines.IsEmpty())
        {
            OutError = TEXT("unreadable or empty");
            return false;
        }
        const TArray<FString> Header = SplitShotLibraryLine(Lines[0]);
        const int32 NameColumn = Header.IndexOfByKey(FString(TEXT("shot_name")));
        int32 InputColumns[ShotLibraryInputCount];
        for (int32 Input = 0; Input < ShotLibraryInputCount; ++Input)
        {
            InputColumns[Input] = Header.IndexOfByKey(FString(ShotLibraryInputColumns[Input]));
            if (InputColumns[Input] == INDEX_NONE)
            {
                OutError = FString::Printf(TEXT("missing column %s"), ShotLibraryInputColumns[Input]);
                return false;
            }
        }
        if (NameColumn == INDEX_NONE)
        {
            OutError = TEXT("missing column shot_name");
            return false;
        }

        for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
        {
            if (Lines[LineIndex].TrimStartAndEnd().IsEmpty())
            {
                continue;
            }
            const TArray<FString> Cells = SplitShotLibraryLine(Lines[LineIndex]);
            if (!Cells.IsValidIndex(NameColumn) || Cells[NameColumn].IsEmpty())
            {
                OutError = FString::Printf(TEXT("line %d has no shot_name"), LineIndex + 1);
                return false;
            }
            float Inputs[ShotLibraryInputCount];
            for (int32 Input = 0; Input < ShotLibraryInputCount; ++Input)
            {
                const int32 Column = InputColumns[Input];
                if (!Cells.IsValidIndex(Column) || Cells[Column].IsEmpty() || !FCString::IsNumeric(*Cells[Column]))
                {
                    OutError = FString::Printf(TEXT("line %d: %s is not a number"), LineIndex + 1, ShotLibraryInputColumns[Input]);
                    return false;
                }
                Inputs[Input] = FCString::Atof(*Cells[Column]);
            }
            FShotLibraryShot& Shot = OutShots.AddDefaulted_GetRef();
            Shot.Session = Session;
            Shot.ShotName = Cells[NameColumn];
            Shot.Launch.SpeedMph = Inputs[0];
            Shot.Launch.VerticalLaunchDeg = Inputs[1];
            Shot.Launch.HorizontalLaunchDeg = Inputs[2];
            Shot.Launch.BackSpinRpm = Inputs[3];
            Shot.Launch.SideSpinRpm = Inputs[4];
            Shot.Launch.TotalSpinRpm = Inputs[5];
            Shot.Launch.SpinAxisDeg = Inputs[6];
            const int32 CarryColumn = Header.IndexOfByKey(FString(TEXT("carry_yd")));
            const int32 TotalColumn = Header.IndexOfByKey(FString(TEXT("total_yd")));
            if (Cells.IsValidIndex(CarryColumn) && !Cells[CarryColumn].IsEmpty())
                Shot.ReferenceCarryYd = FCString::Atof(*Cells[CarryColumn]);
            if (Cells.IsValidIndex(TotalColumn) && !Cells[TotalColumn].IsEmpty())
                Shot.ReferenceTotalYd = FCString::Atof(*Cells[TotalColumn]);
            // Optional air the reference was computed in.
            auto Optional = [&](const TCHAR* Name, float& Out, float Scale)
            {
                const int32 Column = Header.IndexOfByKey(FString(Name));
                if (Cells.IsValidIndex(Column) && !Cells[Column].IsEmpty() && FCString::IsNumeric(*Cells[Column]))
                    Out = FCString::Atof(*Cells[Column]) * Scale;
            };
            Optional(TEXT("temperature_f"), Shot.Air.TemperatureF, 1.0f);
            Optional(TEXT("humidity"), Shot.Air.RelativeHumidity, 1.0f);
            Optional(TEXT("pressure_psi"), Shot.Air.PressurePa, PaPerPsi);
        }
        return true;
    }

    /** Every <session>/shots.csv under Root; a session is named after its path relative to Root. */
    inline int32 LoadShotLibrary(const FString& Root, TArray<FShotLibraryShot>& OutShots, TArray<FString>& OutErrors)
    {
        TArray<FString> Files;
        IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("shots.csv"), true, false);
        Files.Sort();
        for (const FString& File : Files)
        {
            FString Relative = File;
            FPaths::MakePathRelativeTo(Relative, *(Root + TEXT("/")));
            FString Session = FPaths::GetPath(Relative);
            if (Session.IsEmpty())
            {
                Session = TEXT(".");
            }
            FString Error;
            if (!ParseShotLibraryCsv(File, Session, OutShots, Error))
            {
                OutErrors.Add(FString::Printf(TEXT("%s: %s"), *File, *Error));
            }
        }
        return Files.Num();
    }

} // namespace OpenFairwayShotLibrary

#endif
