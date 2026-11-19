// SPDX-License-Identifier: MIT
// Replays the shot library (ShotLibrary.h) through the headless simulator and writes the engine's results for
// Assets/Scripts/shot_compare.py and carry_audit.py, which report them against the reference results stored with each
// shot. Differences are reported, not asserted.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "ShotLibrary.h"

using namespace OpenFairway;

namespace ShotLibraryTestDetail
{
    struct FShotLibraryProfile
    {
        const TCHAR* Name;
        OpenFairway::FBallPhysicsProfile Profile;
    };

    /**
     * Profiles replayed for every shot: the gameplay profile ("current") first, then any candidate change under evaluation,
     * which shot_compare.py and carry_audit.py report beside it.
     */
    TArray<FShotLibraryProfile> MakeShotLibraryProfiles()
    {
        TArray<FShotLibraryProfile> Profiles;
        Profiles.Add({TEXT("current"), OpenFairway::FBallPhysicsProfile()});
        return Profiles;
    }
} // namespace ShotLibraryTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayShotLibraryTest, "UOpenGolf.Physics.ShotLibrary",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayShotLibraryTest::RunTest(const FString& Parameters)
{
    using namespace OpenFairwayShotLibrary;
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
    const TArray<ShotLibraryTestDetail::FShotLibraryProfile> Profiles = ShotLibraryTestDetail::MakeShotLibraryProfiles();
    FString Csv = TEXT("session,shot_name,profile,carry_yd,total_yd,rollout_yd,apex_ft,hang_s,lateral_yd,at_rest,landing_angle_deg,landing_spin_rpm,impact_in_mps,impact_out_mps,bounce_count,ground_distance_m,termination,temperature_f,altitude_ft,humidity,wind_mph,air_density,landing_speed_mps,outside_coverage_s,min_re,max_re,min_spin_ratio,max_spin_ratio\n");
    int32 ResultCount = 0;
    for (const FShotLibraryShot& Shot : Shots)
    {
        for (const ShotLibraryTestDetail::FShotLibraryProfile& Entry : Profiles)
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
            {
                // The 120 Hz flight has converged: a 960 Hz run carries the same.
                FShotSimulationOptions Fine;
                Fine.Dt = 1.0f / 960.0f;
                const auto Reference = ShotSimulator::Simulate(Shot.Launch, ESurface::Fairway, Entry.Profile, Shot.Air,
                    ShotSimulator::DefaultMaxTimeSeconds, FVector3f(0, 1, 0), Fine);
                TestEqual(Label + TEXT(" carry converges within 0.1 yd"), Result.CarryM * YardsPerMeter,
                    Reference.CarryM * YardsPerMeter, 0.1f);
            }
            // The gameplay profile must stop in time; a candidate that does not is reported (at_rest=false in the results).
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
            Csv += FString::Printf(TEXT("%s,%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%s,%.4f,%.1f,%.4f,%.4f,%d,%.4f,%s,%.1f,%.0f,%.2f,%.1f,%.6f,%.4f,%.4f,%.1f,%.1f,%.5f,%.5f\n"), *Shot.Session, *Shot.ShotName, Entry.Name,
                Result.CarryM * YardsPerMeter, Result.TotalM * YardsPerMeter, (Result.TotalM - Result.CarryM) * YardsPerMeter,
                Result.ApexM * FeetPerMeter, Result.HangTimeS, Result.LateralM * YardsPerMeter, Result.bAtRest ? TEXT("true") : TEXT("false"),
                Result.LandingAngleDeg, Result.LandingSpinRpm, Result.FirstImpactTangentInMps, Result.FirstImpactTangentOutMps,
                Result.BounceCount, Result.GroundDistanceM, Result.bAtRest ? TEXT("Rest") : TEXT("TimeLimit"),
                Shot.Air.TemperatureF, Shot.Air.AltitudeFt, Shot.Air.RelativeHumidity, Shot.Air.WindSpeedMph,
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
    using namespace OpenFairwayShotLibrary;
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

#endif
