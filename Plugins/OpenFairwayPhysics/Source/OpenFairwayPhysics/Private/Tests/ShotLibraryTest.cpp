// SPDX-License-Identifier: MIT
// Replays the recorded launch-monitor shots in Data/ShotLibrary through the headless simulator
// and writes the engine's results for Scripts/flightscope_compare.py, which reports them
// against the FlightScope values stored with each shot. Differences are reported, not asserted.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

namespace
{
    struct FShotLibraryShot
    {
        FString Session;
        FString ShotName;
        FLaunchData Launch;
    };

    struct FShotLibraryProfile
    {
        const TCHAR* Name;
        FBallPhysicsProfile Profile;
    };

    // Launch-monitor inputs of every fs.csv row, in the order ParseShotLibraryCsv assigns them.
    const TCHAR* const ShotLibraryInputColumns[] = {TEXT("speed_mph"), TEXT("vla_deg"), TEXT("hla_deg"), TEXT("backspin_rpm"),
        TEXT("sidespin_rpm"), TEXT("total_spin_rpm"), TEXT("spin_axis_deg")};
    constexpr int32 ShotLibraryInputCount = UE_ARRAY_COUNT(ShotLibraryInputColumns);

    TArray<FString> SplitShotLibraryLine(const FString& Line)
    {
        TArray<FString> Cells;
        Line.ParseIntoArray(Cells, TEXT(","), false);
        for (FString& Cell : Cells)
        {
            Cell.TrimStartAndEndInline();
        }
        return Cells;
    }

    /** Reads one session's fs.csv. Every row needs a shot name and the seven numeric launch inputs. */
    bool ParseShotLibraryCsv(const FString& Path, const FString& Session, TArray<FShotLibraryShot>& OutShots, FString& OutError)
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
        }
        return true;
    }

    /**
     * Profiles replayed for every shot: current and historical (the engine before the spin-decay
     * and calibration-interpolation changes) first, then candidate changes under evaluation.
     */
    TArray<FShotLibraryProfile> MakeShotLibraryProfiles()
    {
        TArray<FShotLibraryProfile> Profiles;
        Profiles.Add({TEXT("current"), FBallPhysicsProfile()});
        Profiles.Add({TEXT("historical"), FBallPhysicsProfile::HistoricalReference()});
        return Profiles;
    }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayShotLibraryTest, "UOpenGolf.Physics.ShotLibrary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayShotLibraryTest::RunTest(const FString& Parameters)
{
    // Each subdirectory with an fs.csv is a session named after its relative path.
    const FString LibraryDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Data/ShotLibrary"));
    TArray<FString> Files;
    IFileManager::Get().FindFilesRecursive(Files, *LibraryDir, TEXT("fs.csv"), true, false);
    Files.Sort();
    if (!TestTrue(FString::Printf(TEXT("Shot library files found in %s"), *LibraryDir), Files.Num() > 0))
    {
        return false;
    }

    TArray<FShotLibraryShot> Shots;
    for (const FString& File : Files)
    {
        FString Relative = File;
        FPaths::MakePathRelativeTo(Relative, *(LibraryDir + TEXT("/")));
        FString Session = FPaths::GetPath(Relative);
        if (Session.IsEmpty())
        {
            Session = TEXT(".");
        }
        FString Error;
        if (!ParseShotLibraryCsv(File, Session, Shots, Error))
        {
            AddError(FString::Printf(TEXT("%s: %s"), *File, *Error));
        }
    }
    TSet<FString> Keys;
    for (const FShotLibraryShot& Shot : Shots)
    {
        bool bDuplicate = false;
        Keys.Add(Shot.Session / Shot.ShotName, &bDuplicate);
        TestFalse(FString::Printf(TEXT("%s/%s is unique"), *Shot.Session, *Shot.ShotName), bDuplicate);
    }

    // Default air (75 F, sea level) and fairway, as in the game.
    const TArray<FShotLibraryProfile> Profiles = MakeShotLibraryProfiles();
    FString Csv = TEXT("session,shot_name,profile,carry_yd,total_yd,rollout_yd,apex_ft,hang_s,lateral_yd,at_rest,landing_angle_deg,landing_spin_rpm\n");
    int32 ResultCount = 0;
    for (const FShotLibraryShot& Shot : Shots)
    {
        for (const FShotLibraryProfile& Entry : Profiles)
        {
            const FShotSimulationResult Result = ShotSimulator::Simulate(Shot.Launch, ESurface::Fairway, Entry.Profile);
            const FString Label = FString::Printf(TEXT("%s/%s (%s)"), *Shot.Session, *Shot.ShotName, Entry.Name);
            if (!TestTrue(Label + TEXT(" valid"), Result.bValid))
            {
                continue;
            }
            TestTrue(Label + TEXT(" at rest"), Result.bAtRest);
            TestTrue(Label + TEXT(" finite"), FMath::IsFinite(Result.CarryM) && FMath::IsFinite(Result.TotalM) && FMath::IsFinite(Result.ApexM) &&
                                                  FMath::IsFinite(Result.HangTimeS) && FMath::IsFinite(Result.LateralM));
            Csv += FString::Printf(TEXT("%s,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%s,%.4f,%.1f\n"), *Shot.Session, *Shot.ShotName, Entry.Name,
                Result.CarryM * YardsPerMeter, Result.TotalM * YardsPerMeter, (Result.TotalM - Result.CarryM) * YardsPerMeter,
                Result.ApexM * FeetPerMeter, Result.HangTimeS, Result.LateralM * YardsPerMeter, Result.bAtRest ? TEXT("true") : TEXT("false"),
                Result.LandingAngleDeg, Result.LandingSpinRpm);
            ++ResultCount;
        }
    }

    const FString ResultsPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ShotLibrary/results.csv"));
    TestTrue(TEXT("Results written"), FFileHelper::SaveStringToFile(Csv, *ResultsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    AddInfo(FString::Printf(TEXT("Replayed %d shots from %d files (%d results) into %s. Compare with: python Scripts/flightscope_compare.py --skip-run"),
        Shots.Num(), Files.Num(), ResultCount, *ResultsPath));
    return true;
}

#endif
