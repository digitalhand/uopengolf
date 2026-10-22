// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "LaunchMonitorTypes.h"
#include "OpenFairwayBall.h"
#include "UOpenGolfGameMode.h"

#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
    /** The drive fixture: it lands and rolls out on the back fairway, so the whole shot stays on one surface. */
    FOpenFairwayLaunch DriveLaunch()
    {
        FOpenFairwayLaunch Launch;
        Launch.BallSpeedMph = 150.0f;
        Launch.VerticalLaunchDeg = 12.5f;
        Launch.HorizontalLaunchDeg = 1.5f;
        Launch.bHasBackSpin = true;
        Launch.BackSpinRpm = 2300.0f;
        Launch.bHasSideSpin = true;
        Launch.SideSpinRpm = -400.0f;
        Launch.bHasTotalSpin = true;
        Launch.TotalSpinRpm = 2335.0f;
        Launch.bHasSpinAxis = true;
        Launch.SpinAxisDeg = -9.9f;
        return Launch;
    }

    // The world sweep and the headless loop split the landing step differently; allow for that.
    constexpr float CarryToleranceYd = 1.0f;
    constexpr float TotalToleranceYd = 2.0f;
    constexpr float OfflineToleranceYd = 1.0f;
} // namespace

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FHitRangeShot, FAutomationTestBase*, Test);

bool FHitRangeShot::Update()
{
    UWorld* World = GEditor->PlayWorld;
    AUOpenGolfGameMode* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    if (!Mode || !Mode->GetBall())
    {
        if (GetCurrentRunTime() > 30.0)
        {
            Test->AddError(TEXT("The range did not start within 30 seconds."));
            return true;
        }
        return false;
    }
    Test->TestTrue(TEXT("Launch accepted"), Mode->GetBall()->Launch(DriveLaunch()));
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyRangeShot, FAutomationTestBase*, Test);

bool FVerifyRangeShot::Update()
{
    UWorld* World = GEditor->PlayWorld;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Ball)
    {
        Test->AddError(TEXT("The ball disappeared during the shot."));
        return true;
    }
    if (Ball->IsMoving())
    {
        if (GetCurrentRunTime() > 60.0)
        {
            Test->AddError(TEXT("The ball did not come to rest within 60 seconds."));
            return true;
        }
        return false;
    }

    FOpenFairwayShotResult Shot;
    if (!Test->TestTrue(TEXT("Shot recorded"), Ball->GetShotResult(Shot)))
    {
        return true;
    }
    const OpenFairway::FShotSimulationResult Predicted = Ball->PredictShot(DriveLaunch(), EOpenFairwaySurface::Fairway);
    const float Yards = OpenFairway::YardsPerMeter;
    Test->AddInfo(FString::Printf(TEXT("Range: carry %.2f, total %.2f, offline %.2f yd; headless: carry %.2f, total %.2f, offline %.2f yd"),
        Shot.CarryM * Yards, Shot.TotalM * Yards, Shot.LateralM * Yards, Predicted.CarryM * Yards, Predicted.TotalM * Yards, Predicted.LateralM * Yards));
    Test->TestTrue(TEXT("Landed"), Shot.bLanded);
    Test->TestTrue(TEXT("At rest"), Shot.bAtRest);
    Test->TestTrue(TEXT("Landed on fairway"), Shot.LandingSurface == EOpenFairwaySurface::Fairway);
    Test->TestEqual(TEXT("Carry matches headless"), Shot.CarryM * Yards, Predicted.CarryM * Yards, CarryToleranceYd);
    Test->TestEqual(TEXT("Total matches headless"), Shot.TotalM * Yards, Predicted.TotalM * Yards, TotalToleranceYd);
    Test->TestEqual(TEXT("Offline matches headless"), Shot.LateralM * Yards, Predicted.LateralM * Yards, OfflineToleranceYd);
    Test->TestEqual(TEXT("Roll is total minus carry"), Shot.RollM, Shot.TotalM - Shot.CarryM, 1e-3f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOpenGolfRangeShotTest, "UOpenGolf.Range.Shot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUOpenGolfRangeShotTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the startup map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FHitRangeShot(this));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyRangeShot(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUOpenGolfShotConversionTest, "UOpenGolf.Range.ShotConversion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FUOpenGolfShotConversionTest::RunTest(const FString& Parameters)
{
    FLaunchMonitorShot Shot;
    Shot.BallSpeedMps = 51.17f;
    Shot.BallSpeedSource = ELaunchMonitorValueSource::Measured;
    Shot.VerticalLaunchDeg = 6.95f;
    Shot.VerticalLaunchSource = ELaunchMonitorValueSource::Measured;
    Shot.TotalSpinRpm = 1933;
    Shot.TotalSpinSource = ELaunchMonitorValueSource::Measured;
    Shot.SpinAxisDeg = -1.42f;
    Shot.SpinAxisSource = ELaunchMonitorValueSource::Measured;
    Shot.BackSpinRpm = 1932;
    Shot.BackSpinSource = ELaunchMonitorValueSource::Derived;

    FOpenFairwayLaunch Launch;
    TestTrue(TEXT("Measured shot converts"), AUOpenGolfGameMode::ToLaunch(Shot, Launch));
    TestEqual(TEXT("Speed in mph"), Launch.BallSpeedMph, Shot.GetBallSpeedMph(), 1e-4f);
    TestEqual(TEXT("Missing HLA is straight"), Launch.HorizontalLaunchDeg, 0.0f);
    TestTrue(TEXT("Derived backspin is used"), Launch.bHasBackSpin);
    TestFalse(TEXT("Missing sidespin is left out"), Launch.bHasSideSpin);
    TestTrue(TEXT("Total spin and axis are used"), Launch.bHasTotalSpin && Launch.bHasSpinAxis);

    Shot.HorizontalLaunchSource = ELaunchMonitorValueSource::Measured;
    for (float Hla : {-0.97f, 0.0f, 0.97f})
    {
        Shot.HorizontalLaunchDeg = Hla;
        TestTrue(TEXT("Signed direction converts"), AUOpenGolfGameMode::ToLaunch(Shot, Launch));
        TestEqual(TEXT("Signed direction preserved"), Launch.HorizontalLaunchDeg, Hla);
    }

    Shot.BallSpeedSource = ELaunchMonitorValueSource::Missing;
    TestFalse(TEXT("A shot without ball speed is not hit"), AUOpenGolfGameMode::ToLaunch(Shot, Launch));
    return true;
}

class FCheckRangeFrameRates : public IAutomationLatentCommand
{
public:
    explicit FCheckRangeFrameRates(FAutomationTestBase* InTest) : Test(InTest) {}
    bool Update() override
    {
        UWorld* World = GEditor->PlayWorld;
        auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        if (!Mode || !Mode->GetBall())
        {
            if (GetCurrentRunTime() > 30) { Test->AddError(TEXT("Range did not start")); return true; }
            return false;
        }
        AOpenFairwayBall* Ball = Mode->GetBall();
        FOpenFairwayShotResult Baseline;
        const float Deltas[] = {1.0f / 60, 1.0f / 30, 1.0f / 144, 0.2f, 1.0f / 60};
        // Drive the actor directly within one latent update; world timers cannot auto-reset it between steps.
        for (int32 Case = 0; Case < UE_ARRAY_COUNT(Deltas); ++Case)
        {
            Mode->ResetBallToTee();
            Test->TestTrue(TEXT("Frame-rate shot accepted"), Ball->Launch(DriveLaunch()));
            int32 Frames = 0;
            while (Ball->IsMoving() && Frames < 10000)
            {
                const float Dt = Case == 4 && Frames % 31 == 0 ? 0.25f : Deltas[Case];
                Ball->Tick(Dt);
                if (Frames == 0 && (Case == 3 || Case == 4))
                {
                    FOpenFairwayShotResult First;
                    Ball->GetShotResult(First);
                    Test->TestEqual(TEXT("Hitch consumes at most 12 physics steps"), First.ElapsedS, 0.1f, 1e-5f);
                }
                ++Frames;
            }
            FOpenFairwayShotResult Result;
            Ball->GetShotResult(Result);
            Test->TestTrue(TEXT("Shot reaches rest at every frame rate"), Result.bAtRest);
            if (Case == 0) Baseline = Result;
            Test->TestEqual(TEXT("Carry independent of render rate and stalls"), Result.CarryM, Baseline.CarryM, 0.001f);
            Test->TestEqual(TEXT("Total independent of render rate and stalls"), Result.TotalM, Baseline.TotalM, 0.001f);
            Test->TestEqual(TEXT("Simulation duration independent of render rate"), Result.ElapsedS, Baseline.ElapsedS, 0.01f);
        }
        return true;
    }
private:
    FAutomationTestBase* Test;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeFrameRatesTest, "UOpenGolf.Range.FrameRates",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRangeFrameRatesTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true)) return false;
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCheckRangeFrameRates>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
