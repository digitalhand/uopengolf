// SPDX-License-Identifier: MIT
// Replays the shot library through the headless simulator and writes the engine's results for
// Assets/Scripts/shot_compare.py, which reports them against the reference results stored with each shot.
// Sessions come from uopengolf's own recordings in Assets/Data/ShotLibrary and, when present, from the local
// reference data folder (see Assets/Data/README.md). Differences are reported, not asserted.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
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
        /** The reference's air; 75 F, dry, sea level unless the session gives temperature_f, humidity or pressure_psi. */
        FAirConditions Air;
        float ReferenceCarryYd = -1.0f;
        float ReferenceTotalYd = -1.0f;
    };

    /** Third-party reference data lives outside the repository: UOPENGOLF_DATA, by default ~/data/uopengolf. */
    FString LocalDataDir()
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

    FString RepoShotLibraryDir()
    {
        return FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("Assets/Data/ShotLibrary"));
    }

    FString LocalShotLibraryDir()
    {
        return LocalDataDir() / TEXT("shot_library");
    }

    struct FShotLibraryProfile
    {
        const TCHAR* Name;
        FBallPhysicsProfile Profile;
    };

    // Launch-monitor inputs of every shots.csv row, in the order ParseShotLibraryCsv assigns them.
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

    /** Reads one session's shots.csv. Every row needs a shot name and the seven numeric launch inputs. */
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
            Optional(TEXT("pressure_psi"), Shot.Air.PressurePa, 6894.757f);
        }
        return true;
    }

    /** Every <session>/shots.csv under Root; a session is named after its path relative to Root. */
    int32 LoadShotLibrary(const FString& Root, TArray<FShotLibraryShot>& OutShots, TArray<FString>& OutErrors)
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

    /**
     * Profiles replayed for every shot: current and historical (the pinned ogolf-physics port, before the
     * flight, green, sand and turf changes) first, then comparisons and candidate changes under evaluation.
     * legacy_calibrated is the gameplay flight before the measured aerodynamics; prov1_research uses Pro V1
     * drag measured near 2250 rpm only. See Docs/CarryAudit.md.
     */
    TArray<FShotLibraryProfile> MakeShotLibraryProfiles()
    {
        TArray<FShotLibraryProfile> Profiles;
        Profiles.Add({TEXT("current"), FBallPhysicsProfile()});
        Profiles.Add({TEXT("historical"), FBallPhysicsProfile::HistoricalReference()});
        Profiles.Add({TEXT("legacy_calibrated"), FBallPhysicsProfile::LegacyCalibrated()});
        Profiles.Add({TEXT("prov1_research"), FBallPhysicsProfile::ProV1Reference()});
        return Profiles;
    }

    const TCHAR* FlightModelName(EFlightModel Model)
    {
        switch (Model)
        {
        case EFlightModel::ProV1ResearchV1:
            return TEXT("prov1_research_v1");
        case EFlightModel::SpinRatioV1:
            return TEXT("spin_ratio_v1");
        default:
            return TEXT("legacy");
        }
    }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayShotLibraryTest, "UOpenGolf.Physics.ShotLibrary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayShotLibraryTest::RunTest(const FString& Parameters)
{
    // Each subdirectory with a shots.csv is a session named after its path relative to its library.
    TArray<FShotLibraryShot> Shots;
    TArray<FString> Errors;
    int32 FileCount = LoadShotLibrary(RepoShotLibraryDir(), Shots, Errors);
    if (!TestTrue(FString::Printf(TEXT("Shot library files found in %s"), *RepoShotLibraryDir()), FileCount > 0))
    {
        return false;
    }
    if (IFileManager::Get().DirectoryExists(*LocalShotLibraryDir()))
    {
        FileCount += LoadShotLibrary(LocalShotLibraryDir(), Shots, Errors);
    }
    else
    {
        AddInfo(FString::Printf(TEXT("No local reference data in %s; replaying the repository's recordings only."), *LocalShotLibraryDir()));
    }
    for (const FString& Error : Errors)
    {
        AddError(Error);
    }
    TSet<FString> Keys;
    for (const FShotLibraryShot& Shot : Shots)
    {
        bool bDuplicate = false;
        Keys.Add(Shot.Session / Shot.ShotName, &bDuplicate);
        TestFalse(FString::Printf(TEXT("%s/%s is unique"), *Shot.Session, *Shot.ShotName), bDuplicate);
    }

    // Fairway, in each shot's reference air (the game's default 75 F, dry, sea level unless the session says otherwise).
    const TArray<FShotLibraryProfile> Profiles = MakeShotLibraryProfiles();
    FString Csv = TEXT("session,shot_name,profile,carry_yd,total_yd,rollout_yd,apex_ft,hang_s,lateral_yd,at_rest,landing_angle_deg,landing_spin_rpm,impact_in_mps,impact_out_mps,bounce_count,ground_distance_m,termination,flight_model,temperature_f,altitude_ft,humidity,wind_mph,air_density,landing_speed_mps,outside_coverage_s,min_re,max_re,min_spin_ratio,max_spin_ratio\n");
    int32 ResultCount = 0;
    for (const FShotLibraryShot& Shot : Shots)
    {
        for (const FShotLibraryProfile& Entry : Profiles)
        {
            FShotSimulationOptions Options;
            FString TraceCsv = TEXT("time_s,dt,phase,surface,x,y,z,vx,vy,vz,wx,wy,wz,nx,ny,nz,slip_mps,energy_j\n");
            if (&Entry == &Profiles[0] && (Shot.ShotName.Contains(TEXT("gw_shot_1")) || Shot.ShotName.Contains(TEXT("9i_shot_3")) ||
                Shot.ShotName.Contains(TEXT("7i_shot_4")) || Shot.ShotName == TEXT("6iron_10") || Shot.ShotName == TEXT("3hybrid_74")))
            {
                Options.Trace = [&TraceCsv](const FPhysicsTraceSample& S)
                {
                    TraceCsv += FString::Printf(TEXT("%.6f,%.6f,%s,%s,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%g\n"),
                        S.TimeS, S.Dt, *S.Phase, SurfaceCatalog::GetName(S.Surface), S.State.Position.X, S.State.Position.Y, S.State.Position.Z,
                        S.State.Velocity.X, S.State.Velocity.Y, S.State.Velocity.Z, S.State.Spin.X, S.State.Spin.Y, S.State.Spin.Z,
                        S.Normal.X, S.Normal.Y, S.Normal.Z, S.SlipMps, S.EnergyJ);
                };
            }
            const FShotSimulationResult Result = ShotSimulator::Simulate(Shot.Launch, ESurface::Fairway, Entry.Profile, Shot.Air,
                &Entry == &Profiles[0] ? ShotSimulator::DefaultMaxTimeSeconds : 30.0f, FVector3f(0, 1, 0), Options);
            if (Options.Trace)
            {
                const FString Directory = FPaths::ProjectSavedDir() / TEXT("ShotLibrary/Traces") / Shot.Session;
                IFileManager::Get().MakeDirectory(*Directory, true);
                TestTrue(TEXT("Trace saved"), FFileHelper::SaveStringToFile(TraceCsv, *(Directory / (Shot.ShotName + TEXT(".csv"))),
                    FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
            }
            const FString Label = FString::Printf(TEXT("%s/%s (%s)"), *Shot.Session, *Shot.ShotName, Entry.Name);
            if (!TestTrue(Label + TEXT(" valid"), Result.bValid))
            {
                continue;
            }
            if (Entry.Profile.Flight.bUseRungeKutta4)
            {
                FShotSimulationOptions Fine;
                Fine.Dt = 1.0f / 960.0f;
                const auto Reference = ShotSimulator::Simulate(Shot.Launch, ESurface::Fairway, Entry.Profile, Shot.Air,
                    ShotSimulator::DefaultMaxTimeSeconds, FVector3f(0, 1, 0), Fine);
                TestTrue(Label + TEXT(" RK4 rests"), Result.bAtRest);
                TestEqual(Label + TEXT(" carry converges within 0.1 yd"), Result.CarryM * YardsPerMeter,
                    Reference.CarryM * YardsPerMeter, 0.1f);
            }
            // Only the gameplay profile must stop in time: the historical turf runs a ball struck along the
            // ground for hundreds of yards, which at_rest=false in the results reports.
            if (&Entry == &Profiles[0])
            {
                TestTrue(Label + TEXT(" at rest"), Result.bAtRest);
            }
            else if (!Result.bAtRest)
            {
                AddInfo(Label + TEXT(" still moving at the time limit"));
            }
            TestTrue(Label + TEXT(" finite"), FMath::IsFinite(Result.CarryM) && FMath::IsFinite(Result.TotalM) && FMath::IsFinite(Result.ApexM) &&
                                                  FMath::IsFinite(Result.HangTimeS) && FMath::IsFinite(Result.LateralM));
            Csv += FString::Printf(TEXT("%s,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%s,%.4f,%.1f,%.4f,%.4f,%d,%.4f,%s,%s,%.1f,%.0f,%.2f,%.1f,%.6f,%.4f,%.4f,%.1f,%.1f,%.5f,%.5f\n"), *Shot.Session, *Shot.ShotName, Entry.Name,
                Result.CarryM * YardsPerMeter, Result.TotalM * YardsPerMeter, (Result.TotalM - Result.CarryM) * YardsPerMeter,
                Result.ApexM * FeetPerMeter, Result.HangTimeS, Result.LateralM * YardsPerMeter, Result.bAtRest ? TEXT("true") : TEXT("false"),
                Result.LandingAngleDeg, Result.LandingSpinRpm, Result.FirstImpactTangentInMps, Result.FirstImpactTangentOutMps,
                Result.BounceCount, Result.GroundDistanceM, Result.bAtRest ? TEXT("Rest") : TEXT("TimeLimit"),
                FlightModelName(Entry.Profile.Flight.Model), Shot.Air.TemperatureF, Shot.Air.AltitudeFt, Shot.Air.RelativeHumidity, Shot.Air.WindSpeedMph,
                Shot.Air.Density(), Result.LandingSpeedMps, FMath::Min(Result.OutsideMeasuredCoverageS, Result.HangTimeS),
                Result.MinFlightRe == MAX_flt ? 0 : Result.MinFlightRe, Result.MaxFlightRe,
                Result.MinFlightSpinRatio == MAX_flt ? 0 : Result.MinFlightSpinRatio, Result.MaxFlightSpinRatio);
            ++ResultCount;
        }
    }

    const FString ResultsPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("ShotLibrary/results.csv"));
    TestTrue(TEXT("Results written"), FFileHelper::SaveStringToFile(Csv, *ResultsPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
    AddInfo(FString::Printf(TEXT("Replayed %d shots from %d files (%d results) into %s. Compare with: python Assets/Scripts/shot_compare.py --skip-run"),
        Shots.Num(), FileCount, ResultCount, *ResultsPath));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotLabelIndependenceTest, "UOpenGolf.Physics.ClubLabelIndependence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// The reported club is an unverified label: the same launch under another club name must play identically.
bool FShotLabelIndependenceTest::RunTest(const FString& Parameters)
{
    const FString Path = FPaths::ProjectSavedDir() / TEXT("ShotLibrary/metadata_test.csv");
    const FString Header = TEXT("shot_name,reported_club,speed_mph,vla_deg,hla_deg,backspin_rpm,sidespin_rpm,total_spin_rpm,spin_axis_deg\n");
    TArray<FShotLibraryShot> Hybrid;
    TArray<FShotLibraryShot> Wedge;
    FString Error;
    FFileHelper::SaveStringToFile(Header + TEXT("example,3 Hybrid,109.3,16.7,-3.3,5072,0,5072,0\n"), *Path);
    TestTrue(TEXT("Parse original metadata"), ParseShotLibraryCsv(Path, TEXT("metadata"), Hybrid, Error));
    FFileHelper::SaveStringToFile(Header + TEXT("example,Pitching Wedge,109.3,16.7,-3.3,5072,0,5072,0\n"), *Path);
    TestTrue(TEXT("Parse changed metadata"), ParseShotLibraryCsv(Path, TEXT("metadata"), Wedge, Error));
    if (!TestTrue(TEXT("One shot each"), Hybrid.Num() == 1 && Wedge.Num() == 1))
    {
        return false;
    }
    const FShotSimulationResult First = ShotSimulator::Simulate(Hybrid[0].Launch);
    const FShotSimulationResult Second = ShotSimulator::Simulate(Wedge[0].Launch);
    TestEqual(TEXT("Club labels cannot change carry"), First.CarryM, Second.CarryM);
    TestEqual(TEXT("Club labels cannot change total"), First.TotalM, Second.TotalM);
    TestEqual(TEXT("Club labels cannot change apex"), First.ApexM, Second.ApexM);
    return true;
}

namespace RolloutCalibrationDetail
{
    // The fitted turf values, in the order the search perturbs them.
    enum EParameter
    {
        AngleLoss,
        SpinLoss,
        Rolling,
        Viscosity,
        FormDrag,
        ParameterCount
    };
    const TCHAR* const ParameterNames[ParameterCount] = {TEXT("angle_loss"), TEXT("spin_loss"), TEXT("rolling"), TEXT("viscosity"), TEXT("form_drag")};
    constexpr float Lower[ParameterCount] = {0.05f, 0.0f, 0.02f, 0.001f, 0.0f};
    constexpr float Upper[ParameterCount] = {3.0f, 1.5f, 0.20f, 0.035f, 0.035f};

    // The struck-along-the-ground grass drag scales (stage 2): a shot launched into or along the ground keeps these
    // shares of the turf's linear and speed-squared grass drag for its whole roll.
    enum EStruckParameter
    {
        StruckViscosity,
        StruckFormDrag,
        StruckParameterCount
    };
    constexpr float StruckLower[StruckParameterCount] = {0.02f, 0.0f};
    constexpr float StruckUpper[StruckParameterCount] = {1.0f, 3.0f};

    // Launch classes from the launch values; the reported club is never read.
    constexpr int32 ClassCount = 5;
    const TCHAR* const ClassNames[ClassCount] = {TEXT("ground"), TEXT("low"), TEXT("mid_low_spin"), TEXT("mid_high_spin"), TEXT("high")};
    int32 LaunchClass(const FLaunchData& Launch)
    {
        const float Vla = Launch.VerticalLaunchDeg;
        return Vla < 2.5f ? 0 : Vla < 9.0f ? 1 : Vla >= 20.0f ? 4 : ShotSetup::ParseSpin(Launch).TotalSpinRpm < 4000.0f ? 2 : 3;
    }

    /** Launched into or along flat ground: the shot contacts at t = 0 and never flies (the export's carry-0 rows). */
    bool IsStruck(const FLaunchData& Launch)
    {
        return Launch.VerticalLaunchDeg <= 0.0f;
    }

    // The previous recalibration (first-impact retention 0.95, its 0.75-5 m/s ramp, bite 0.35; 2026-11-16),
    // for the acceptance report only: roll error per class and over the airborne classes, yd.
    constexpr float RetentionModelClassMae[ClassCount] = {22.17f, 7.55f, 2.29f, 1.11f, 0.58f};
    constexpr float RetentionModelAirborneMae = 1.96f;
    // A plausible fairway is slower than the green but not like rough: championship fairways measure about 9.5 ft.
    // The lower bound is a judgment, not a measurement; without it the fit buys ground-launched distance with
    // a fairway that stops a stimpmeter ball in 3-4 ft.
    constexpr float MinFairwayStimpFeet = 6.0f;
    // UOpenGolf.Physics.RolloutRegression allows 15% per 1 deg and 3% per 0.1 deg of launch; the search keeps a
    // margin under both so rounded catalog values still pass.
    constexpr float SearchStepLimit = 0.14f;
    constexpr float SearchFineStepLimit = 0.028f;
    // UOpenGolf.Physics.SlopeRollout (after ogolf-physics) requires rough to hold a resting ball on steeper slopes than
    // a green. A slope's hold angle is atan(rolling friction * slope resistance ratio), and rough keeps its ported
    // ratio to fairway's rolling friction, so the search keeps rough's hold at least this far above the green's.
    constexpr float MinRoughOverGreenHoldDeg = 1.0f;

    float HoldAngleDeg(float RollingFriction)
    {
        return FMath::RadiansToDegrees(FMath::Atan(RollingFriction * FRolloutProfile().SlopeResistanceRatio));
    }

    /** Rough's rolling friction for a candidate fairway value: the ported rough-to-fairway ratio. */
    float RoughRollingFriction(float FairwayRollingFriction)
    {
        return FairwayRollingFriction * SurfaceCatalog::Get(ESurface::Rough, false).RollingFriction /
               SurfaceCatalog::Get(ESurface::Fairway, false).RollingFriction;
    }

    struct FScore
    {
        bool bAllRest = true;
        int32 Count[ClassCount] = {};
        float Mae[ClassCount] = {};
        float Bias[ClassCount] = {};
        /** The ground class without the struck shots: the near-ground launches that fly, stage 1's ground term. */
        int32 FlownGroundCount = 0;
        float FlownGroundMae = 0.0f;
        float FlownGroundBias = 0.0f;
        /** The shots struck into or along the ground, stage 2's target. */
        int32 StruckCount = 0;
        float StruckMae = 0.0f;
        float StruckBias = 0.0f;
        float AirborneMae = 0.0f;
        float RollMae = 0.0f;
        float TotalMae = 0.0f;
        float WorstRoll = 0.0f;
        float WorstStep = 0.0f;
        float WorstFineStep = 0.0f;
        float StimpFeet = 0.0f;
        float GreenStimpFeet = 0.0f;
        float RoughHoldDeg = 0.0f;
        float GreenHoldDeg = 0.0f;
        /**
         * Search constraints: every shot rests, totals follow launch angle, a plausible fairway speed, rough holds
         * steeper slopes than a green, flown ground bias bounded.
         */
        bool bFeasible = false;
        /** Stage 1's ranking: airborne roll error and a little of the flown ground class, plus any constraint violation. */
        float Score = TNumericLimits<float>::Max();

        /** The acceptance items this candidate misses over every shot, struck ones included; empty when it meets them all. */
        FString AcceptanceGaps() const
        {
            TArray<FString> Gaps;
            if (!bFeasible)
            {
                Gaps.Add(TEXT("a search constraint"));
            }
            if (FMath::Abs(Bias[0]) > 10.0f)
            {
                Gaps.Add(FString::Printf(TEXT("ground bias %+.1f outside +/-10 yd"), Bias[0]));
            }
            if (AirborneMae > RetentionModelAirborneMae)
            {
                Gaps.Add(FString::Printf(TEXT("airborne roll %.2f > %.2f yd"), AirborneMae, RetentionModelAirborneMae));
            }
            for (int32 Class = 0; Class < ClassCount; ++Class)
            {
                if (Count[Class] > 0 && Mae[Class] > RetentionModelClassMae[Class] + 0.5f)
                {
                    Gaps.Add(FString::Printf(TEXT("%s roll %.2f > %.2f yd"), ClassNames[Class], Mae[Class], RetentionModelClassMae[Class] + 0.5f));
                }
            }
            return FString::Join(Gaps, TEXT(", "));
        }

        bool Accepted() const
        {
            return AcceptanceGaps().IsEmpty();
        }
    };

    float StimpFeet(const FSurfaceSettings& Surface, const FRolloutProfile& Rollout)
    {
        const FVector3f Up(0.0f, 1.0f, 0.0f);
        FBallState Ball;
        Ball.Velocity = FVector3f(GreenSpeed::StimpmeterSpeedMps, 0.0f, 0.0f);
        Ball.Spin = FVector3f::CrossProduct(Up, Ball.Velocity) / GolfPhysics::Radius;
        const int32 Steps = static_cast<int32>(60.0f / GolfPhysics::SimulationDt);
        for (int32 Step = 0; Step < Steps && !GolfPhysics::CanRest(Ball, true, Up, Surface, Rollout); ++Step)
        {
            Ball = GolfPhysics::StepGround(Ball, Up, Surface, Rollout, 0.0f, GolfPhysics::SimulationDt);
        }
        return Ball.Position.X / 0.3048f;
    }

    FSurfaceSettings Candidate(FSurfaceSettings Surface, const float (&P)[ParameterCount], const float (&S)[StruckParameterCount])
    {
        Surface.LandingAngleLoss = P[AngleLoss];
        Surface.LandingSpinLoss = P[SpinLoss];
        Surface.RollingFriction = P[Rolling];
        Surface.GrassViscosity = P[Viscosity];
        Surface.GrassFormDrag = P[FormDrag];
        Surface.StruckViscosityScale = S[StruckViscosity];
        Surface.StruckFormDragScale = S[StruckFormDrag];
        return Surface;
    }

    FShotSimulationResult SimulateWith(const FLaunchData& Launch, const FBallPhysicsProfile& Profile, const float (&P)[ParameterCount],
        const float (&S)[StruckParameterCount])
    {
        const FShotSpin Spin = ShotSetup::ParseSpin(Launch);
        const FResolvedShotParams Params =
            FResolvedShotParams::Resolve(Profile, {}, Launch.SpeedMph, Launch.VerticalLaunchDeg, Spin.TotalSpinRpm, Spin.BackSpinRpm);
        FShotSimulationOptions Options;
        Options.SurfaceOverride = Candidate(Params.ResolveSurface(ESurface::Fairway, Profile), P, S);
        return ShotSimulator::Simulate(Launch, ESurface::Fairway, Profile, {}, 60.0f, FVector3f(0.0f, 1.0f, 0.0f), Options);
    }

    FLaunchData StraightLaunch(float SpeedMph, float VlaDeg, float BackSpinRpm)
    {
        FLaunchData Launch;
        Launch.SpeedMph = SpeedMph;
        Launch.VerticalLaunchDeg = VlaDeg;
        Launch.BackSpinRpm = BackSpinRpm;
        Launch.SideSpinRpm = 0.0f;
        return Launch;
    }

    /**
     * Worst relative change of total between neighbouring launch angles. The pair that crosses from at or below 0 deg
     * to above it is skipped: that is the struck-along-the-ground step, by design (see GolfPhysics::StruckAlongGround).
     */
    float WorstStep(const FBallPhysicsProfile& Profile, const float (&P)[ParameterCount], const float (&S)[StruckParameterCount], float StartDeg,
        float StepDeg, int32 Samples, float BackSpinRpm, float SpeedMph, bool& bAllRest)
    {
        float Worst = 0.0f, Previous = -1.0f, PreviousVla = 0.0f;
        for (int32 Sample = 0; Sample < Samples; ++Sample)
        {
            const float Vla = StartDeg + Sample * StepDeg;
            const FShotSimulationResult Result = SimulateWith(StraightLaunch(SpeedMph, Vla, BackSpinRpm), Profile, P, S);
            bAllRest &= Result.bAtRest;
            const bool bCrossesZero = PreviousVla <= 1e-4f && Vla > 1e-4f;
            if (Previous > 0.0f && !bCrossesZero)
            {
                Worst = FMath::Max(Worst, FMath::Abs(Result.TotalM - Previous) / FMath::Max(Result.TotalM, Previous));
            }
            Previous = Result.TotalM;
            PreviousVla = Vla;
        }
        return Worst;
    }

    FScore Evaluate(const TArray<FShotLibraryShot>& Shots, const FBallPhysicsProfile& Profile, const FSurfaceSettings& Fairway, float GreenStimp,
        const float (&P)[ParameterCount], const float (&S)[StruckParameterCount])
    {
        FScore Score;
        Score.GreenStimpFeet = GreenStimp;
        Score.StimpFeet = StimpFeet(Candidate(Fairway, P, S), Profile.Rollout);
        Score.RoughHoldDeg = HoldAngleDeg(RoughRollingFriction(P[Rolling]));
        Score.GreenHoldDeg = HoldAngleDeg(FResolvedShotParams().ResolveSurface(ESurface::Green, Profile).RollingFriction);
        float RollSum = 0.0f, TotalSum = 0.0f;
        int32 Compared = 0;
        for (const FShotLibraryShot& Shot : Shots)
        {
            if (Shot.ReferenceCarryYd < 0.0f || Shot.ReferenceTotalYd < 0.0f)
            {
                continue;
            }
            const FShotSimulationResult Result = SimulateWith(Shot.Launch, Profile, P, S);
            Score.bAllRest &= Result.bAtRest;
            const float RollError = (Result.TotalM - Result.CarryM) * YardsPerMeter - (Shot.ReferenceTotalYd - Shot.ReferenceCarryYd);
            const int32 Class = LaunchClass(Shot.Launch);
            Score.Mae[Class] += FMath::Abs(RollError);
            Score.Bias[Class] += RollError;
            ++Score.Count[Class];
            if (IsStruck(Shot.Launch))
            {
                Score.StruckMae += FMath::Abs(RollError);
                Score.StruckBias += RollError;
                ++Score.StruckCount;
            }
            else if (Class == 0)
            {
                Score.FlownGroundMae += FMath::Abs(RollError);
                Score.FlownGroundBias += RollError;
                ++Score.FlownGroundCount;
            }
            RollSum += FMath::Abs(RollError);
            TotalSum += FMath::Abs(Result.TotalM * YardsPerMeter - Shot.ReferenceTotalYd);
            Score.WorstRoll = FMath::Max(Score.WorstRoll, FMath::Abs(RollError));
            ++Compared;
        }
        float AirborneSum = 0.0f;
        int32 AirborneCount = 0;
        for (int32 Class = 0; Class < ClassCount; ++Class)
        {
            if (Class > 0)
            {
                AirborneSum += Score.Mae[Class];
                AirborneCount += Score.Count[Class];
            }
            if (Score.Count[Class] > 0)
            {
                Score.Mae[Class] /= Score.Count[Class];
                Score.Bias[Class] /= Score.Count[Class];
            }
        }
        if (Score.FlownGroundCount > 0)
        {
            Score.FlownGroundMae /= Score.FlownGroundCount;
            Score.FlownGroundBias /= Score.FlownGroundCount;
        }
        if (Score.StruckCount > 0)
        {
            Score.StruckMae /= Score.StruckCount;
            Score.StruckBias /= Score.StruckCount;
        }
        Score.AirborneMae = AirborneCount > 0 ? AirborneSum / AirborneCount : 0.0f;
        Score.RollMae = Compared > 0 ? RollSum / Compared : 0.0f;
        Score.TotalMae = Compared > 0 ? TotalSum / Compared : 0.0f;

        // The same sweeps as UOpenGolf.Physics.RolloutRegression.
        const TPair<float, float> Sweeps[] = {{60.0f, 1500.0f}, {80.0f, 2000.0f}, {100.0f, 2400.0f}};
        for (const TPair<float, float>& Sweep : Sweeps)
        {
            Score.WorstStep = FMath::Max(Score.WorstStep, WorstStep(Profile, P, S, -3.0f, 1.0f, 16, Sweep.Value, Sweep.Key, Score.bAllRest));
        }
        for (const float Speed : {60.0f, 80.0f, 100.0f})
        {
            Score.WorstFineStep = FMath::Max(Score.WorstFineStep, WorstStep(Profile, P, S, -1.0f, 0.1f, 21, 2000.0f, Speed, Score.bAllRest));
        }

        const float Violation = FMath::Max(0.0f, Score.WorstStep - SearchStepLimit) * 100.0f +
                                FMath::Max(0.0f, Score.WorstFineStep - SearchFineStepLimit) * 100.0f +
                                FMath::Max(0.0f, Score.StimpFeet - GreenStimp) + FMath::Max(0.0f, MinFairwayStimpFeet - Score.StimpFeet) +
                                FMath::Max(0.0f, Score.GreenHoldDeg + MinRoughOverGreenHoldDeg - Score.RoughHoldDeg) * 10.0f +
                                FMath::Max(0.0f, FMath::Abs(Score.FlownGroundBias) - 10.0f);
        Score.bFeasible = Score.bAllRest && Violation <= 0.0f;
        // Airborne roll error decides; the near-ground launches that fly count a little. The struck shots have their
        // own drag scales (stage 2), so they do not pull the turf values.
        const float Fit = Score.AirborneMae + 0.05f * Score.FlownGroundMae + (Score.bAllRest ? 0.0f : 1000.0f);
        Score.Score = Fit + (Violation > 0.0f ? 100.0f + Violation : 0.0f);
        return Score;
    }

    /** Stage 2: mean absolute total error of the struck shots alone (their carry is 0, so roll and total agree). */
    float StruckError(const TArray<FShotLibraryShot>& Shots, const FBallPhysicsProfile& Profile, const float (&P)[ParameterCount],
        const float (&S)[StruckParameterCount])
    {
        float Sum = 0.0f;
        int32 Count = 0;
        bool bAllRest = true;
        for (const FShotLibraryShot& Shot : Shots)
        {
            if (Shot.ReferenceTotalYd < 0.0f || !IsStruck(Shot.Launch))
            {
                continue;
            }
            const FShotSimulationResult Result = SimulateWith(Shot.Launch, Profile, P, S);
            bAllRest &= Result.bAtRest;
            Sum += FMath::Abs(Result.TotalM * YardsPerMeter - Shot.ReferenceTotalYd);
            ++Count;
        }
        return (Count > 0 ? Sum / Count : 0.0f) + (bAllRest ? 0.0f : 1000.0f);
    }

    FString Describe(const float (&P)[ParameterCount], const float (&S)[StruckParameterCount], const FScore& Score)
    {
        FString Text = FString::Printf(TEXT("angle loss %.3f, spin loss %.3f, rolling %.4f, viscosity %.4f, form drag %.4f, struck x%.3f / x%.3f: ")
                                       TEXT("airborne roll %.2f yd, all roll %.2f, total %.2f, worst roll %.1f;"),
            P[AngleLoss], P[SpinLoss], P[Rolling], P[Viscosity], P[FormDrag], S[StruckViscosity], S[StruckFormDrag], Score.AirborneMae, Score.RollMae,
            Score.TotalMae, Score.WorstRoll);
        for (int32 Class = 0; Class < ClassCount; ++Class)
        {
            Text += FString::Printf(TEXT(" %s %+.1f/%.1f"), ClassNames[Class], Score.Bias[Class], Score.Mae[Class]);
        }
        Text += FString::Printf(TEXT(" (struck %+.1f/%.1f over %d, flown %+.1f/%.1f over %d)"), Score.StruckBias, Score.StruckMae, Score.StruckCount,
            Score.FlownGroundBias, Score.FlownGroundMae, Score.FlownGroundCount);
        return Text + FString::Printf(TEXT("; worst 1 deg step %.0f%%, 0.1 deg step %.1f%% (0 deg crossing excluded), stimp %.1f ft (green %.1f), ")
                                      TEXT("slope hold rough %.1f deg (green %.1f); %s, %s"),
            Score.WorstStep * 100.0f, Score.WorstFineStep * 100.0f, Score.StimpFeet, Score.GreenStimpFeet, Score.RoughHoldDeg, Score.GreenHoldDeg,
            Score.bFeasible ? TEXT("feasible") : TEXT("infeasible"),
            Score.Accepted() ? TEXT("meets the acceptance list") : *(TEXT("misses: ") + Score.AcceptanceGaps()));
    }
} // namespace RolloutCalibrationDetail

// Explicit opt-in fit, outside the ordinary Physics test prefix, writing every candidate to Saved/ShotLibrary/calibration.csv.
// Stage 1 searches the fairway's landing loss and ground resistance against the reference roll of the local launch-monitor
// shots that leave the ground; stage 2 fits the struck-along-the-ground grass drag scales to the shots launched into the
// ground. It uses every local shot with a reference total. Shots are grouped by launch values only; the reported club is
// never read. Nothing here changes the catalog.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRolloutCalibrationTest, "UOpenGolf.Calibration.Rollout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRolloutCalibrationTest::RunTest(const FString& Parameters)
{
    using namespace RolloutCalibrationDetail;
    TArray<FShotLibraryShot> Shots;
    TArray<FString> Errors;
    LoadShotLibrary(LocalShotLibraryDir(), Shots, Errors);
    for (const FString& Error : Errors)
    {
        AddError(Error);
    }
    Shots.RemoveAll([](const FShotLibraryShot& Shot) { return Shot.ReferenceTotalYd < 0.0f; });
    if (Shots.IsEmpty())
    {
        AddWarning(FString::Printf(TEXT("No local shots with a reference total in %s; nothing to calibrate."), *LocalShotLibraryDir()));
        return true;
    }
    const FBallPhysicsProfile Profile;
    const FResolvedShotParams Neutral;
    const FSurfaceSettings Fairway = Neutral.ResolveSurface(ESurface::Fairway, Profile);
    const float GreenStimp = StimpFeet(Neutral.ResolveSurface(ESurface::Green, Profile), Profile.Rollout);

    FString Csv;
    for (const TCHAR* Name : ParameterNames)
    {
        Csv += FString(Name) + TEXT(",");
    }
    Csv += TEXT("struck_viscosity_scale,struck_form_scale,stage,score,feasible,accepted,airborne_roll_mae,roll_mae,total_mae,worst_roll");
    for (const TCHAR* Name : ClassNames)
    {
        Csv += FString::Printf(TEXT(",%s_bias,%s_mae"), Name, Name);
    }
    Csv += TEXT(",struck_bias,struck_mae,flown_ground_bias,flown_ground_mae,worst_step,worst_fine_step,stimp_ft\n");

    const float Catalog[ParameterCount] = {Fairway.LandingAngleLoss, Fairway.LandingSpinLoss, Fairway.RollingFriction, Fairway.GrassViscosity, Fairway.GrassFormDrag};
    const float CatalogStruck[StruckParameterCount] = {Fairway.StruckViscosityScale, Fairway.StruckFormDragScale};
    int32 Evaluations = 0;
    auto Record = [&](const float (&P)[ParameterCount], const float (&S)[StruckParameterCount], int32 Stage, const FScore& Score)
    {
        ++Evaluations;
        for (const float Value : P)
        {
            Csv += FString::Printf(TEXT("%g,"), Value);
        }
        Csv += FString::Printf(TEXT("%g,%g,%d,%g,%d,%d,%g,%g,%g,%g"), S[StruckViscosity], S[StruckFormDrag], Stage, Score.Score, Score.bFeasible ? 1 : 0,
            Score.Accepted() ? 1 : 0, Score.AirborneMae, Score.RollMae, Score.TotalMae, Score.WorstRoll);
        for (int32 Class = 0; Class < ClassCount; ++Class)
        {
            Csv += FString::Printf(TEXT(",%g,%g"), Score.Bias[Class], Score.Mae[Class]);
        }
        Csv += FString::Printf(TEXT(",%g,%g,%g,%g,%g,%g,%g\n"), Score.StruckBias, Score.StruckMae, Score.FlownGroundBias, Score.FlownGroundMae, Score.WorstStep,
            Score.WorstFineStep, Score.StimpFeet);
    };
    const FScore CatalogScore = Evaluate(Shots, Profile, Fairway, GreenStimp, Catalog, CatalogStruck);
    Record(Catalog, CatalogStruck, 0, CatalogScore);
    AddInfo(TEXT("Catalog: ") + Describe(Catalog, CatalogStruck, CatalogScore));

    // Stage 1, from the catalog values: global samples across the whole range, perturbations around the best that
    // narrow as the search goes, then coordinate refinement with a halving step. The struck scales stay at the
    // catalog's; they do not touch any shot this stage scores.
    float Best[ParameterCount];
    FMemory::Memcpy(Best, Catalog, sizeof(Best));
    FScore BestScore = CatalogScore;
    const auto Try = [&](const float (&P)[ParameterCount])
    {
        const FScore Score = Evaluate(Shots, Profile, Fairway, GreenStimp, P, CatalogStruck);
        Record(P, CatalogStruck, 1, Score);
        if (Score.Score < BestScore.Score)
        {
            BestScore = Score;
            FMemory::Memcpy(Best, P, sizeof(Best));
        }
    };
    FRandomStream Random(20261117);
    for (int32 Trial = 0; Trial < 400; ++Trial)
    {
        float P[ParameterCount];
        for (int32 Axis = 0; Axis < ParameterCount; ++Axis)
        {
            P[Axis] = Random.FRandRange(Lower[Axis], Upper[Axis]);
        }
        Try(P);
    }
    constexpr int32 LocalTrials = 600;
    for (int32 Trial = 0; Trial < LocalTrials; ++Trial)
    {
        const float Width = FMath::Lerp(0.15f, 0.01f, static_cast<float>(Trial) / LocalTrials);
        float P[ParameterCount];
        for (int32 Axis = 0; Axis < ParameterCount; ++Axis)
        {
            P[Axis] = FMath::Clamp(Best[Axis] + Random.FRandRange(-1.0f, 1.0f) * Width * (Upper[Axis] - Lower[Axis]), Lower[Axis], Upper[Axis]);
        }
        Try(P);
    }
    for (float Width = 0.02f; Width > 0.001f; Width *= 0.5f)
    {
        for (int32 Axis = 0; Axis < ParameterCount; ++Axis)
        {
            for (const float Direction : {-1.0f, 1.0f})
            {
                float P[ParameterCount];
                FMemory::Memcpy(P, Best, sizeof(P));
                P[Axis] = FMath::Clamp(P[Axis] + Direction * Width * (Upper[Axis] - Lower[Axis]), Lower[Axis], Upper[Axis]);
                Try(P);
            }
        }
    }
    AddInfo(TEXT("Stage 1 best: ") + Describe(Best, CatalogStruck, BestScore));

    // The turf values stage 2 builds on: the catalog's, unless stage 1 beat them on airborne roll by more than 0.05 yd.
    constexpr float AdoptMarginYd = 0.05f;
    const bool bAdopt = BestScore.bFeasible && BestScore.AirborneMae < CatalogScore.AirborneMae - AdoptMarginYd;
    float Turf[ParameterCount];
    FMemory::Memcpy(Turf, bAdopt ? Best : Catalog, sizeof(Turf));
    AddInfo(bAdopt ? TEXT("Stage 2 uses the stage 1 turf values (airborne roll improved by more than 0.05 yd).")
                   : TEXT("Stage 2 uses the catalog turf values (stage 1 did not improve airborne roll by more than 0.05 yd)."));

    // Stage 2: a grid over the struck scales, then coordinate refinement with a halving step.
    float Struck[StruckParameterCount];
    FMemory::Memcpy(Struck, CatalogStruck, sizeof(Struck));
    float BestStruckError = StruckError(Shots, Profile, Turf, Struck);
    const auto TryStruck = [&](const float (&S)[StruckParameterCount])
    {
        const float Error = StruckError(Shots, Profile, Turf, S);
        if (Error < BestStruckError)
        {
            BestStruckError = Error;
            FMemory::Memcpy(Struck, S, sizeof(Struck));
        }
    };
    constexpr int32 GridSteps = 25;
    for (int32 I = 0; I <= GridSteps; ++I)
    {
        for (int32 J = 0; J <= GridSteps; ++J)
        {
            const float S[StruckParameterCount] = {FMath::Lerp(StruckLower[StruckViscosity], StruckUpper[StruckViscosity], static_cast<float>(I) / GridSteps),
                FMath::Lerp(StruckLower[StruckFormDrag], StruckUpper[StruckFormDrag], static_cast<float>(J) / GridSteps)};
            TryStruck(S);
        }
    }
    for (float Width = 0.04f; Width > 0.0005f; Width *= 0.5f)
    {
        bool bImproved = true;
        while (bImproved)
        {
            bImproved = false;
            for (int32 Axis = 0; Axis < StruckParameterCount; ++Axis)
            {
                for (const float Direction : {-1.0f, 1.0f})
                {
                    float S[StruckParameterCount];
                    FMemory::Memcpy(S, Struck, sizeof(S));
                    S[Axis] = FMath::Clamp(S[Axis] + Direction * Width * (StruckUpper[Axis] - StruckLower[Axis]), StruckLower[Axis], StruckUpper[Axis]);
                    const float Before = BestStruckError;
                    TryStruck(S);
                    bImproved |= BestStruckError < Before - 1e-4f;
                }
            }
        }
    }
    const FScore Final = Evaluate(Shots, Profile, Fairway, GreenStimp, Turf, Struck);
    Record(Turf, Struck, 2, Final);
    AddInfo(FString::Printf(TEXT("Stage 2 best (struck shots %.2f yd mean total error): "), BestStruckError) + Describe(Turf, Struck, Final));

    AddInfo(FString::Printf(TEXT("%d candidates in %s"), Evaluations, *(FPaths::ProjectSavedDir() / TEXT("ShotLibrary/calibration.csv"))));
    return TestTrue(TEXT("Calibration report written"), FFileHelper::SaveStringToFile(Csv,
        *(FPaths::ProjectSavedDir() / TEXT("ShotLibrary/calibration.csv")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
}

#endif
