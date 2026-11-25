// SPDX-License-Identifier: MIT
// SMainMenu's home-page launch buttons, its Courses browser and the course round's final scorecard.
#include "UI/Menu/MainMenu.h"
#include "Course/GolfCourseSubsystem.h"
#include "Course/RoundConditions.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GolfCourseDefinition.h"
#include "OpenGolfOnlineSubsystem.h"
#include "UI/Common/HudStatusGlyph.h"
#include "UI/Common/HudStyle.h"
#include "UI/Hud/ShotDisplay.h"
#include "UI/Menu/CourseBrowser.h"
#include "UI/Menu/MainMenuInternal.h"
#include "UI/Menu/RoundSetup.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SOverlay.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    /** The launch buttons' icons are drawn at twice the glyph's 16-unit size. */
    constexpr float LaunchIconScale = 2.0f;
    constexpr float LaunchIconGap = 12.0f;
    constexpr int32 LaunchLabelSize = 18;
    constexpr int32 ScorecardCourseSize = 22;
    constexpr int32 ScorecardLineSize = 20;
} // namespace

const FButtonStyle* SMainMenu::LaunchStyle(bool bHighlighted) const
{
    return bHighlighted ? &Style.PrimaryButton : &Style.Button;
}

void SMainMenu::BuildLaunchButtons(TSharedRef<SVerticalBox> Body)
{
    using EIcon = SHudStatusGlyph::EKind;
    // An icon and label; Highlighted picks the primary style, which SetCoursesVisible moves between Training and Courses.
    auto Add = [&](const TCHAR* Label, EIcon Icon, TAttribute<bool> Highlighted, TFunction<void()> Action)
    {
        auto Tint = [this, Highlighted] { return Highlighted.Get() ? Style.PrimaryLabelColor : Style.TextColor; };
        // clang-format off
        TSharedRef<SButton> Button = SNew(SButton).ButtonStyle(LaunchStyle(Highlighted.Get()))
            .OnClicked_Lambda([Action = MoveTemp(Action)] { Action(); return FReply::Handled(); })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, LaunchIconGap, 0)
                [
                    SNew(SDPIScaler).DPIScale(LaunchIconScale)
                    [ SNew(SHudStatusGlyph).Kind(Icon).Color_Lambda(Tint) ]
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Text(FText::FromString(Label)).Font(HudStyle::Font("Regular", LaunchLabelSize))
                    .ColorAndOpacity_Lambda([Tint] { return FSlateColor(Tint()); })
                ]
            ];
        Body->AddSlot().AutoHeight().Padding(0, 0, 0, MenuStyle::ButtonGap).HAlign(HAlign_Left)
        [
            SNew(SBox).WidthOverride(MenuStyle::MenuButtonWidth)[Button]
        ];
        // clang-format on
        return Button;
    };
    TrainingButton = Add(TEXT("Training"), EIcon::Tee, TAttribute<bool>::CreateLambda([this] { return !bCoursesVisible; }), [this] { Resume(); });
    CoursesButton = Add(TEXT("Courses"), EIcon::Flag, TAttribute<bool>::CreateLambda([this] { return bCoursesVisible; }),
        [this] { SetCoursesVisible(true); });
    Add(TEXT("Exit"), EIcon::Exit, false, [this] { Dialog = ExitDialog; Rebuild(); });
}

void SMainMenu::SetCoursesVisible(bool bVisible)
{
    if (bCoursesVisible == bVisible)
        return;
    bCoursesVisible = bVisible;
    if (!bVisible)
        CloseRoundSetup();
    // A button's style is not an attribute, so the highlight moves here; the labels and icons follow by themselves.
    if (const TSharedPtr<SButton> Button = TrainingButton.Pin())
        Button->SetButtonStyle(LaunchStyle(!bVisible));
    if (const TSharedPtr<SButton> Button = CoursesButton.Pin())
        Button->SetButtonStyle(LaunchStyle(bVisible));
}

TSharedRef<SWidget> SMainMenu::BuildCourseBrowser()
{
    const UGameInstance* Instance = Controller.IsValid() ? Controller->GetGameInstance() : nullptr;
    return SNew(SCourseBrowser).Style(&Style)
        .Courses(Instance ? Instance->GetSubsystem<UGolfCourseSubsystem>() : nullptr)
        .Visibility_Lambda([this] { return bCoursesVisible ? EVisibility::Visible : EVisibility::Collapsed; })
        .OnPlay_Lambda([this](const UGolfCourseDefinition& Course) { OpenRoundSetup(Course); });
}

void SMainMenu::OpenRoundSetup(const UGolfCourseDefinition& Course)
{
    UGameInstance* Instance = Controller.IsValid() ? Controller->GetGameInstance() : nullptr;
    UGolfCourseSubsystem* Courses = Instance ? Instance->GetSubsystem<UGolfCourseSubsystem>() : nullptr;
    if (!RoundSetupHost.IsValid())
        return;
    BrowserFocus = FSlateApplication::Get().GetKeyboardFocusedWidget();
    bFocusPending = false;
    SetupCourse = &Course;
    const TWeakObjectPtr<const UGolfCourseDefinition> Weak(&Course);
    const TWeakObjectPtr<UGolfCourseSubsystem> WeakCourses(Courses);
    // The page keeps this session's last choices for the course, and rolls the round's wind each time it opens; Reset to
    // defaults keeps that wind.
    OpenGolfPlay::FOpenGolfRoundSetup Setup = Courses ? Courses->LastSetup(Course) : RoundConditions::Defaults(Course, GolfSettings::Get());
    RoundConditions::RollWind(Setup);
    OpenGolfPlay::FOpenGolfRoundSetup Defaults = RoundConditions::Defaults(Course, GolfSettings::Get());
    Defaults.WindSpeedMph = Setup.WindSpeedMph;
    Defaults.WindFromDeg = Setup.WindFromDeg;
    const TSharedRef<SRoundSetup> Page = SNew(SRoundSetup).Style(&Style).Course(&Course).Setup(Setup).Defaults(Defaults).Online(OnlineService())
        .OnBack_Lambda([this] { CloseRoundSetup(); })
        .OnStart_Lambda([this, Weak, WeakCourses](const OpenGolfPlay::FOpenGolfRoundSetup& Setup)
        {
            const UGolfCourseDefinition* Picked = Weak.Get();
            if (!Picked || !Controller.IsValid())
                return;
            if (UGolfCourseSubsystem* Remember = WeakCourses.Get())
                Remember->RememberSetup(*Picked, Setup);
            Controller->StartCourse(*Picked, Setup);
        });
    RoundSetupHost->SetContent(Page);
    if (FSlateApplication::IsInitialized() && Page->GetPreferredFocus().IsValid())
        FSlateApplication::Get().SetKeyboardFocus(Page->GetPreferredFocus(), EFocusCause::Navigation);
}

void SMainMenu::CloseRoundSetup()
{
    if (!IsRoundSetupOpen())
        return;
    SetupCourse.Reset();
    if (RoundSetupHost.IsValid())
        RoundSetupHost->SetContent(SNullWidget::NullWidget);
    if (TSharedPtr<SWidget> Previous = BrowserFocus.Pin(); bCoursesVisible && Previous.IsValid())
        FSlateApplication::Get().SetKeyboardFocus(Previous, EFocusCause::Navigation);
    else
        bFocusPending = true;
    BrowserFocus.Reset();
}

void SMainMenu::BuildFinalScore(TSharedRef<SVerticalBox> Body)
{
    const UWorld* World = Controller.IsValid() ? Controller->GetWorld() : nullptr;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const UGolfCourseDefinition* Course = Mode ? Mode->GetCourse() : nullptr;
    Body->AddSlot().AutoHeight()[Text(TEXT("Final scorecard"), MenuStyle::TitleSize)];
    if (Mode && Course)
    {
        const FOpenGolfHoleRound& Round = Mode->GetHoleRound();
        Body->AddSlot().AutoHeight().Padding(0, 4, 0, 20)[MainMenuDetail::MakeText(Course->DisplayName, ScorecardCourseSize)];
        const FString Line = FString::Printf(TEXT("Hole %d   ·   Par %d   ·   %d %s   ·   %s"), Mode->GetHoleNumber(), Round.Par,
            Round.Strokes, Round.Strokes == 1 ? TEXT("stroke") : TEXT("strokes"), *Round.ScoreToPar());
        Body->AddSlot().AutoHeight().Padding(0, 0, 0, 20)[Text(Line, ScorecardLineSize)];
        const auto& Scores = Mode->GetCompletedHoles();
        const auto& Numbers = Mode->GetCompletedHoleNumbers();
        if (Scores.Num() > 1 && Numbers.Num() == Scores.Num())
        {
            // A line for each nine the round visited, by the holes' own numbers, then the total against par.
            int32 Strokes[2] = {0, 0}, Par[2] = {0, 0}, Count[2] = {0, 0}, First[2] = {0, 0}, Last[2] = {0, 0};
            for (int32 Index = 0; Index < Scores.Num(); ++Index)
            {
                const int32 Nine = Numbers[Index] > OpenGolfPlay::HolesPerNine ? 1 : 0;
                Strokes[Nine] += Scores[Index].Strokes;
                Par[Nine] += Scores[Index].Par;
                // Holes play in ascending order, so a nine's first hole seen is its lowest.
                if (Count[Nine]++ == 0)
                    First[Nine] = Numbers[Index];
                Last[Nine] = FMath::Max(Last[Nine], Numbers[Index]);
            }
            const TCHAR* const NineNames[] = {TEXT("Front 9"), TEXT("Back 9")};
            for (int32 Nine = 0; Nine < 2; ++Nine)
            {
                if (Count[Nine] == 0)
                    continue;
                const FString Holes = First[Nine] == Last[Nine] ? FString::Printf(TEXT("hole %d"), First[Nine])
                                                                : FString::Printf(TEXT("holes %d–%d"), First[Nine], Last[Nine]);
                const FString Row = FString::Printf(TEXT("%s   ·   %s   ·   %d strokes   ·   %s"), NineNames[Nine], *Holes, Strokes[Nine],
                    *FOpenGolfHoleRound::ScoreToPar(Strokes[Nine], Par[Nine]));
                Body->AddSlot().AutoHeight()[Text(Row, ScorecardLineSize)];
            }
            const int32 Total = Strokes[0] + Strokes[1];
            Body->AddSlot().AutoHeight()[Text(FString::Printf(TEXT("Total   ·   %d holes   ·   %d strokes   ·   %s"), Scores.Num(), Total,
                *FOpenGolfHoleRound::ScoreToPar(Total, Par[0] + Par[1])), ScorecardLineSize)];
        }
        const TWeakObjectPtr<const UGolfCourseDefinition> Again(Course);
        AddMenuButton(Body, TEXT("Play again"), [this, Again, Setup = Mode->GetRoundSetup()]
        {
            if (!Controller.IsValid() || !Again.IsValid())
                return;
            // The same choices; a new round brings new weather.
            OpenGolfPlay::FOpenGolfRoundSetup Next = Setup;
            RoundConditions::RollWind(Next);
            Controller->StartCourse(*Again, Next);
        }, true);
    }
    AddMenuButton(Body, TEXT("Courses"), [this] { if (Controller.IsValid()) Controller->ReturnToCourses(); });
}
