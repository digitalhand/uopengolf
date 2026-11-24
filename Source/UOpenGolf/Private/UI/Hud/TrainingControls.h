// SPDX-License-Identifier: MIT
#pragma once

#include "Templates/SubclassOf.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class SButton;
class SComboButton;
class UTrainingDrill;

class STrainingPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STrainingPicker) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Player)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);

private:
    TSharedRef<SWidget> MakeMenu();
    /** Starts Game with this many tries; tagged TrainingStart.<class>.<tries>. */
    TSharedRef<SButton> MakeStartButton(TSubclassOf<UTrainingDrill> Game, int32 Tries);
    TWeakObjectPtr<AUOpenGolfPlayerController> Player;
    TSharedPtr<SComboButton> Combo;
    FButtonStyle Style = HudStyle::ToolbarButton();
    FSlateRoundedBoxBrush Panel{HudStyle::LaunchTileColor(), HudStyle::TileRadius};
};

class SDrillDialog : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SDrillDialog) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Player)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    TSharedPtr<SWidget> FirstButton() const;
    virtual FNavigationReply OnNavigation(const FGeometry&, const FNavigationEvent&) override;
    virtual FReply OnMouseButtonDown(const FGeometry&, const FPointerEvent&) override { return FReply::Handled(); }

private:
    TWeakObjectPtr<AUOpenGolfPlayerController> Player;
    TSharedPtr<SButton> Exit, Restart;
    bool bComplete = false;
    /** Opened by picking another game: Exit Drill starts it. */
    bool bSwitching = false;
    FSlateRoundedBoxBrush Panel{HudStyle::LaunchTileColor(), HudStyle::TileRadius};
    FSlateRoundedBoxBrush Backdrop{FLinearColor(0, 0, 0, .45f), 0.f};
    FButtonStyle Style = HudStyle::ToolbarButton();
};
