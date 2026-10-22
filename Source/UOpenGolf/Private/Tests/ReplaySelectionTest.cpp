// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyReplaySelection, FAutomationTestBase*, Test);

bool FVerifyReplaySelection::Update()
{
    UWorld* World = GEditor->PlayWorld;
    auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
    auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    if (!World || !World->HasBegunPlay() || !Controller || !Mode || !Mode->GetBall())
    {
        if (GetCurrentRunTime() > 30.0)
        {
            Test->AddError(TEXT("Timed out waiting for replay controls."));
            return true;
        }
        return false;
    }
    auto* Monitor = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
    FLaunchMonitorShot Shot;
    Test->TestEqual(TEXT("Default selects screenshot wood"), Controller->GetSelectedReplayShot(), 0);
    Controller->SetReplayPanelOpen(true);
    Test->TestTrue(TEXT("Earlier screenshot selectable"), Controller->SelectReplayShot(3));
    Test->TestFalse(TEXT("Selection does not hit ball"), Monitor->GetLastShot(Shot));
    Controller->InjectReplayShot();
    Test->TestFalse(TEXT("Hit closes selector"), Controller->IsReplayPanelOpen());
    Test->TestTrue(TEXT("Selected shot delivered to range"), Monitor->GetLastShot(Shot) && Mode->GetBall()->IsMoving());
    Test->TestEqual(TEXT("Selected screenshot inputs"), Shot.GetBallSpeedMph(), 75.7f, 0.02f);
    const int32 FirstNumber = Shot.ShotNumber;
    Controller->InjectReplayShot();
    Monitor->GetLastShot(Shot);
    Test->TestEqual(TEXT("H repeats selected speed"), Shot.GetBallSpeedMph(), 75.7f, 0.02f);
    Test->TestEqual(TEXT("R delivers another shot"), Shot.ShotNumber, FirstNumber + 1);
    Test->TestFalse(TEXT("Bad selection rejected"), Controller->SelectReplayShot(999));
    Test->TestEqual(TEXT("Bad selection retains preset"), Controller->GetSelectedReplayShot(), 3);
    Test->TestTrue(TEXT("Wood can be reselected"), Controller->SelectReplayShot(0));
    Controller->InjectReplayShot();
    Monitor->GetLastShot(Shot);
    Test->TestEqual(TEXT("New preset takes effect"), Shot.GetBallSpeedMph(), 114.46f, 0.01f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FReplaySelectionTest, "UOpenGolf.Range.ReplaySelection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifySelectedWoodCarry, FAutomationTestBase*, Test);

bool FVerifySelectedWoodCarry::Update()
{
    UWorld* World = GEditor->PlayWorld;
    const auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    FOpenFairwayShotResult Result;
    if (Mode && Mode->GetBall() && Mode->GetBall()->GetShotResult(Result) && Result.bLanded)
    {
        const float Carry = Result.CarryM * OpenFairway::YardsPerMeter;
        Test->AddInfo(FString::Printf(TEXT("Selected wood in range: %.4f yd carry"), Carry));
        Test->TestEqual(TEXT("Game replay uses corrected low-launch flight"), Carry, 121.5451f, 0.1f);
        return true;
    }
    if (GetCurrentRunTime() > 30.0)
    {
        Test->AddError(TEXT("Selected wood did not land within 30 seconds."));
        return true;
    }
    return false;
}

bool FReplaySelectionTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyReplaySelection(this));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifySelectedWoodCarry(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
