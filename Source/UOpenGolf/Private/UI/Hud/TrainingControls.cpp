// SPDX-License-Identifier: MIT
#include "UI/Hud/TrainingControls.h"
#include "Training/TrainingScoring.h"
#include "Training/TrainingSession.h"
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

TSharedRef<SWidget> STrainingPicker::MakeMenu()
{
    TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    for (const auto& Game : ATrainingSession::Games())
    {
        TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
        // clang-format off
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(FMargin(10, 0, 12, 0))
        [
            // One width for every name, so the tries buttons line up in columns.
            SNew(SBox).MinDesiredWidth(140)[ SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).Text(Game.GetDefaultObject()->Name()) ]
        ];
        // clang-format on
        for (const int32 Tries : TrainingScoring::TryChoices)
        {
            TSharedRef<SButton> Start = MakeStartButton(Game, Tries);
            if (Rows->NumSlots() == 0 && Row->NumSlots() == 1)
                Combo->SetMenuContentWidgetToFocus(Start);
            Row->AddSlot().AutoWidth().Padding(FMargin(2, 0))[Start];
        }
        Rows->AddSlot().AutoHeight().Padding(FMargin(0, 2))[Row];
    }
    return SNew(SBorder).BorderImage(&Panel).Padding(3)[Rows];
}

TSharedRef<SButton> STrainingPicker::MakeStartButton(TSubclassOf<UTrainingDrill> Game, int32 Tries)
{
    const FText Label = FText::Format(NSLOCTEXT("Training", "StartTries", "{0} tries"), Tries);
    // clang-format off
    return SNew(SButton).ButtonStyle(&Style).ContentPadding(FMargin(12, 7))
        .Tag(FName(*FString::Printf(TEXT("TrainingStart.%s.%d"), *Game->GetName(), Tries)))
#if WITH_ACCESSIBILITY
        .AccessibleText(FText::Format(NSLOCTEXT("Training", "StartGameTries", "{0}, {1} tries"), Game.GetDefaultObject()->Name(), Tries))
#endif
        // With a game running, the choice opens the Exit Drill prompt, whose Exit Drill starts it.
        .IsEnabled_Lambda([this] {
            auto* Session = SessionFor(Player);
            return Session && Session->Mode()->GetBall() && !Session->Mode()->GetBall()->IsMoving();
        })
        .OnClicked_Lambda([this, Game, Tries] {
            if (auto* Session = SessionFor(Player)) Session->ChooseDrill(Game, Tries);
            Combo->SetIsOpen(false);
            // The prompt keeps its own focus.
            if (Player.IsValid() && Player->IsDrillDialogOpen())
                return FReply::Handled();
            return FReply::Handled().SetUserFocus(Combo.ToSharedRef());
        })
        [ SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).Text(Label) ];
    // clang-format on
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
    const FText Switch = bSwitching ? FText::Format(NSLOCTEXT("Training", "SwitchDrill", "Exit Drill to start {0}, {1} tries."),
                                          Pending->Game.GetDefaultObject()->Name(), Pending->Tries)
                                    : FText::GetEmpty();
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
