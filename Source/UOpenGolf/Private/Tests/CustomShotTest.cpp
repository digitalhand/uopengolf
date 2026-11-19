// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Tests/PermissionTestAccess.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "LaunchMonitorLimits.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"

#include <limits>

DEFINE_LATENT_AUTOMATION_COMMAND_ONE_PARAMETER(FVerifyCustomShot, FAutomationTestBase*, Test);

bool FVerifyCustomShot::Update()
{
    UWorld* World = GEditor->PlayWorld;
    auto* Controller = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
    auto* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    if (!World || !World->HasBegunPlay() || !Controller || !Mode || !Mode->GetBall())
    {
        if (GetCurrentRunTime() > 30.0)
        {
            Test->AddError(TEXT("Timed out waiting for the test-shot controls."));
            return true;
        }
        return false;
    }
    auto* Monitor = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
    FLaunchMonitorShot Shot;

    // Defaults: a basic straight shot, with the presets still selected until Custom is chosen.
    const FOpenGolfCustomShot Defaults = Controller->GetCustomShot();
    Test->TestFalse(TEXT("Presets selected at start"), Controller->IsCustomShotSelected());
    Test->TestEqual(TEXT("Default speed 100 mph"), Defaults.BallSpeedMps * LaunchMonitorMphPerMps, 100.0f, 0.001f);
    Test->TestEqual(TEXT("Default launch"), Defaults.VerticalLaunchDeg, 16.0f);
    Test->TestEqual(TEXT("Default straight direction"), Defaults.HorizontalLaunchDeg, 0.0f);
    Test->TestEqual(TEXT("Default spin"), Defaults.TotalSpinRpm, 5000);
    Test->TestEqual(TEXT("Default straight axis"), Defaults.SpinAxisDeg, 0.0f);

    // A custom shot is a manual shot: without the admin permission it never reaches the ball.
    Controller->SelectCustomShot();
    const int32 Before = Mode->GetBall()->GetShotNumber();
    Controller->InjectReplayShot();
    Test->TestEqual(TEXT("Non-admin cannot hit a custom shot"), Mode->GetBall()->GetShotNumber(), Before);
    Monitor->InjectCustomShot(44.704f, 16.0f, 0.0f, 5000, 0.0f);
    Test->TestEqual(TEXT("Direct custom injection cannot bypass permission"), Mode->GetBall()->GetShotNumber(), Before);

    FOpenGolfPermissionTestAccess::Set(World);
    Controller->SetReplayPanelOpen(true);
    Controller->InjectReplayShot();
    Test->TestFalse(TEXT("Hit closes the panel"), Controller->IsReplayPanelOpen());
    Test->TestTrue(TEXT("Custom shot delivered to range"), Monitor->GetLastShot(Shot) && Mode->GetBall()->IsMoving());
    Test->TestEqual(TEXT("Custom source"), Shot.Source, FString(LaunchMonitorCustomSource));
    Test->TestEqual(TEXT("Default custom speed"), Shot.GetBallSpeedMph(), 100.0f, 0.001f);
    Test->TestEqual(TEXT("Default custom direction"), Shot.HorizontalLaunchDeg, 0.0f);

    // Edited values repeat exactly: H is the duplicate.
    FOpenGolfCustomShot Edited;
    Edited.BallSpeedMps = 120.0f / LaunchMonitorMphPerMps;
    Edited.VerticalLaunchDeg = 13.5f;
    Edited.HorizontalLaunchDeg = 1.5f;
    Edited.TotalSpinRpm = 4200;
    Edited.SpinAxisDeg = -10.0f;
    Controller->SetCustomShot(Edited);
    Controller->InjectReplayShot();
    Monitor->GetLastShot(Shot);
    const FLaunchMonitorShot First = Shot;
    Controller->InjectReplayShot();
    Monitor->GetLastShot(Shot);
    Test->TestEqual(TEXT("Edited speed"), First.GetBallSpeedMph(), 120.0f, 0.001f);
    Test->TestEqual(TEXT("Edited axis"), First.SpinAxisDeg, -10.0f);
    Test->TestEqual(TEXT("Repeat keeps speed"), Shot.BallSpeedMps, First.BallSpeedMps);
    Test->TestEqual(TEXT("Repeat keeps launch"), Shot.VerticalLaunchDeg, First.VerticalLaunchDeg);
    Test->TestEqual(TEXT("Repeat keeps direction"), Shot.HorizontalLaunchDeg, First.HorizontalLaunchDeg);
    Test->TestEqual(TEXT("Repeat keeps spin"), Shot.TotalSpinRpm, First.TotalSpinRpm);
    Test->TestEqual(TEXT("Repeat keeps axis"), Shot.SpinAxisDeg, First.SpinAxisDeg);
    Test->TestEqual(TEXT("Repeat is a new shot"), Shot.ShotNumber, First.ShotNumber + 1);

    // Out-of-range entries are limited to what the launch monitor accepts, so they still hit.
    FOpenGolfCustomShot Extreme;
    Extreme.BallSpeedMps = 1000.0f;
    Extreme.VerticalLaunchDeg = -95.0f;
    Extreme.HorizontalLaunchDeg = 120.0f;
    Extreme.TotalSpinRpm = -50;
    Extreme.SpinAxisDeg = -400.0f;
    Controller->SetCustomShot(Extreme);
    const FOpenGolfCustomShot Limited = Controller->GetCustomShot();
    Test->TestTrue(TEXT("Speed limited"), Limited.BallSpeedMps > 0.0f && Limited.BallSpeedMps < LaunchMonitorLimits::MaxBallSpeedMps);
    Test->TestTrue(TEXT("Launch limited"), FMath::Abs(Limited.VerticalLaunchDeg) < LaunchMonitorLimits::MaxAbsVerticalAngleDeg && Limited.VerticalLaunchDeg < 0.0f);
    Test->TestTrue(TEXT("Direction limited"), FMath::Abs(Limited.HorizontalLaunchDeg) < LaunchMonitorLimits::MaxAbsHorizontalAngleDeg && Limited.HorizontalLaunchDeg > 0.0f);
    Test->TestEqual(TEXT("Spin limited"), Limited.TotalSpinRpm, 0);
    Test->TestEqual(TEXT("Axis limited"), Limited.SpinAxisDeg, -LaunchMonitorLimits::MaxAbsSpinAxisDeg);
    Extreme.BallSpeedMps = std::numeric_limits<float>::quiet_NaN();
    Controller->SetCustomShot(Extreme);
    Test->TestEqual(TEXT("Non-finite speed takes the default"), Controller->GetCustomShot().BallSpeedMps, Defaults.BallSpeedMps);
    Controller->ResetCustomShot();
    Test->TestEqual(TEXT("Reset restores the straight shot"), Controller->GetCustomShot().TotalSpinRpm, Defaults.TotalSpinRpm);

    // A preset leaves the custom shot; its values stay for the next time Custom is chosen.
    Controller->SetCustomShot(Edited);
    Test->TestTrue(TEXT("Preset selectable"), Controller->SelectReplayShot(3));
    Test->TestFalse(TEXT("Preset leaves custom"), Controller->IsCustomShotSelected());
    Controller->InjectReplayShot();
    Monitor->GetLastShot(Shot);
    Test->TestEqual(TEXT("Preset hits again"), Shot.GetBallSpeedMph(), 75.7f, 0.02f);
    Test->TestFalse(TEXT("Bad preset rejected"), Controller->SelectReplayShot(999));
    Controller->SelectCustomShot();
    Test->TestTrue(TEXT("Bad preset keeps a custom selection"), !Controller->SelectReplayShot(-1) && Controller->IsCustomShotSelected());
    Controller->InjectReplayShot();
    Monitor->GetLastShot(Shot);
    Test->TestEqual(TEXT("Custom values kept across selections"), Shot.GetBallSpeedMph(), 120.0f, 0.001f);

    FOpenGolfPermissionTestAccess::Set(World, false);
    const int32 After = Mode->GetBall()->GetShotNumber();
    Controller->InjectReplayShot();
    Test->TestEqual(TEXT("Revoked permission prevents custom hits"), Mode->GetBall()->GetShotNumber(), After);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomShotTest, "UOpenGolf.Range.CustomShot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCustomShotTest::RunTest(const FString& Parameters)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
    {
        AddError(TEXT("Could not load the range map."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FVerifyCustomShot(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
