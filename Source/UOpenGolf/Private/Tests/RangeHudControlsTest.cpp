// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/RenderedPlay.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"
#include "OpenGolfTerrain.h"
#include "UI/Hud/BallReadyGlow.h"
#include "GameFramework/HUD.h"
#include "Engine/GameInstance.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace RangeHudControlsTestDetail
{
    /** The range HUD's clickable controls, by their Slate tags. */
    const TCHAR* const HudControls[] = {TEXT("ClubPicker"), TEXT("RecordToggle"), TEXT("LaunchMonitorStatus")};

    bool ShowsText(const TSharedRef<SWidget>& Root, const FString& Value)
    {
        if (Root->GetTypeAsString() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Root)->GetText().ToString() == Value)
            return true;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (ShowsText(Children->GetChildAt(Index), Value))
                return true;
        }
        return false;
    }

    /** Whether a real click at the widget's centre would reach it, as Slate routes it. */
    bool Reachable(const TSharedRef<SWindow>& Window, const SWidget& Widget, FString& OutHitPath)
    {
        const FWidgetPath Path = RenderedPlay::PathAt(Window, RenderedPlay::PointOn(Widget));
        for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
            OutHitPath += Path.Widgets[Index].Widget->GetTypeAsString() + TEXT(" ");
        return Path.IsValid() && Path.ContainsWidget(&Widget);
    }

    /** A widget's last laid-out size in its own units, before the readable-DPI scale; zero when it is missing. */
    FVector2D SizeOf(const TSharedRef<SWindow>& Window, FName Tag)
    {
        const TSharedPtr<SWidget> Widget = RenderedPlay::WithTag(Window, Tag);
        return Widget ? Widget->GetTickSpaceGeometry().GetLocalSize() : FVector2D::ZeroVector;
    }

    bool IsShown(const TSharedRef<SWindow>& Window, FName Tag)
    {
        const TSharedPtr<SWidget> Widget = RenderedPlay::WithTag(Window, Tag);
        return Widget && Widget->GetVisibility().IsVisible();
    }

    const TCHAR* const PlacingHeading = TEXT("Practice  ·  Ball placement");
} // namespace RangeHudControlsTestDetail

/**
 * Clicks the range HUD's controls as a player would: the club picker and both status buttons must be reachable,
 * the record button toggles recording and the launch-monitor button opens Settings › Simulator. Also checks that
 * changing clubs never resizes the shot tiles, that the ball-ready glow shows at address, that placing a ball
 * retitles the hole and hides its tiles, that recording and the glow stay off the hole where they do not apply, and
 * that nothing has a tooltip.
 */
class FRangeHudControlsCapture : public IAutomationLatentCommand
{
public:
    explicit FRangeHudControlsCapture(FAutomationTestBase* InTest) : Test(InTest) {}

    bool Update() override
    {
        if (GetCurrentRunTime() > 150)
        {
            Test->AddError(FString::Printf(TEXT("Range HUD controls test timed out at stage %d"), Stage));
            FScreenshotRequest::Reset();
            RemoveReadyGlow();
            return true;
        }
        UWorld* World = GEditor->PlayWorld;
        Player = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
        Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        if (!Player || !Mode || !World->GetGameViewport() || !World->GetGameViewport()->GetWindow() || GetCurrentRunTime() < WaitUntil)
            return false;
        Window = World->GetGameViewport()->GetWindow();
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
    /** Runs the current stage; true when the test is finished. */
    bool Step()
    {
        using namespace RangeHudControlsTestDetail;
        switch (Stage)
        {
        case 0:
            // Training with the HUD shown; a startup menu is closed through its own Escape handler.
            if (Player->IsMainMenuOpen())
            {
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                return Wait(1.0);
            }
            Stage = 1;
            return Wait(2.0);
        case 1:
            for (const TCHAR* Tag : HudControls)
            {
                const TSharedPtr<SWidget> Control = RenderedPlay::WithTag(Window.ToSharedRef(), Tag);
                FString HitPath;
                Test->TestTrue(FString::Printf(TEXT("%s is shown"), Tag), Control.IsValid());
                if (Control)
                {
                    const bool bReached = Reachable(Window.ToSharedRef(), *Control, HitPath);
                    Test->TestTrue(FString::Printf(TEXT("A click reaches %s; hit path: %s"), Tag, *HitPath), bReached);
                }
            }
            RenderedPlay::ExpectNoToolTips(*Test, Window.ToSharedRef(), TEXT("the range HUD"));
            Test->TestTrue(TEXT("The ball is at address"), Mode->IsBallAtAddress());
            Test->TestTrue(TEXT("The ball-ready glow shows at address"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            // The longest club name used to widen the heading row, and with it every tile.
            HeadingSize = SizeOf(Window.ToSharedRef(), TEXT("ShotHeading"));
            TileSize = SizeOf(Window.ToSharedRef(), TEXT("ShotTile"));
            Test->TestTrue(TEXT("The shot tiles are laid out"), TileSize.X > 0 && HeadingSize.X > 0);
            if (UOpenLaunchMonitorSubsystem* Monitor = Player->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
            {
                StartClub = Monitor->GetClub();
                Monitor->SetClub(ELaunchMonitorClub::PitchingWedge);
            }
            Stage = 2;
            return Wait(1.0);
        case 2:
            Test->TestTrue(FString::Printf(TEXT("A club change keeps the heading width: %s, then %s"), *HeadingSize.ToString(),
                *SizeOf(Window.ToSharedRef(), TEXT("ShotHeading")).ToString()), SizeOf(Window.ToSharedRef(), TEXT("ShotHeading")).Equals(HeadingSize, 0.5));
            Test->TestTrue(FString::Printf(TEXT("A club change keeps the tile size: %s, then %s"), *TileSize.ToString(),
                *SizeOf(Window.ToSharedRef(), TEXT("ShotTile")).ToString()), SizeOf(Window.ToSharedRef(), TEXT("ShotTile")).Equals(TileSize, 0.5));
            Test->TestTrue(TEXT("The club picker shows PW"), ShowsText(Window.ToSharedRef(), TEXT("PW")));
            return Capture(TEXT("hud-pw"));
        case 3:
            if (UOpenLaunchMonitorSubsystem* Monitor = Player->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
                Monitor->SetClub(StartClub);
            Stage = 4;
            return Wait(1.0);
        case 4:
            return Capture(TEXT("ball-not-ready"));
        case 5:
            // No launch monitor reports a ball here, so the green state is drawn by a second glow over the HUD-less view.
            ReadyGlow = SNew(SBallReadyGlow).Controller(Player).Ready(true);
            PlayWorld()->GetGameViewport()->AddViewportWidgetContent(ReadyGlow.ToSharedRef(), 100);
            Player->GetHUD()->bShowHUD = false;
            Stage = 6;
            return Wait(1.0);
        case 6:
            return Capture(TEXT("ball-ready"));
        case 7:
            if (ReadyGlow)
            {
                // The HUD has to lay out again before its buttons can be clicked where they are drawn.
                RemoveReadyGlow();
                return Wait(1.0);
            }
            Test->TestFalse(TEXT("Recording starts off"), Mode->IsRecordingShots());
            return ClickThen(TEXT("RecordToggle"), 8);
        case 8:
            Test->TestTrue(TEXT("The record button starts recording"), Mode->IsRecordingShots());
            return Capture(TEXT("hud-recording"));
        case 9:
            return ClickThen(TEXT("RecordToggle"), 10);
        case 10:
            Test->TestFalse(TEXT("The record button stops recording"), Mode->IsRecordingShots());
            return ClickThen(TEXT("LaunchMonitorStatus"), 11);
        case 11:
            Test->TestTrue(TEXT("The launch-monitor button opens the menu"), Player->IsMainMenuOpen());
            Test->TestTrue(TEXT("The menu shows Settings › Simulator"), ShowsText(Window.ToSharedRef(), TEXT("Launch monitor connection")));
            RenderedPlay::ExpectNoToolTips(*Test, Window.ToSharedRef(), TEXT("Settings › Simulator"));
            return Capture(TEXT("lm-settings"));
        case 12:
            // Back to the pause menu, then resume Training, through the menu's own Escape handling.
            if (Player->IsMainMenuOpen())
            {
                if (++Escapes > 4)
                {
                    Test->AddError(TEXT("Escape did not close the menu"));
                    return true;
                }
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                return Wait(1.0);
            }
            Mode->SetPlayMode(EOpenGolfPlayMode::Hole);
            Player->TogglePlacingBall();
            Stage = 13;
            return Wait(1.5);
        case 13:
            Test->TestTrue(TEXT("B starts placing the ball"), Player->IsPlacingBall());
            Test->TestTrue(TEXT("Placing retitles the hole"), ShowsText(Window.ToSharedRef(), PlacingHeading));
            Test->TestFalse(TEXT("The glow hides while placing"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            Test->TestFalse(TEXT("Stroke, To Pin and Putts hide while placing"), IsShown(Window.ToSharedRef(), TEXT("HoleTiles")));
            Test->TestFalse(TEXT("The hole has no record toggle"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            RenderedPlay::ExpectNoToolTips(*Test, Window.ToSharedRef(), TEXT("the hole HUD"));
            return Capture(TEXT("placing"));
        case 14:
            Player->CancelPlacingBall();
            Stage = 15;
            return Wait(1.0);
        case 15:
            // A normal hole: the tiles and the glow are back; recording stays a range control, and F9 is ignored.
            Test->TestTrue(TEXT("The hole tiles show on a normal hole"), IsShown(Window.ToSharedRef(), TEXT("HoleTiles")));
            Test->TestTrue(TEXT("The glow shows at address on the hole"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            Test->TestFalse(TEXT("Still no record toggle on the hole"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            {
                const bool bWasRecording = Mode->IsRecordingShots();
                Player->ToggleRecordShots();
                Test->TestEqual(TEXT("F9 does not change recording on the hole"), Mode->IsRecordingShots(), bWasRecording);
            }
            {
                // A practice hole: a ball placed on the green, as HolePlayTest places one.
                FVector Ground;
                FVector Normal;
                const bool bGround = Mode->GetRange() && Mode->GetRange()->Terrain &&
                    Mode->GetRange()->Terrain->GroundAt(Mode->GetCupLocation() - FVector(300, 0, 0), Ground, Normal);
                Test->TestTrue(TEXT("A ball is placed on the green"), bGround && Mode->PlaceBallAt(Ground));
            }
            Stage = 16;
            return Wait(1.5);
        case 16:
            Test->TestTrue(TEXT("A placed ball makes the hole practice"), Mode->IsPractice());
            Test->TestTrue(TEXT("The practice hole is titled"), ShowsText(Window.ToSharedRef(), PlacingHeading));
            Test->TestFalse(TEXT("Stroke, To Pin and Putts hide on a practice hole"), IsShown(Window.ToSharedRef(), TEXT("HoleTiles")));
            Test->TestFalse(TEXT("No glow on a practice hole"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            Test->TestFalse(TEXT("No record toggle on a practice hole"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            return Capture(TEXT("practice"));
        case 17:
            Mode->SetPlayMode(EOpenGolfPlayMode::Range);
            Stage = 18;
            return Wait(1.0);
        case 18:
            Test->TestFalse(TEXT("The range is not practice"), Mode->IsPractice());
            Test->TestTrue(TEXT("The record toggle is back on the range"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            Test->TestTrue(TEXT("The glow is back on the range"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            return true;
        default:
            return true;
        }
    }

    UWorld* PlayWorld() const { return GEditor->PlayWorld; }

    /** Removes the green test glow and brings the HUD back. */
    void RemoveReadyGlow()
    {
        if (ReadyGlow && PlayWorld() && PlayWorld()->GetGameViewport())
            PlayWorld()->GetGameViewport()->RemoveViewportWidgetContent(ReadyGlow.ToSharedRef());
        ReadyGlow.Reset();
        if (Player && Player->GetHUD())
            Player->GetHUD()->bShowHUD = true;
    }

    bool Wait(double Seconds)
    {
        WaitUntil = GetCurrentRunTime() + Seconds;
        return false;
    }

    bool ClickThen(const TCHAR* Tag, int32 NextStage)
    {
        const TSharedPtr<SWidget> Control = RenderedPlay::WithTag(Window.ToSharedRef(), Tag);
        if (!Control)
        {
            Test->AddError(FString(TEXT("Missing HUD control: ")) + Tag);
            return true;
        }
        RenderedPlay::Click(Window.ToSharedRef(), RenderedPlay::PointOn(*Control));
        Stage = NextStage;
        return Wait(1.0);
    }

    bool Capture(const TCHAR* Name)
    {
        File = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Screenshots/RangeHud") /
            FString::Printf(TEXT("%s-%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")), Name));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FScreenshotRequest::RequestScreenshot(File, true, false);
        return false;
    }

    FAutomationTestBase* Test;
    AUOpenGolfPlayerController* Player = nullptr;
    AUOpenGolfGameMode* Mode = nullptr;
    TSharedPtr<SWindow> Window;
    int32 Stage = 0;
    int32 Escapes = 0;
    double WaitUntil = 0;
    FString File;
    FVector2D HeadingSize = FVector2D::ZeroVector;
    FVector2D TileSize = FVector2D::ZeroVector;
    ELaunchMonitorClub StartClub = ELaunchMonitorClub::Driver;
    TSharedPtr<SBallReadyGlow> ReadyGlow;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRangeHudControlsTest, "UOpenGolf.Visual.RangeHudControls",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FRangeHudControlsTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FRangeHudControlsCapture>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
