// SPDX-License-Identifier: MIT
#include "UI/ReplayShotPanel.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/HudScale.h"
#include "InputCoreTypes.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UOpenGolfPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

void SReplayShotPanel::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    // The panel spans the viewport over the play controls: only its button and open panel take clicks.
    SetVisibility(EVisibility::SelfHitTestInvisible);
    for (const FString& Name : UOpenLaunchMonitorSubsystem::GetReplayShotNames())
    {
        Options.Add(MakeShared<FString>(Name));
    }
    const FSlateFontInfo BodyFont = FCoreStyle::GetDefaultFontStyle("Regular", 11);
    // Preserve Slate's layout syntax, which clang-format otherwise collapses.
    // clang-format off
    ChildSlot
    [
        SNew(SDPIScaler).DPIScale_Lambda([this] { return HudScale::Readable(Controller.IsValid() ? Controller->GetWorld() : nullptr); })
        [
            SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(20, 0, 20, HudScale::BottomPanelGap)
            [
                SNew(SButton).IsFocusable(false)
                // Test shots are for playing without a launch monitor.
                .Visibility_Lambda([this] { return Controller.IsValid() && !Controller->IsReplayPanelOpen() && !IsLaunchMonitorConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
                .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->SetReplayPanelOpen(true); return FReply::Handled(); })
                [ SNew(STextBlock).Font(BodyFont).Text(FText::FromString(TEXT("Test shots [P]   |   Hit [H]"))) ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(520)
                .Visibility_Lambda([this] { return Controller.IsValid() && Controller->IsReplayPanelOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
                [
                    SNew(SBorder).Padding(16)
                    .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                    .BorderBackgroundColor(FLinearColor(0.025f, 0.035f, 0.05f, 1.0f))
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1)
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Test shots"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("Close [P]")))
                                .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->SetReplayPanelOpen(false); return FReply::Handled(); })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                        [
                            SAssignNew(ShotList, SComboBox<TSharedPtr<FString>>)
                            .OptionsSource(&Options).MaxListHeight(210)
                            .OnComboBoxOpening_Lambda([this]
                            {
                                if (Controller.IsValid() && Options.IsValidIndex(Controller->GetSelectedReplayShot()))
                                    ShotList->SetSelectedItem(Options[Controller->GetSelectedReplayShot()]);
                            })
                            .OnGenerateWidget_Lambda([BodyFont](TSharedPtr<FString> Item) { return SNew(STextBlock).Font(BodyFont).Text(FText::FromString(*Item)); })
                            .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { if (Controller.IsValid()) Controller->SelectReplayShot(Options.IndexOfByKey(Item)); })
                            [ SNew(STextBlock).Font(BodyFont).Text(this, &SReplayShotPanel::SelectedName) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                        [
                            SNew(STextBlock).Font(BodyFont).WrapTextAt(488)
                            .Text_Lambda([this] { return FText::FromString(Controller.IsValid() ? UOpenLaunchMonitorSubsystem::GetReplayShotDescription(Controller->GetSelectedReplayShot()) : FString()); })
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                        [ SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("Hit selected shot [H]"))).OnClicked(this, &SReplayShotPanel::HitShot) ]
                    ]
                ]
            ]
        ]
    ];
    // clang-format on
}

FText SReplayShotPanel::SelectedName() const
{
    const int32 Index = Controller.IsValid() ? Controller->GetSelectedReplayShot() : INDEX_NONE;
    return FText::FromString(Options.IsValidIndex(Index) ? *Options[Index] : FString());
}

bool SReplayShotPanel::IsLaunchMonitorConnected() const
{
    const UGameInstance* GameInstance = Controller.IsValid() ? Controller->GetGameInstance() : nullptr;
    const UOpenLaunchMonitorSubsystem* LaunchMonitor = GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    return LaunchMonitor && LaunchMonitor->GetConnectionState() != ELaunchMonitorConnectionState::Disconnected;
}

FReply SReplayShotPanel::HitShot()
{
    if (Controller.IsValid())
        Controller->InjectReplayShot();
    return FReply::Handled();
}

FReply SReplayShotPanel::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::H)
        return HitShot();
    if (Event.GetKey() == EKeys::P && Controller.IsValid())
    {
        Controller->ToggleReplayPanel();
        return FReply::Handled();
    }
    // The panel can hold keyboard focus, so it forwards the play keys the controller binds.
    if (Event.GetKey() == EKeys::G && Controller.IsValid())
    {
        Controller->TogglePlayMode();
        return FReply::Handled();
    }
    if ((Event.GetKey() == EKeys::A || Event.GetKey() == EKeys::D) && Controller.IsValid())
    {
        Controller->NudgeAim(Event.GetKey() == EKeys::D ? 1 : -1);
        return FReply::Handled();
    }
    if (Event.GetKey() == EKeys::B && Controller.IsValid())
    {
        Controller->TogglePlacingBall();
        return FReply::Handled();
    }
    if (Event.GetKey() == EKeys::W && Controller.IsValid())
    {
        Controller->ToggleWindPanel();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}
