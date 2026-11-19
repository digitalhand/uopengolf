// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Editor.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/PermissionTestAccess.h"
#include "Tests/RenderedPlay.h"
#include "UI/Hud/ReplayShotPanel.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UnrealClient.h"
#include "Widgets/Input/SEditableTextBox.h"

/**
 * Uses the Test shots panel's Custom shot as a player would: Custom shows the straight-shot defaults, typing into a
 * field never presses a play key (H, Tab), Enter stores the typed value, text that is not a number is put back, and
 * the hit button hits the typed shot. Captures the open custom panel to Saved/Screenshots/CustomShotPanel.
 */
class FCustomShotPanelCapture : public IAutomationLatentCommand
{
public:
    explicit FCustomShotPanelCapture(FAutomationTestBase* InTest) : Test(InTest) {}

    bool Update() override
    {
        if (GetCurrentRunTime() > 60.0)
        {
            Test->AddError(FString::Printf(TEXT("Custom shot panel test timed out at stage %d"), Stage));
            FScreenshotRequest::Reset();
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        Player = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        if (!Player || !Mode || !Mode->GetBall() || !World->GetGameViewport() || !World->GetGameViewport()->GetWindow() || GetCurrentRunTime() < WaitUntil)
            return false;
        Window = World->GetGameViewport()->GetWindow();
        Monitor = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
        if (!File.IsEmpty())
        {
            if (!IFileManager::Get().FileExists(*File))
                return false;
            Test->AddInfo(TEXT("Captured ") + File);
            File.Empty();
            ++Stage;
        }
        return Step();
    }

private:
    bool Wait(double Seconds)
    {
        WaitUntil = GetCurrentRunTime() + Seconds;
        return false;
    }

    TSharedPtr<SEditableTextBox> Field(int32 Index) const
    {
        return StaticCastSharedPtr<SEditableTextBox>(RenderedPlay::WithTag(Window.ToSharedRef(), SReplayShotPanel::CustomFieldTag(Index)));
    }

    static void Press(const FKey& Key, bool bShift = false)
    {
        const FModifierKeysState Modifiers(bShift, false, false, false, false, false, false, false, false);
        FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(Key, Modifiers, 0, false, 0, 0));
        FSlateApplication::Get().ProcessKeyUpEvent(FKeyEvent(Key, Modifiers, 0, false, 0, 0));
    }

    static void Type(const FString& Text)
    {
        for (const TCHAR Character : Text)
            FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(Character, FModifierKeysState(), 0, false));
    }

    /** Runs the current stage; true when the test is finished. */
    bool Step()
    {
        switch (Stage)
        {
        case 0:
            FOpenGolfPermissionTestAccess::Set(Player->GetWorld());
            Player->SetReplayPanelOpen(true);
            Stage = 1;
            return Wait(1.5);
        case 1:
            if (!Test->TestTrue(TEXT("Panel open"), Player->IsReplayPanelOpen()) || !RenderedPlay::ClickLabelled(*Test, TEXT("Custom")))
                return true;
            Stage = 2;
            return Wait(1.0);
        case 2:
        {
            Test->TestTrue(TEXT("Custom selected by its button"), Player->IsCustomShotSelected());
            // Automation never reads the player's preferences, so speeds are in mph.
            const TCHAR* const Defaults[] = {TEXT("100"), TEXT("16"), TEXT("0"), TEXT("5000"), TEXT("0")};
            for (int32 Index = 0; Index < SReplayShotPanel::CustomFieldCount; ++Index)
            {
                const TSharedPtr<SEditableTextBox> Box = Field(Index);
                // Laid out with a size only once Custom shows the fields.
                if (!Test->TestTrue(FString::Printf(TEXT("Field %d shown"), Index), Box.IsValid() && Box->GetTickSpaceGeometry().GetLocalSize().X > 0.0))
                    return true;
                Test->TestEqual(FString::Printf(TEXT("Field %d default"), Index), Box->GetText().ToString(), FString(Defaults[Index]));
            }
            Test->TestTrue(TEXT("Hit button names the custom shot"), RenderedPlay::FindButton(Window.ToSharedRef(), TEXT("Hit custom shot [H]")).IsValid());
            Test->TestTrue(TEXT("Dropdown reads Custom shot"), RenderedPlay::HasText(Window.ToSharedRef(), TEXT("Custom shot")));
            RenderedPlay::ExpectNoToolTips(*Test, Window.ToSharedRef(), TEXT("the custom test shot"));
            File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() /
                FString::Printf(TEXT("Screenshots/CustomShotPanel/%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
            FScreenshotRequest::RequestScreenshot(File, true, false, false, FIntRect(), true);
            return false;
        }
        case 3:
        {
            // Typing: H and Tab stay in the panel, so no shot and no club change.
            const TSharedPtr<SEditableTextBox> Speed = Field(0);
            FSlateApplication::Get().SetKeyboardFocus(Speed, EFocusCause::SetDirectly);
            if (!Test->TestTrue(TEXT("Speed field takes focus"), Speed->HasKeyboardFocus()))
                return true;
            const int32 ShotsBefore = Mode->GetBall()->GetShotNumber();
            const ELaunchMonitorClub ClubBefore = Monitor->GetClub();
            Press(EKeys::H);
            Test->TestEqual(TEXT("H in a field does not hit"), Mode->GetBall()->GetShotNumber(), ShotsBefore);
            Test->TestTrue(TEXT("H in a field leaves the panel open"), Player->IsReplayPanelOpen());
            Press(EKeys::Tab);
            Test->TestTrue(TEXT("Tab moves to the launch field"), Field(1)->HasKeyboardFocus());
            Test->TestTrue(TEXT("Tab in a field keeps the club"), Monitor->GetClub() == ClubBefore);
            Press(EKeys::Tab, true);
            Test->TestTrue(TEXT("Shift+Tab moves back"), Speed->HasKeyboardFocus());

            Speed->SetText(FText::GetEmpty());
            Type(TEXT("120"));
            Press(EKeys::Enter);
            Test->TestEqual(TEXT("Enter stores the typed speed"), Player->GetCustomShot().BallSpeedMps * LaunchMonitorMphPerMps, 120.0f, 0.001f);
            Test->TestFalse(TEXT("Enter leaves the field"), Speed->HasKeyboardFocus());
            Test->TestEqual(TEXT("Field shows the stored speed"), Speed->GetText().ToString(), FString(TEXT("120")));

            const TSharedPtr<SEditableTextBox> Spin = Field(3);
            FSlateApplication::Get().SetKeyboardFocus(Spin, EFocusCause::SetDirectly);
            Spin->SetText(FText::GetEmpty());
            Type(TEXT("fast"));
            Press(EKeys::Enter);
            Test->TestEqual(TEXT("Text that is not a number keeps the spin"), Player->GetCustomShot().TotalSpinRpm, 5000);
            Test->TestEqual(TEXT("Spin field put back"), Spin->GetText().ToString(), FString(TEXT("5000")));

            const TSharedPtr<SEditableTextBox> Axis = Field(4);
            FSlateApplication::Get().SetKeyboardFocus(Axis, EFocusCause::SetDirectly);
            Axis->SetText(FText::GetEmpty());
            Type(TEXT("-7.5"));
            Press(EKeys::Enter);
            Test->TestEqual(TEXT("Negative axis typed"), Player->GetCustomShot().SpinAxisDeg, -7.5f);
            Stage = 4;
            return Wait(0.5);
        }
        case 4:
            Hits = Mode->GetBall()->GetShotNumber();
            if (!RenderedPlay::ClickLabelled(*Test, TEXT("Hit custom shot [H]")))
                return true;
            Stage = 5;
            return Wait(0.5);
        case 5:
        {
            FLaunchMonitorShot Shot;
            Test->TestTrue(TEXT("Hit button hits the custom shot"), Monitor->GetLastShot(Shot) && Shot.Source == LaunchMonitorCustomSource);
            Test->TestEqual(TEXT("Typed speed hit"), Shot.GetBallSpeedMph(), 120.0f, 0.001f);
            Test->TestEqual(TEXT("Typed axis hit"), Shot.SpinAxisDeg, -7.5f);
            Test->TestEqual(TEXT("One hit"), Mode->GetBall()->GetShotNumber(), Hits + 1);
            Test->TestFalse(TEXT("Hit closes the panel"), Player->IsReplayPanelOpen());
            FOpenGolfPermissionTestAccess::Set(Player->GetWorld(), false);
            return true;
        }
        default:
            return true;
        }
    }

    FAutomationTestBase* Test;
    AUOpenGolfPlayerController* Player = nullptr;
    AUOpenGolfGameMode* Mode = nullptr;
    UOpenLaunchMonitorSubsystem* Monitor = nullptr;
    TSharedPtr<SWindow> Window;
    int32 Stage = 0;
    int32 Hits = 0;
    double WaitUntil = 0;
    FString File;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCustomShotPanelVisualTest, "UOpenGolf.Visual.CustomShotPanel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FCustomShotPanelVisualTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FCustomShotPanelCapture>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
