// SPDX-License-Identifier: MIT
#pragma once

#include "Framework/SlateDelegates.h"
#include "Training/TrainingSession.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class SButton;
class SComboButton;

class STrainingPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STrainingPicker) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Player)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    bool IsOpen() const;

private:
    TSharedRef<SWidget> MakeMenu();
    TSharedRef<SButton> MakeOption(FName Tag, const FText& Label, TAttribute<bool> Selected, FOnClicked OnClick);
    /** An option that sets Choice.*Field to Value, checked while it holds that value. */
    template <typename T>
    TSharedRef<SButton> MakeChoice(FName Tag, const FText& Label, T FDrillChoice::*Field, T Value);
    FReply StartDrill();
    FDrillChoice Choice;
    FSlateRoundedBoxBrush OptionBrush{HudStyle::TileColor(), HudStyle::TileRadius, HudStyle::MapEdgeColor(), 1.f};
    FSlateRoundedBoxBrush SelectedBrush{HudStyle::HoverTileColor(), HudStyle::TileRadius, HudStyle::AccentColor(), 1.f};
    FButtonStyle StartStyle = HudStyle::ToolbarButton()
        .SetNormal(FSlateRoundedBoxBrush(HudStyle::AccentColor(), HudStyle::TileRadius))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor::White, HudStyle::TileRadius))
        .SetPressed(FSlateRoundedBoxBrush(HudStyle::LabelColor(), HudStyle::TileRadius))
        .SetDisabled(FSlateRoundedBoxBrush(HudStyle::LabelColor(), HudStyle::TileRadius));
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
