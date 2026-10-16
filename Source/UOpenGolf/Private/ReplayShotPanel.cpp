// SPDX-License-Identifier: MIT
#include "ReplayShotPanel.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HudScale.h"
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
                [ SNew(STextBlock).Font(BodyFont).Text(FText::FromString(TEXT("Test shots [P]   |   Repeat [R]"))) ]
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
                        [ SNew(SButton).IsFocusable(false).Text(FText::FromString(TEXT("Hit selected shot [R]"))).OnClicked(this, &SReplayShotPanel::HitShot) ]
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
    if (Event.GetKey() == EKeys::R)
        return HitShot();
    if (Event.GetKey() == EKeys::P && Controller.IsValid())
    {
        Controller->ToggleReplayPanel();
        return FReply::Handled();
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}
