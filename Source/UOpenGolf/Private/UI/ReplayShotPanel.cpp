// SPDX-License-Identifier: MIT
#include "UI/ReplayShotPanel.h"
#include "UI/HudStyle.h"

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
    SetVisibility(TAttribute<EVisibility>::CreateLambda([this]{return Controller.IsValid() && Controller->IsMainMenuOpen() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;}));
    for (const FString& Name : UOpenLaunchMonitorSubsystem::GetReplayShotNames())
    {
        Options.Add(MakeShared<FString>(Name));
    }
    const FSlateFontInfo BodyFont = HudStyle::Font("Regular", 11);
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
                    .BorderBackgroundColor(HudStyle::LaunchTileColor())
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1)
                            [ SNew(STextBlock).Text(FText::FromString(TEXT("Test shots"))).Font(HudStyle::Font("Bold", 16)) ]
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
                            .Text_Lambda([this] { FString Description = Controller.IsValid() ? UOpenLaunchMonitorSubsystem::GetReplayShotDescription(Controller->GetSelectedReplayShot()) : FString();
                                int32 SpeedEnd; if (Description.FindChar(TCHAR('|'), SpeedEnd))
                                    Description = GolfSettings::Speed(FCString::Atof(*Description) * .44704f) + TEXT("   ") + Description.Mid(SpeedEnd);
                                return FText::FromString(Description); })
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
    // The panel can hold keyboard focus, so it hands every play key to the controller's input, as the viewport would.
    if (!Event.IsRepeat() && Controller.IsValid() && Controller->ForwardUIKey(Event.GetKey(), true))
        return FReply::Handled();
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

FReply SReplayShotPanel::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // The release ends a held A/D or Shift.
    if (Controller.IsValid() && Controller->ForwardUIKey(Event.GetKey(), false))
        return FReply::Handled();
    return SCompoundWidget::OnKeyUp(Geometry, Event);
}
