// SPDX-License-Identifier: MIT
// SMainMenu's home-page launch buttons, its Courses browser and the course round's final scorecard.
#include "UI/Menu/MainMenu.h"
#include "Course/GolfCourseSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GolfCourseDefinition.h"
#include "UI/Common/HudStatusGlyph.h"
#include "UI/Common/HudStyle.h"
#include "UI/Hud/ShotDisplay.h"
#include "UI/Menu/CourseBrowser.h"
#include "UI/Menu/MainMenuInternal.h"
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
        .OnPlay_Lambda([this](const UGolfCourseDefinition& Course)
        {
            if (Controller.IsValid())
                Controller->StartCourse(Course);
        });
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
        const TWeakObjectPtr<const UGolfCourseDefinition> Again(Course);
        AddMenuButton(Body, TEXT("Play again"), [this, Again]
        {
            if (Controller.IsValid() && Again.IsValid())
                Controller->StartCourse(*Again);
        }, true);
    }
    AddMenuButton(Body, TEXT("Courses"), [this] { if (Controller.IsValid()) Controller->ReturnToCourses(); });
}
