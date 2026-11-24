// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/RenderedPlay.h"
#include "Training/DistanceLadder.h"
#include "Training/DistancePractice.h"
#include "Tests/BundledCourse.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfHUD.h"
#include "UOpenGolfRange.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfSettings.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/HudToolTip.h"
#include "UI/Hud/BallReadyGlow.h"
#include "UI/Hud/ShotDisplay.h"
#include "GameFramework/HUD.h"
#include "Widgets/IToolTip.h"
#include "Widgets/SToolTip.h"
#include "Engine/GameInstance.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "OpenGolfOnlineSubsystem.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace RangeHudControlsTestDetail
{
    /** The range HUD's clickable controls, by their Slate tags. */
    const TCHAR* const HudControls[] = {TEXT("TrainingBack"), TEXT("TrainingGames"), TEXT("ClubPicker"), TEXT("PlaceBall"), TEXT("RecordToggle"), TEXT("HudSettings")};
    /** The range toolbar's icons, each named by a hover tooltip in the HUD style. */
    const TCHAR* const HudToolTipControls[] = {TEXT("TrainingBack"), TEXT("ClubPicker"), TEXT("PlaceBall"), TEXT("RecordToggle")};
    /** The toolbar tooltips that name a control, by tag. */
    const TPair<const TCHAR*, const TCHAR*> HudToolTipTexts[] = {
        {TEXT("PlaceBall"), TEXT("Place ball (B)")}, {TEXT("ClubPicker"), TEXT("Change Club")}, {TEXT("RecordToggle"), TEXT("Record Shots")}};

    /** The first tooltip Slate would show under Root: the club picker's sits on its inner combo button. */
    TSharedPtr<IToolTip> ShownToolTip(const TSharedRef<SWidget>& Root)
    {
        const TSharedPtr<IToolTip> Tip = Root->GetToolTip();
        if (Tip.IsValid() && !Tip->IsEmpty())
            return Tip;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
            if (const TSharedPtr<IToolTip> Found = ShownToolTip(Children->GetChildAt(Index)))
                return Found;
        return nullptr;
    }

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
        for (TSharedPtr<SWidget> Widget = RenderedPlay::WithTag(Window, Tag); Widget; Widget = Widget->GetParentWidget())
        {
            if (!Widget->GetVisibility().IsVisible()) return false;
            if (Widget.Get() == &Window.Get()) return true;
        }
        return false;
    }

    /** Whether the widget tagged Tag is shown in any visible window, such as an open drop-down. */
    bool IsShownInAnyWindow(FName Tag)
    {
        TArray<TSharedRef<SWindow>> Windows;
        FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
        return Windows.ContainsByPredicate([Tag](const TSharedRef<SWindow>& Each) { return IsShown(Each, Tag); });
    }

    const TCHAR* const PlacingHeading = TEXT("Training · Ball Placement");
} // namespace RangeHudControlsTestDetail

/**
 * Clicks the range HUD's controls as a player would: the club picker and toolbar and settings buttons must be reachable,
 * the record button toggles recording and the cog opens Settings › Player. Also checks that
 * changing clubs never resizes the shot tiles, that the ball-ready glow shows at address, that placing a ball
 * retitles the hole and hides its tiles, that recording and the glow stay off the hole where they do not apply.
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
            RestorePreferences();
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
            Test->TestTrue(TEXT("Training header shows"), ShowsText(Window.ToSharedRef(), TEXT("Training")));
            // The version label is the version alone: ShowsText matches whole text, so no key hint follows it.
            if (const FString Version = ShotDisplay::VersionLabel(ShotDisplay::ProjectVersion()); !Version.IsEmpty())
                Test->TestTrue(TEXT("The version label shows only the version"), ShowsText(Window.ToSharedRef(), Version));
            for (const TCHAR* Tag : HudToolTipControls)
            {
                const TSharedPtr<SWidget> Control = RenderedPlay::WithTag(Window.ToSharedRef(), Tag);
                const TSharedPtr<IToolTip> Tip = Control ? ShownToolTip(Control.ToSharedRef()) : nullptr;
                Test->TestTrue(FString::Printf(TEXT("%s has a HUD-styled tooltip"), Tag),
                    Tip.IsValid() && Tip->AsWidget()->GetTag() == HudToolTip::Tag);
            }
            for (const auto& [Tag, Expected] : HudToolTipTexts)
            {
                const TSharedPtr<SWidget> Control = RenderedPlay::WithTag(Window.ToSharedRef(), Tag);
                const TSharedPtr<IToolTip> Tip = Control ? ShownToolTip(Control.ToSharedRef()) : nullptr;
                Test->TestEqual(FString::Printf(TEXT("%s tooltip text"), Tag),
                    Tip.IsValid() ? StaticCastSharedRef<SToolTip>(Tip->AsWidget())->GetTextTooltip().ToString() : FString(),
                    FString(Expected));
            }
            {
                // The owner wants no tooltip on the training games button.
                const TSharedPtr<SWidget> Games = RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("TrainingGames"));
                TArray<FString> WithTips;
                if (Games)
                    RenderedPlay::WidgetsWithToolTips(Games.ToSharedRef(), WithTips, false);
                Test->TestTrue(TEXT("Training games has no tooltip"), Games.IsValid() && WithTips.IsEmpty());
            }
            Test->TestFalse(TEXT("Training omits the course toggle"), RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("GameModeToggle")).IsValid());
            Test->TestFalse(TEXT("The recording icon has no visible caption"), ShowsText(Window.ToSharedRef(), TEXT("Record")));
            {
                // The club button also names the club, so it is wider than the icon buttons but as tall.
                const FVector2D ClubSize = SizeOf(Window.ToSharedRef(), TEXT("ClubPicker"));
                const FVector2D IconSize = SizeOf(Window.ToSharedRef(), TEXT("PlaceBall"));
                Test->TestTrue(TEXT("Icon buttons have equal sizes"), IconSize.Equals(SizeOf(Window.ToSharedRef(), TEXT("RecordToggle")), 0.5));
                Test->TestEqual(TEXT("The club button is as tall as the icon buttons"), ClubSize.Y, IconSize.Y, 0.5);
                Test->TestTrue(TEXT("The club button is wider for the club's name"), ClubSize.X > IconSize.X + 10);
                ClubButtonSize = ClubSize;
            }
            Test->TestTrue(TEXT("The ball is at address"), Mode->IsBallAtAddress());
            Test->TestTrue(TEXT("The ball-ready glow shows at address"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            // The longest club name used to widen the heading row, and with it every tile.
            HeadingSize = SizeOf(Window.ToSharedRef(), TEXT("ShotHeading"));
            TileSize = SizeOf(Window.ToSharedRef(), TEXT("ShotTile"));
            Test->TestTrue(TEXT("The shot tiles are laid out"), TileSize.X > 0 && HeadingSize.X > 0);
            if (UOpenLaunchMonitorSubsystem* Monitor = Player->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
            {
                StartClub = Monitor->GetClub();
                Monitor->SetClub(ELaunchMonitorClub::Iron7);
            }
            return ClickThen(TEXT("ClubPicker"), 20);
        case 20:
            Test->TestTrue(TEXT("The club drawer opens"), FSlateApplication::Get().AnyMenusVisible());
            return Capture(TEXT("club-drawer"));
        case 21:
            if (!RenderedPlay::ClickLabelled(*Test, TEXT("8i"))) return true;
            Stage = 2;
            return Wait(0.5);
        case 2:
            Test->TestFalse(TEXT("Selecting a club closes the drawer"), FSlateApplication::Get().AnyMenusVisible());
            if (const UOpenLaunchMonitorSubsystem* Monitor = Player->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
                Test->TestEqual(TEXT("Drawer selection updates the monitor's club"), Monitor->GetClub(), ELaunchMonitorClub::Iron8);
            Test->TestTrue(FString::Printf(TEXT("A club change keeps the heading width: %s, then %s"), *HeadingSize.ToString(),
                *SizeOf(Window.ToSharedRef(), TEXT("ShotHeading")).ToString()), SizeOf(Window.ToSharedRef(), TEXT("ShotHeading")).Equals(HeadingSize, 0.5));
            Test->TestTrue(FString::Printf(TEXT("A club change keeps the tile size: %s, then %s"), *TileSize.ToString(),
                *SizeOf(Window.ToSharedRef(), TEXT("ShotTile")).ToString()), SizeOf(Window.ToSharedRef(), TEXT("ShotTile")).Equals(TileSize, 0.5));
            if (const TSharedPtr<SWidget> Name = RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("ClubPickerName")))
            {
                const TSharedRef<STextBlock> Label = StaticCastSharedRef<STextBlock>(Name.ToSharedRef());
                Test->TestEqual(TEXT("The closed club picker shows the selected club's short label"), Label->GetText().ToString(), FString(TEXT("8i")));
                Test->TestEqual(TEXT("The club label matches the course tiles' number size"), Label->GetFont().Size, HudStyle::ValueFont().Size);
            }
            else
                Test->AddError(TEXT("Missing the club picker's label"));
            Test->TestTrue(FString::Printf(TEXT("A club change keeps the club button's size: %s, then %s"), *ClubButtonSize.ToString(),
                *SizeOf(Window.ToSharedRef(), TEXT("ClubPicker")).ToString()), SizeOf(Window.ToSharedRef(), TEXT("ClubPicker")).Equals(ClubButtonSize, 0.5));
            return ClickThen(TEXT("ClubPicker"), 22);
        case 22:
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
            Stage = 23;
            return Wait(0.5);
        case 23:
            Test->TestFalse(TEXT("Escape dismisses the club drawer"), FSlateApplication::Get().AnyMenusVisible());
            Test->TestFalse(TEXT("Escape from the drawer keeps the game in Training"), Player->IsMainMenuOpen());
            Stage = 2;
            return Capture(TEXT("hud-club"));
        case 3:
            if (UOpenLaunchMonitorSubsystem* Monitor = Player->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
                Monitor->SetClub(StartClub);
            return ClickThen(TEXT("TrainingGames"), 40);
        case 40:
            {
                TArray<TSharedRef<SWindow>> Windows;
                FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
                const bool bListed = Windows.ContainsByPredicate([](const TSharedRef<SWindow>& Menu) {
                    return RenderedPlay::HasText(Menu, TEXT("Distance Practice")) && RenderedPlay::HasText(Menu, TEXT("Distance Ladder"));
                });
                Test->TestTrue(TEXT("Games lists both target games"), bListed);
            }
            return ActivateWithKeyboard(TEXT("TrainingRadius.6"), 49);
        case 49:
            Test->TestFalse(TEXT("Selecting a radius does not start a drill"), Mode->IsDrillActive());
            // Distance Practice has no shot limit, so it offers no tries.
            Test->TestTrue(TEXT("The picker shows the radius choices"), IsShownInAnyWindow(TEXT("TrainingRadius.6")));
            Test->TestFalse(TEXT("Distance Practice hides the tries"), IsShownInAnyWindow(TEXT("TrainingTries.5")));
            return ActivateWithKeyboard(TEXT("TrainingStart"), 41);
        case 41:
            if (!Test->TestTrue(TEXT("Games starts Distance Practice"), Mode->IsDrillActive())) return true;
            Test->TestEqual(TEXT("Practice starts with a 6 ft ring"), CastChecked<UDistancePractice>(Mode->GetTrainingSession()->Active())->GetRadiusFeet(), 6);
            if (auto Slider = RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("TrainingDistance")))
            {
                FSlateApplication::Get().SetKeyboardFocus(Slider);
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_DPad_Right, FModifierKeysState(), 0, false, 0, 0));
                Test->TestEqual(TEXT("Controller moves the real slider 5 yd"), CastChecked<UDistancePractice>(Mode->GetTrainingSession()->Active())->GetDistance(), 30);
            }
            else Test->AddError(TEXT("The drill slider is missing"));
            return Capture(TEXT("distance-practice"));
        case 42:
            // A game chosen while another runs replaces it.
            return ClickThen(TEXT("TrainingGames"), 46);
        case 46:
            if (!RenderedPlay::ClickTagged(*Test, TEXT("TrainingGame.DistanceLadder"))) return true;
            if (!RenderedPlay::ClickTagged(*Test, TEXT("TrainingRadius.10"))) return true;
            Test->TestFalse(TEXT("Changing setup does not open a switch prompt"), Player->IsDrillDialogOpen());
            Stage = 69;
            return Wait(.25);
        case 69:
            {
                TArray<TSharedRef<SWindow>> Windows;
                FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
                for (const auto& Menu : Windows)
                    if (RenderedPlay::WithTag(Menu, TEXT("TrainingSetup")))
                    {
                        RenderedPlay::SaveWindowPng(*Test, Menu, TEXT("RangeHud/training-game-picker.png"));
                        break;
                    }
            }
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
            Stage = 71;
            return Wait(.25);
        case 71:
            Test->TestFalse(TEXT("Escape closes the training picker"), FSlateApplication::Get().AnyMenusVisible());
            Test->TestFalse(TEXT("Escape leaves the active run alone"), Player->IsDrillDialogOpen());
            return ClickThen(TEXT("TrainingGames"), 72);
        case 72:
            if (!RenderedPlay::ClickTagged(*Test, TEXT("TrainingGame.DistanceLadder"))) return true;
            if (!RenderedPlay::ClickTagged(*Test, TEXT("TrainingRadius.10"))) return true;
            Stage = 75;
            return Wait(.25);
        case 75:
            // The tries row lays out on the frame after the Ladder is selected.
            Test->TestTrue(TEXT("The Ladder offers tries"), IsShownInAnyWindow(TEXT("TrainingTries.5")));
            if (!RenderedPlay::ClickTagged(*Test, TEXT("TrainingTries.5"))) return true;
            Test->TestTrue(TEXT("Selecting the tries keeps Distance Practice running"),
                Cast<UDistancePractice>(Mode->GetTrainingSession()->Active()) != nullptr);
            return ActivateWithKeyboard(TEXT("TrainingStart"), 47);
        case 47:
            // Picking the other game asks first, through the Exit Drill prompt.
            Test->TestTrue(TEXT("Picking the Ladder opens the Exit Drill prompt"), Player->IsDrillDialogOpen());
            Test->TestTrue(TEXT("Distance Practice runs until the player exits"),
                Cast<UDistancePractice>(Mode->GetTrainingSession()->Active()) != nullptr);
            Test->TestTrue(TEXT("The prompt says which game Exit Drill starts"),
                ShowsText(Window.ToSharedRef(), TEXT("Exit Drill to start Distance Ladder with a 10 ft ring and 5 tries.")));
            if (!RenderedPlay::ClickLabelled(*Test, TEXT("Exit Drill"))) return true;
            Stage = 48;
            return Wait(1.0);
        case 48:
            Test->TestFalse(TEXT("Exit Drill closes the prompt"), Player->IsDrillDialogOpen());
            Test->TestTrue(TEXT("Exit Drill starts the Distance Ladder"), Cast<UDistanceLadder>(Mode->GetTrainingSession()->Active()) != nullptr);
            Test->TestFalse(TEXT("The Distance Practice panel closes"), RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("TrainingDistance")).IsValid());
            Test->TestTrue(TEXT("The Ladder panel shows"), ShowsText(Window.ToSharedRef(), TEXT("Distance Ladder")));
            if (const auto* Ladder = Cast<UDistanceLadder>(Mode->GetTrainingSession()->Active()))
            {
                Test->TestEqual(TEXT("The new drill gets the selected radius"), Ladder->GetRadiusFeet(), 10);
                Test->TestEqual(TEXT("The new drill gets the selected tries"), Ladder->GetTries(), 5);
            }
            Stage = 73;
            return Capture(TEXT("distance-ladder-10ft"));
        case 74:
            return ClickThen(TEXT("TrainingBack"), 43);
        case 43:
            Test->TestTrue(TEXT("X opens the drill dialog"), Player->IsDrillDialogOpen());
            {
                auto RestartButton = RenderedPlay::FindButton(Window.ToSharedRef(), TEXT("Restart Drill"));
                auto& App = FSlateApplication::Get();
                App.ProcessKeyDownEvent(FKeyEvent(EKeys::Gamepad_DPad_Right, FModifierKeysState(), 0, false, 0, 0));
                Test->TestTrue(TEXT("D-pad focuses Restart Drill"), RestartButton && RestartButton->HasKeyboardFocus());
            }
            if (!RenderedPlay::ClickLabelled(*Test, TEXT("Restart Drill"))) return true;
            Stage = 44;
            return Wait(.5);
        case 44:
            Test->TestTrue(TEXT("Restart returns to the drill"), Mode->IsDrillActive() && !Player->IsDrillDialogOpen());
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
            Stage = 45;
            return Wait(.5);
        case 45:
            Test->TestTrue(TEXT("Esc opens the same drill dialog"), Player->IsDrillDialogOpen());
            if (!RenderedPlay::ClickLabelled(*Test, TEXT("Exit Drill"))) return true;
            Test->TestFalse(TEXT("Exit returns to free Training"), Mode->IsDrillActive());
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
            return ActivateWithKeyboard(TEXT("RecordToggle"), 10);
        case 10:
            Test->TestFalse(TEXT("The record button stops recording"), Mode->IsRecordingShots());
            return ClickThen(TEXT("HudSettings"), 11);
        case 11:
            Test->TestTrue(TEXT("The cog opens the menu"), Player->IsMainMenuOpen());
            Test->TestTrue(TEXT("The menu shows Settings › Player"), ShowsText(Window.ToSharedRef(), TEXT("Handedness")));
            RenderedPlay::ExpectNoToolTips(*Test, Window.ToSharedRef(), TEXT("Settings › Player"));
            return Capture(TEXT("player-settings"));
        case 12:
            // Escape from Settings should return directly to the current Training session.
            if (Player->IsMainMenuOpen())
            {
                if (++Escapes > 1)
                {
                    Test->AddError(TEXT("Escape did not close the menu"));
                    return true;
                }
                FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
                return Wait(1.0);
            }
            Test->TestEqual(TEXT("Settings preserves the current mode"), Mode->GetPlayMode(), EOpenGolfPlayMode::Range);
            return ActivateWithKeyboard(TEXT("HudSettings"), 24);
        case 24:
            if (!RenderedPlay::ClickLabelled(*Test, GolfSettings::Get().LeftHanded ? TEXT("Left") : TEXT("Right")))
                return true;
            Stage = 25;
            return Wait(0.5);
        case 25:
            if (!RenderedPlay::ClickLabelled(*Test, GolfSettings::Get().LeftHanded ? TEXT("Right") : TEXT("Left")))
                return true;
            Stage = 26;
            return Wait(0.5);
        case 26:
            FSlateApplication::Get().ProcessKeyDownEvent(FKeyEvent(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0));
            Stage = 27;
            return Wait(0.5);
        case 27:
            Test->TestTrue(TEXT("Unsaved changes keep Settings open"), Player->IsMainMenuOpen());
            if (!RenderedPlay::ClickLabelled(*Test, TEXT("Discard and go back")))
                return true;
            Stage = 28;
            return Wait(0.5);
        case 28:
            Test->TestFalse(TEXT("Discard returns directly to training"), Player->IsMainMenuOpen());
            Test->TestEqual(TEXT("Discard keeps the range session"), Mode->GetPlayMode(), EOpenGolfPlayMode::Range);
            return ClickThen(TEXT("PlaceBall"), 13);
        case 13:
            Test->TestTrue(TEXT("B starts placing the ball"), Player->IsPlacingBall());
            Test->TestTrue(TEXT("Placing retitles the hole"), ShowsText(Window.ToSharedRef(), PlacingHeading));
            Test->TestFalse(TEXT("The glow hides while placing"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            Test->TestFalse(TEXT("Stroke, To Pin and Putts hide while placing"), IsShown(Window.ToSharedRef(), TEXT("HoleTiles")));
            Test->TestTrue(TEXT("Training keeps its record toggle"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            {
                // The pointer over the course is the HUD's ball icon while placing, not a system pointer.
                const UGameViewportClient* Viewport = Player->GetWorld()->GetGameViewport();
                const TSharedPtr<SWidget> Pointer = Viewport ? Viewport->GetSoftwareCursorWidget(Player->CurrentMouseCursor) : nullptr;
                Test->TestTrue(TEXT("Placing shows the ball pointer"), Pointer.IsValid() && Pointer->GetTag() == FName(TEXT("BallPointer")));
            }
            return Capture(TEXT("placing"));
        case 14:
            Player->CancelPlacingBall();
            Mode->SetPlayMode(EOpenGolfPlayMode::Hole, BundledCourse::Load());
            Stage = 15;
            return Wait(1.0);
        case 15:
            Test->TestTrue(TEXT("Course HUD names the course"), ShowsText(Window.ToSharedRef(), TEXT("Savanna Range")));
            Test->TestTrue(TEXT("Course header shows the hole"), ShowsText(Window.ToSharedRef(), TEXT("Hole 1")));
            Test->TestTrue(TEXT("Course header shows par and length on their own line"), ShowsText(Window.ToSharedRef(), TEXT("Par 4  ·  400 yd")));
            {
                const FVector2D CourseHeading = SizeOf(Window.ToSharedRef(), TEXT("ShotHeading"));
                Test->TestTrue(FString::Printf(TEXT("The course heading is at least two tiles tall: %s, tile %s"), *CourseHeading.ToString(),
                    *TileSize.ToString()), CourseHeading.Y >= 2 * TileSize.Y - 0.5);
                Test->TestEqual(TEXT("The course heading keeps the column width"), CourseHeading.X, HeadingSize.X, 0.5);
            }
            Test->TestFalse(TEXT("Course rounds hide Games"), IsShown(Window.ToSharedRef(), TEXT("TrainingGames")));
            Test->TestFalse(TEXT("Course rounds hide the Training X"), IsShown(Window.ToSharedRef(), TEXT("TrainingBack")));
            // A normal hole: the tiles and the glow are back; recording stays a range control, and F9 is ignored.
            Test->TestTrue(TEXT("The hole tiles show on a normal hole"), IsShown(Window.ToSharedRef(), TEXT("HoleTiles")));
            Test->TestTrue(TEXT("The glow shows at address on the hole"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            Test->TestFalse(TEXT("Still no record toggle on the hole"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            {
                // The player badge sits right after the club button in the toolbar, and only while signed in.
                const UOpenGolfOnlineSubsystem* Account = Player->GetGameInstance()->GetSubsystem<UOpenGolfOnlineSubsystem>();
                Test->TestEqual(TEXT("The course shows the player badge only while signed in"), IsShown(Window.ToSharedRef(), TEXT("PlayerBadge")),
                    Account && Account->HasSession());
                int32 ClubSlot = INDEX_NONE, BadgeSlot = INDEX_NONE;
                if (const TSharedPtr<SWidget> Toolbar = RenderedPlay::WithTag(Window.ToSharedRef(), TEXT("PlayToolbar")))
                    for (int32 Index = 0; Index < Toolbar->GetChildren()->Num(); ++Index)
                    {
                        const FName Tag = Toolbar->GetChildren()->GetChildAt(Index)->GetTag();
                        if (Tag == FName(TEXT("ClubPicker"))) ClubSlot = Index;
                        if (Tag == FName(TEXT("PlayerBadge"))) BadgeSlot = Index;
                    }
                Test->TestTrue(FString::Printf(TEXT("The player badge follows the club button (slots %d, %d)"), ClubSlot, BadgeSlot),
                    ClubSlot != INDEX_NONE && BadgeSlot == ClubSlot + 1);
            }
            {
                const bool bWasRecording = Mode->IsRecordingShots();
                Player->ToggleRecordShots();
                Test->TestEqual(TEXT("F9 does not change recording on the hole"), Mode->IsRecordingShots(), bWasRecording);
            }
            Stage = 150;
            return Capture(TEXT("course-heading"));
        case 151:
            {
                Mode->SetPlayMode(EOpenGolfPlayMode::Range);
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
            Test->TestTrue(TEXT("Training recording is available"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            return Capture(TEXT("practice"));
        case 17:
            Mode->SetPlayMode(EOpenGolfPlayMode::Range);
            Stage = 18;
            return Wait(1.0);
        case 18:
            Test->TestFalse(TEXT("The range is not practice"), Mode->IsPractice());
            Test->TestTrue(TEXT("The record toggle is back on the range"), IsShown(Window.ToSharedRef(), TEXT("RecordToggle")));
            Test->TestTrue(TEXT("The glow is back on the range"), IsShown(Window.ToSharedRef(), TEXT("BallReadyGlow")));
            if (!GolfSettings::BypassStartupMenu()) return true;
            SavedPreferences = GolfSettings::Get();
            {
                GolfSettings::FPreferences Preferences = SavedPreferences.GetValue();
                Preferences.HudPercent = 150;
                Preferences.TextPercent = 150;
                Preferences.ReducedMotion = 1;
                if (!Test->TestTrue(TEXT("Apply large HUD, large text and reduced motion"), GolfSettings::Commit(Preferences, PlayWorld())))
                    return true;
                if (AUOpenGolfHUD* Hud = Cast<AUOpenGolfHUD>(Player->GetHUD())) Hud->RefreshTheme();
                Window->Resize(FVector2D(1024, 640));
            }
            Stage = 29;
            return Wait(0.5);
        case 29:
            for (const TCHAR* Tag : HudControls)
            {
                const TSharedPtr<SWidget> Control = RenderedPlay::WithTag(Window.ToSharedRef(), Tag);
                FString HitPath;
                Test->TestTrue(FString::Printf(TEXT("Large HUD keeps %s reachable"), Tag),
                    Control.IsValid() && Reachable(Window.ToSharedRef(), *Control, HitPath));
            }
            return ClickThen(TEXT("ClubPicker"), 30);
        case 30:
            Test->TestTrue(TEXT("The drawer opens with reduced motion"), FSlateApplication::Get().AnyMenusVisible());
            return Capture(TEXT("large-hud-reduced-motion"));
        case 31:
            RenderedPlay::Click(Window.ToSharedRef(), RenderedPlay::PointOn(*Window, FVector2D(0.5, 0.25)));
            Stage = 32;
            return Wait(0.5);
        case 32:
            Test->TestFalse(TEXT("Clicking outside dismisses the drawer"), FSlateApplication::Get().AnyMenusVisible());
            RestorePreferences();
            return true;
        default:
            return true;
        }
    }

    UWorld* PlayWorld() const { return GEditor->PlayWorld; }

    void RestorePreferences()
    {
        if (SavedPreferences.IsSet())
        {
            GolfSettings::Commit(SavedPreferences.GetValue(), PlayWorld());
            if (Player && PlayWorld())
                if (AUOpenGolfHUD* Hud = Cast<AUOpenGolfHUD>(Player->GetHUD())) Hud->RefreshTheme();
            SavedPreferences.Reset();
        }
    }

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

    bool ActivateWithKeyboard(const TCHAR* Tag, int32 NextStage)
    {
        TArray<TSharedRef<SWindow>> Windows;
        FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
        TSharedPtr<SWidget> Control;
        for (const auto& Candidate : Windows)
            if ((Control = RenderedPlay::WithTag(Candidate, Tag)))
                break;
        if (!Test->TestTrue(FString(TEXT("Focusable HUD control: ")) + Tag, Control && Control->SupportsKeyboardFocus()))
            return true;
        auto& App = FSlateApplication::Get();
        App.SetKeyboardFocus(Control, EFocusCause::Navigation);
        const FKeyEvent Enter(EKeys::Enter, FModifierKeysState(), 0, false, 0, 0);
        App.ProcessKeyDownEvent(Enter);
        App.ProcessKeyUpEvent(Enter);
        Stage = NextStage;
        return Wait(1.0);
    }

    bool ClickThen(const TCHAR* Tag, int32 NextStage)
    {
        const TSharedPtr<SWidget> Control = RenderedPlay::WithTag(Window.ToSharedRef(), Tag);
        if (!Control)
        {
            Test->AddError(FString(TEXT("Missing HUD control: ")) + Tag);
            return true;
        }
        // Keep Slate's cursor position in sync with the routed test event across popup and focus changes.
        FSlateApplication::Get().SetCursorPos(RenderedPlay::PointOn(*Control));
        if (!RenderedPlay::ClickOn(*Test, Window.ToSharedRef(), Control, Tag))
            return true;
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
    TOptional<GolfSettings::FPreferences> SavedPreferences;
    FVector2D HeadingSize = FVector2D::ZeroVector;
    FVector2D TileSize = FVector2D::ZeroVector;
    FVector2D ClubButtonSize = FVector2D::ZeroVector;
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
