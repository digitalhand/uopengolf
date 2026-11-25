// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Tests/RenderedPlay.h"
#include "Tests/HoleMapContent.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "Course/GolfCourseSubsystem.h"
#include "Engine/GameInstance.h"
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

namespace
{
    /** The browser across real level travel: choosing a course, a round, the final scorecard and the way back. */
    class FVisitCourse : public IAutomationLatentCommand
    {
    public:
        explicit FVisitCourse(FAutomationTestBase& InTest) : Test(InTest) {}

        bool Update() override
        {
            if (GetCurrentRunTime() > 130)
            {
                Test.AddError(FString::Printf(TEXT("Course browser timed out at step %d"), int32(Step)));
                return true;
            }
            UWorld* World = GEditor->PlayWorld;
            auto* Mode = World && World->HasBegunPlay() ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
            auto* Player = World ? Cast<AUOpenGolfPlayerController>(World->GetFirstPlayerController()) : nullptr;
            if (!Mode || !Player || !Mode->GetBall() || !World->GetGameViewport() || GetCurrentRunTime() < Next)
                return false;
            const TSharedPtr<SWindow> Window = World->GetGameViewport()->GetWindow();
            if (!Window)
                return false;
            const TSharedRef<SWindow> Root = Window.ToSharedRef();
            const auto Click = [&](const TCHAR* Label) { return RenderedPlay::ClickLabelled(Test, Label); };
            const auto CardShown = [&] { return RenderedPlay::FindButton(Root, TEXT("Savanna Range")).IsValid(); };
            switch (Step)
            {
            case EStep::OpenMenu:
                Player->SetWindOn(true);
                Player->SetWindSpeedMph(12);
                Player->OpenMainMenu();
                break;
            case EStep::OpenCourses:
            {
                const TSharedPtr<SWidget> Training = RenderedPlay::FindButton(Root, TEXT("Training"));
                if (!Test.TestTrue(TEXT("Training button is visible on Home"), Training.IsValid()))
                    return true;
                HomeTrainingButton = Training;
                HomeTrainingPosition = RenderedPlay::PointOn(*Training);
                Test.TestFalse(TEXT("Course cards start hidden"), CardShown());
                if (!Click(TEXT("Courses")))
                    return true;
                break;
            }
            case EStep::Search:
            {
                const TSharedPtr<SWidget> Training = RenderedPlay::FindButton(Root, TEXT("Training"));
                const TSharedPtr<SWidget> Card = RenderedPlay::FindButton(Root, TEXT("Savanna Range"));
                if (!Test.TestTrue(TEXT("Training and the course card are visible together"), Training && Card))
                    return true;
                Test.TestTrue(TEXT("Training stays in place while Courses opens"),
                    Training == HomeTrainingButton.Pin() && RenderedPlay::PointOn(*Training).Equals(HomeTrainingPosition, 0.5));
                Test.TestTrue(TEXT("Courses appears to the right of the menu buttons"),
                    RenderedPlay::PointOn(*Card, {0, 0.5}).X > RenderedPlay::PointOn(*Training, {1, 0.5}).X);
                {
                    // Course plugins such as Hunter Ranch are listed beside the bundled course in the editor.
                    const auto* Courses = World->GetGameInstance()->GetSubsystem<UGolfCourseSubsystem>();
                    const int32 Installed = Courses ? Courses->GetCourses().Num() : 0;
                    Test.TestTrue(TEXT("The browser counts the installed courses"), Installed >= 1 &&
                        RenderedPlay::HasText(Root, FString::Printf(TEXT("%d installed"), Installed)));
                }
                const TSharedPtr<SWidget> SearchBox = RenderedPlay::WithTag(Root, TEXT("CourseSearch"));
                if (!Test.TestTrue(TEXT("Search is present and enabled"), SearchBox && SearchBox->IsEnabled()))
                    return true;
                // Typed, as a player would: the box reports edits, not text set from code, a moment after typing stops.
                FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::Mouse);
                for (const TCHAR Character : FString(TEXT("zq")))
                    FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(Character, FModifierKeysState(), 0, false));
                break;
            }
            case EStep::ClearSearch:
                Test.TestFalse(TEXT("A search without a match hides the card"), CardShown());
                // Slate edits text on characters: Backspace arrives as '\b', as the platform sends it.
                for (int32 Index = 0; Index < 2; ++Index)
                    FSlateApplication::Get().ProcessKeyCharEvent(FCharacterEvent(TEXT('\b'), FModifierKeysState(), 0, false));
                break;
            case EStep::SelectCard:
                Test.TestTrue(TEXT("Clearing the search shows the card again"), CardShown());
                Test.TestFalse(TEXT("Selection required before Play"), RenderedPlay::FindButton(Root, TEXT("Play")).IsValid());
                if (!Click(TEXT("Savanna Range")))
                    return true;
                break;
            case EStep::Selected:
                Test.TestFalse(TEXT("Selecting a card keeps the player in the browser"), Player->IsCourseRound());
                for (const TCHAR* Detail : {TEXT("Holes"), TEXT("Par"), TEXT("Length")})
                    Test.TestTrue(FString(TEXT("The selected card shows ")) + Detail, RenderedPlay::HasText(Root, Detail));
                RenderedPlay::SaveWindowPng(Test, Root, TEXT("Menu/Courses-selected.png"));
                Window->Resize(FVector2D(800, 600));
                break;
            case EStep::PlayFromNarrow:
            {
                RenderedPlay::SaveWindowPng(Test, Root, TEXT("Menu/Courses-narrow.png"));
                const TSharedPtr<SWidget> Play = RenderedPlay::FindButton(Root, TEXT("Play"));
                if (!Test.TestTrue(TEXT("Play is keyboard focusable"), Play && Play->SupportsKeyboardFocus()))
                    return true;
                FSlateApplication::Get().SetKeyboardFocus(Play, EFocusCause::Navigation);
                Press(EKeys::Gamepad_FaceButton_Bottom);
                break;
            }
            case EStep::FirstRound:
            case EStep::SecondRound:
                if (!Player->IsCourseRound() || Player->IsMainMenuOpen())
                    return false;
                // After level travel the hole map waits for the new world's assets; it then shows the course, not
                // loading placeholders.
                if (!Mode->GetRange()->GetHoleMapTexture())
                    return false;
                if (Step == EStep::FirstRound)
                {
                    HoleMapContent::ShowsTurf(Test, Mode->GetRange()->GetHoleMapCaptureTarget(), TEXT("Course round"));
                    RenderedPlay::SaveWindowPng(Test, Root, TEXT("Minimap-course.png"));
                }
                Test.TestTrue(TEXT("The round plays the chosen course"),
                    Mode->GetCourse() && Mode->GetCourse()->GetPrimaryAssetId().PrimaryAssetName == TEXT("DA_SavannaRange"));
                Test.TestEqual(TEXT("Fresh course starts at zero"), Mode->GetHoleRound().Strokes, 0);
                Player->TogglePlacingBall();
                Test.TestFalse(TEXT("Placement unavailable in Courses"), Player->IsPlacingBall());
                Player->SetWindOn(false);
                Test.TestEqual(TEXT("Ball uses locked course wind"), Mode->GetBall()->WindSpeedMph, Mode->GetCourse() ? Mode->GetCourse()->WindMph : -1.f);
                if (Step == EStep::FirstRound)
                    Player->OpenMainMenu();
                else
                    Finish(*Mode);
                break;
            case EStep::ResumeRound:
                if (!Click(TEXT("Resume round")))
                    return true;
                break;
            case EStep::FinishFirst:
                Finish(*Mode);
                break;
            case EStep::FirstScorecard:
            case EStep::SecondScorecard:
            {
                // Wait for the scorecard's page transition and layout before hit-testing its buttons.
                if (!Player->IsMainMenuOpen() || Mode->GetHoledSeconds() < AUOpenGolfGameMode::HoleOutDelayS + 0.3f)
                    return false;
                Test.TestEqual(TEXT("Final score retained"), Mode->GetHoleRound().Strokes, 1);
                Test.TestTrue(TEXT("The scorecard reads the score against par"), RenderedPlay::HasText(Root, TEXT("-3"), true));
                const TSharedPtr<SWidget> Action = RenderedPlay::FindButton(Root, Step == EStep::FirstScorecard ? TEXT("Play again") : TEXT("Courses"));
                if (!Test.TestTrue(TEXT("Scorecard action is focusable"), Action && Action->SupportsKeyboardFocus()))
                    return true;
                FSlateApplication::Get().SetKeyboardFocus(Action, EFocusCause::Navigation);
                Press(EKeys::Enter);
                break;
            }
            case EStep::BackInCourses:
                if (!Player->IsMainMenuOpen() || Player->IsCourseRound())
                    return false;
                Test.TestTrue(TEXT("Return opens Courses"), RenderedPlay::WithTag(Root, TEXT("CourseSearch")).IsValid());
                if (!Click(TEXT("Training")))
                    return true;
                break;
            case EStep::TrainingAgain:
                Test.TestFalse(TEXT("Training starts directly"), Player->IsMainMenuOpen() || Player->IsCourseRound());
                Test.TestEqual(TEXT("Training restores its wind"), Mode->GetRange()->WindSpeedMph, 12.f);
                return true;
            }
            Step = EStep(int32(Step) + 1);
            Next = GetCurrentRunTime() + 0.6;
            return false;
        }

    private:
        /** In order; each runs once the previous has settled (0.6 s, longer for travel and the scorecard). */
        enum class EStep : int32
        {
            OpenMenu, OpenCourses, Search, ClearSearch, SelectCard, Selected, PlayFromNarrow,
            FirstRound, ResumeRound, FinishFirst, FirstScorecard, SecondRound, SecondScorecard, BackInCourses, TrainingAgain
        };

        /** One stroke, holed: an ace. */
        static void Finish(AUOpenGolfGameMode& Mode)
        {
            FOpenFairwayShotResult Result;
            Result.bHoled = true;
            Result.ShotNumber = 1;
            Mode.GetBall()->OnBallAtRest.Broadcast(Result);
        }

        /** Presses and releases Key on the focused widget. */
        static void Press(const FKey& Key)
        {
            const FKeyEvent Event(Key, FModifierKeysState(), 0, false, 0, 0);
            FSlateApplication::Get().ProcessKeyDownEvent(Event);
            FSlateApplication::Get().ProcessKeyUpEvent(Event);
        }

        FAutomationTestBase& Test;
        TWeakPtr<SWidget> HomeTrainingButton;
        FVector2D HomeTrainingPosition = FVector2D::ZeroVector;
        EStep Step = EStep::OpenMenu;
        double Next = 0;
    };
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourseMenuTest, "UOpenGolf.Visual.CourseBrowser",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)

bool FCourseMenuTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FVisitCourse>(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
