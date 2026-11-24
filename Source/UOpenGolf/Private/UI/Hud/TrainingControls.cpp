// SPDX-License-Identifier: MIT
#include "UI/Hud/TrainingControls.h"
#include "Training/TrainingScoring.h"
#include "UI/Common/HudStatusGlyph.h"
#include "UI/Common/HudScale.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "Engine/World.h"
#include "OpenFairwayBall.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    ATrainingSession* SessionFor(const TWeakObjectPtr<AUOpenGolfPlayerController>& Player)
    {
        const auto* Mode = Player.IsValid() ? Player->GetWorld()->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
        return Mode ? Mode->GetTrainingSession() : nullptr;
    }
} // namespace

void STrainingPicker::Construct(const FArguments& Args)
{
    Player = Args._Player;
    Choice.Game = ATrainingSession::Games()[0];
    const FText Label = NSLOCTEXT("Training", "Games", "Training games");
    // clang-format off
    ChildSlot
    [
        // No tooltip: the owner wants none on the training games button.
        SAssignNew(Combo, SComboButton).ButtonStyle(&Style).HasDownArrow(false).ContentPadding(0)
        .MenuPlacement(MenuPlacement_BelowAnchor)
#if WITH_ACCESSIBILITY
        .AccessibleText(Label)
#endif
        .OnGetMenuContent(this, &STrainingPicker::MakeMenu)
        .ButtonContent()
        [
            SNew(SBox).WidthOverride(HudStyle::ToolbarButtonSize).HeightOverride(HudStyle::ToolbarButtonSize)
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [ SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Games) ]
        ]
    ];
    // clang-format on
}

bool STrainingPicker::IsOpen() const
{
    return Combo && Combo->IsOpen();
}

TSharedRef<SButton> STrainingPicker::MakeOption(FName Tag, const FText& Label, TAttribute<bool> Selected, FOnClicked OnClick)
{
    // clang-format off
    return SNew(SButton).Tag(Tag).ButtonStyle(&Style).ContentPadding(FMargin(3, 4)).HAlign(HAlign_Fill)
#if WITH_ACCESSIBILITY
        .AccessibleText(TAttribute<FText>::CreateLambda([Selected, Label] {
            return Selected.Get() ? FText::Format(NSLOCTEXT("Training", "SelectedOption", "{0}, selected"), Label) : Label;
        }))
#endif
        .OnClicked(OnClick)
        [
            SNew(SBorder).Padding(FMargin(8, 6)).HAlign(HAlign_Center)
            // Bound to this picker's lifetime: the brushes are its members, and the menu is a separate window.
            .BorderImage(TAttribute<const FSlateBrush*>::CreateSPLambda(this, [this, Selected]() -> const FSlateBrush* {
                return Selected.Get() ? &SelectedBrush : &OptionBrush;
            }))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(SBox).WidthOverride(16.f)
                    [
                        SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).ColorAndOpacity(HudStyle::AccentColor())
                        .Text_Lambda([Selected] { return Selected.Get() ? FText::FromString(TEXT("\u2713")) : FText::GetEmpty(); })
                    ]
                ]
                + SHorizontalBox::Slot().AutoWidth()
                [
                    SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).Text(Label).ColorAndOpacity(HudStyle::TextColor())
                ]
            ]
        ];
    // clang-format on
}

template <typename T>
TSharedRef<SButton> STrainingPicker::MakeChoice(FName Tag, const FText& Label, T FDrillChoice::*Field, T Value)
{
    return MakeOption(Tag, Label, TAttribute<bool>::CreateSPLambda(this, [this, Field, Value] { return Choice.*Field == Value; }),
        FOnClicked::CreateSPLambda(this, [this, Field, Value] {
            Choice.*Field = Value;
            return FReply::Handled();
        }));
}

TSharedRef<SWidget> STrainingPicker::MakeMenu()
{
    // The running game's settings; with none running, the player's last choice.
    const ATrainingSession* Session = SessionFor(Player);
    if (const UTrainingDrill* Drill = Session ? Session->Active() : nullptr)
    {
        const FDrillChoice Running = Drill->GetChoice();
        Choice.Game = Running.Game;
        Choice.RadiusFeet = Running.RadiusFeet;
        // A game without tries keeps the tries last picked for one with them.
        if (Drill->UsesTries())
            Choice.Tries = Running.Tries;
    }

    TSharedRef<SHorizontalBox> Games = SNew(SHorizontalBox);
    for (const TSubclassOf<UTrainingDrill>& Game : ATrainingSession::Games())
    {
        const TSharedRef<SButton> Button = MakeChoice(FName(*FString::Printf(TEXT("TrainingGame.%s"), *Game->GetName())),
            Game.GetDefaultObject()->Name(), &FDrillChoice::Game, Game);
        if (Choice.Game == Game)
            Combo->SetMenuContentWidgetToFocus(Button);
        Games->AddSlot().FillWidth(1.f)[Button];
    }

    TSharedRef<SHorizontalBox> Radii = SNew(SHorizontalBox);
    for (const int32 Feet : TrainingScoring::RadiusChoices)
        Radii->AddSlot().AutoWidth()[MakeChoice(FName(*FString::Printf(TEXT("TrainingRadius.%d"), Feet)), TrainingScoring::RadiusText(Feet),
            &FDrillChoice::RadiusFeet, Feet)];

    TSharedRef<SHorizontalBox> Tries = SNew(SHorizontalBox);
    for (const int32 Count : TrainingScoring::TryChoices)
        Tries->AddSlot().AutoWidth()[MakeChoice(FName(*FString::Printf(TEXT("TrainingTries.%d"), Count)), FText::AsNumber(Count),
            &FDrillChoice::Tries, Count)];

    auto Label = [](const FText& Text)
    {
        return SNew(SBox).MinDesiredWidth(115.f).VAlign(VAlign_Center)
            [SNew(STextBlock).Font(HudStyle::Font("Regular", 11)).Text(Text).ColorAndOpacity(HudStyle::TextColor())];
    };

    // clang-format off
    return SNew(SBorder).Tag(TEXT("TrainingSetup")).BorderImage(&Panel).Padding(FMargin(10, 7))
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[Games]
        + SVerticalBox::Slot().AutoHeight().Padding(4, 4, 4, 8)
        [
            SNew(STextBlock).Font(HudStyle::Font("Regular", 11)).ColorAndOpacity(HudStyle::LabelColor())
            .Text(TAttribute<FText>::CreateSPLambda(this, [this] { return Choice.Game.GetDefaultObject()->Hint(); }))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(4, 0, 0, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()[Label(NSLOCTEXT("Training", "WinningRadius", "Winning radius"))]
            + SHorizontalBox::Slot().AutoWidth()[Radii]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(4, 0, 0, 0)
        [
            // Only a game that takes tries offers them; Distance Practice has no shot limit.
            SNew(SHorizontalBox)
            .Visibility(TAttribute<EVisibility>::CreateSPLambda(this, [this] {
                return Choice.Game.GetDefaultObject()->UsesTries() ? EVisibility::Visible : EVisibility::Collapsed;
            }))
            + SHorizontalBox::Slot().AutoWidth()[Label(NSLOCTEXT("Training", "TriesPerTarget", "Tries per target"))]
            + SHorizontalBox::Slot().AutoWidth()[Tries]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(4, 8, 4, 5)
        [
            SNew(SButton).Tag(TEXT("TrainingStart")).ButtonStyle(&StartStyle).ContentPadding(FMargin(12, 8)).HAlign(HAlign_Center)
            .IsEnabled(TAttribute<bool>::CreateSPLambda(this, [this] {
                const auto* Running = SessionFor(Player);
                const auto* Ball = Running ? Running->Mode()->GetBall() : nullptr;
                return Ball && !Ball->IsMoving();
            }))
            .OnClicked(this, &STrainingPicker::StartDrill)
            [
                SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).ColorAndOpacity(HudStyle::MapInkColor())
                .Text(TAttribute<FText>::CreateSPLambda(this, [this] {
                    return FText::Format(NSLOCTEXT("Training", "StartGame", "Start {0}"), Choice.Game.GetDefaultObject()->Name());
                }))
            ]
        ]
    ];
    // clang-format on
}

FReply STrainingPicker::StartDrill()
{
    auto* Session = SessionFor(Player);
    if (!Session || !Session->ChooseDrill(Choice.Game, Choice.Tries, Choice.RadiusFeet))
        return FReply::Handled();

    Combo->SetIsOpen(false);
    if (Player.IsValid() && Player->IsDrillDialogOpen())
        return FReply::Handled();
    return FReply::Handled().SetUserFocus(Combo.ToSharedRef());
}

void SDrillDialog::Construct(const FArguments& Args)
{
    Player = Args._Player;
    auto Choose = [this](bool bRestart)
    {
        if (auto* Session = SessionFor(Player))
        {
            if (bRestart)
            {
                Session->CancelPendingDrill();
                Session->RestartDrill();
            }
            else if (Session->GetPendingDrill())
                Session->StartPendingDrill();
            else
                Session->ExitDrill();
        }
        if (Player.IsValid())
            Player->CloseDrillDialog();
        return FReply::Handled();
    };
    auto MakeChoice = [this, Choose](TSharedPtr<SButton>& Button, FText Label, bool bRestart)
    {
        return SAssignNew(Button, SButton).ButtonStyle(&Style).ContentPadding(FMargin(12, 8))
            .OnClicked_Lambda([Choose, bRestart] { return Choose(bRestart); })
            [ SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).Text(Label) ];
    };
    auto* Session = SessionFor(Player);
    const UTrainingDrill* Drill = Session ? Session->Active() : nullptr;
    const FText Title = Drill ? Drill->Name() : FText::GetEmpty();
    const FText Result = Drill ? Drill->ResultText() : FText::GetEmpty();
    // A game picked while this one runs starts on Exit Drill.
    const TOptional<FDrillChoice> Pending = Session ? Session->GetPendingDrill() : TOptional<FDrillChoice>();
    bSwitching = Pending.IsSet();
    FText Switch;
    if (bSwitching)
    {
        const UTrainingDrill* Next = Pending->Game.GetDefaultObject();
        const FText Radius = TrainingScoring::RadiusText(Pending->RadiusFeet);
        Switch = Next->UsesTries()
            ? FText::Format(NSLOCTEXT("Training", "SwitchDrill", "Exit Drill to start {0} with a {1} ring and {2} tries."), Next->Name(), Radius, Pending->Tries)
            : FText::Format(NSLOCTEXT("Training", "SwitchDrillRing", "Exit Drill to start {0} with a {1} ring."), Next->Name(), Radius);
    }
    // A finished drill offers another run; the button does the same either way.
    bComplete = Drill && Drill->IsComplete();
    const FText RestartLabel = bComplete ? NSLOCTEXT("Training", "PlayAgain", "Play Again") : NSLOCTEXT("Training", "RestartDrill", "Restart Drill");
    // clang-format off
    ChildSlot
    [
        SNew(SBorder).BorderImage(&Backdrop).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(SDPIScaler).DPIScale_Lambda([this] { return HudScale::Readable(Player.IsValid() ? Player->GetWorld() : nullptr); })
            [
                SNew(SBorder).BorderImage(&Panel).Padding(16)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                    [ SNew(STextBlock).Font(HudStyle::Font("Bold", 14)).Text(Title) ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                    [
                        SNew(STextBlock).Font(HudStyle::Font("Regular", 11)).Text(Result).Tag(TEXT("DrillResult"))
                        .Visibility(Result.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                    [
                        SNew(STextBlock).Font(HudStyle::Font("Regular", 11)).Text(Switch).Tag(TEXT("DrillSwitch"))
                        .Visibility(bSwitching ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
                    ]
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().AutoWidth()
                        [ MakeChoice(Exit, NSLOCTEXT("Training", "ExitDrill", "Exit Drill"), false) ]
                        + SHorizontalBox::Slot().AutoWidth().Padding(6, 0, 0, 0)
                        [ MakeChoice(Restart, RestartLabel, true) ]
                    ]
                ]
            ]
        ]
    ];
    // clang-format on
}

TSharedPtr<SWidget> SDrillDialog::FirstButton() const
{
    // A finished game most likely goes again, unless the player has picked another.
    return bComplete && !bSwitching ? Restart : Exit;
}

FNavigationReply SDrillDialog::OnNavigation(const FGeometry&, const FNavigationEvent&)
{
    return FNavigationReply::Explicit(Exit->HasKeyboardFocus() ? Restart : Exit);
}
