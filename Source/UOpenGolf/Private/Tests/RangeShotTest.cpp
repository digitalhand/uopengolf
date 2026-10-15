// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

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
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/HelloWorld"), false, true))
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

#endif
