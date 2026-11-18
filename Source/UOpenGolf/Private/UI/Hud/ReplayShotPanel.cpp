// SPDX-License-Identifier: MIT
#include "UI/Hud/ReplayShotPanel.h"
#include "UI/Common/MenuStyle.h"
#include "UI/Common/HudStyle.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Framework/Application/SlateApplication.h"
#include "UI/Common/HudScale.h"
#include "InputCoreTypes.h"
#include "OpenGolfSettings.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UOpenGolfPlayerController.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace ReplayShotPanelDetail
{
    const TCHAR* const CustomLabel = TEXT("Custom shot");
    const TCHAR* const FieldLabels[SReplayShotPanel::CustomFieldCount] = {
        TEXT("Ball speed"), TEXT("Launch angle"), TEXT("Direction"), TEXT("Total spin"), TEXT("Spin axis")};

    /** Up to two decimals, without trailing zeros, so a typed value reads back as typed. */
    FString Number(double Value)
    {
        return FString::SanitizeFloat(FMath::RoundToDouble(Value * 100.0) / 100.0, 0);
    }
} // namespace ReplayShotPanelDetail

FName SReplayShotPanel::CustomFieldTag(int32 Field)
{
    static const TCHAR* const Tags[CustomFieldCount] = {
        TEXT("CustomShotSpeed"), TEXT("CustomShotLaunch"), TEXT("CustomShotDirection"), TEXT("CustomShotSpin"), TEXT("CustomShotAxis")};
    return Field >= 0 && Field < CustomFieldCount ? FName(Tags[Field]) : NAME_None;
}

void SReplayShotPanel::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    Style=MenuStyle::Make();
    SetTag(TEXT("TestShotControls"));
    // The panel spans the viewport over the play controls: only its button and open panel take clicks.
    SetVisibility(TAttribute<EVisibility>::CreateLambda([this]{return Controller.IsValid() && Controller->IsMainMenuOpen() ? EVisibility::Collapsed : EVisibility::SelfHitTestInvisible;}));
    for (const FString& Name : UOpenLaunchMonitorSubsystem::GetReplayShotNames())
    {
        Options.Add(MakeShared<FString>(Name));
    }
    const FSlateFontInfo BodyFont = HudStyle::Font("Regular", 11);
    TSharedRef<SVerticalBox> CustomRows = SNew(SVerticalBox);
    for (int32 Field = 0; Field < CustomFieldCount; ++Field)
        CustomRows->AddSlot().AutoHeight().Padding(0, 0, 0, 6)[MakeFieldRow(Field)];
    // Preserve Slate's layout syntax, which clang-format otherwise collapses.
    // clang-format off
    CustomRows->AddSlot().AutoHeight().Padding(0, 4, 0, 0)
    [
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [ SNew(STextBlock).Font(BodyFont).AutoWrapText(true).Text(FText::FromString(TEXT("Negative direction / axis = left; positive = right."))) ]
        + SHorizontalBox::Slot().AutoWidth()
        [
            SNew(SButton).ButtonStyle(&Style.Button).IsFocusable(true).Text(FText::FromString(TEXT("Straight shot")))
            .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->ResetCustomShot(); return FReply::Handled(); })
        ]
    ];
    ChildSlot
    [
        SNew(SDPIScaler).DPIScale_Lambda([this] { return HudScale::Readable(Controller.IsValid() ? Controller->GetWorld() : nullptr); })
        [
            SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(20, 0, 20, HudScale::BottomPanelGap)
            [
                SNew(SButton).ButtonStyle(&Style.Button).IsFocusable(true)
                // Test shots are for playing without a launch monitor.
                .Visibility_Lambda([this] { return Controller.IsValid() && Controller->CanUseManualShots() && !Controller->IsReplayPanelOpen() && !IsLaunchMonitorConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
                .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->SetReplayPanelOpen(true); return FReply::Handled(); })
                [ SNew(STextBlock).Font(BodyFont).Text(FText::FromString(TEXT("Test shots [P]   |   Hit [H]"))) ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(20, 0, 20, HudScale::BottomPanelGap)
            [
                SNew(SButton).ButtonStyle(&Style.Button)
                .Visibility_Lambda([this] { return Controller.IsValid() && !Controller->CanUseManualShots() && !IsLaunchMonitorConnected() ? EVisibility::Visible : EVisibility::Collapsed; })
                .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->OpenLaunchMonitorSettings(); return FReply::Handled(); })
                [ SNew(STextBlock).Font(BodyFont).Text(FText::FromString(TEXT("Connect a launch monitor to hit shots"))) ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(SBox).WidthOverride(520)
                .Visibility_Lambda([this] { return Controller.IsValid() && Controller->CanUseManualShots() && Controller->IsReplayPanelOpen() ? EVisibility::Visible : EVisibility::Collapsed; })
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
                                SNew(SButton).ButtonStyle(&Style.Button).IsFocusable(true).Text(FText::FromString(TEXT("Close [P]")))
                                .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->SetReplayPanelOpen(false); return FReply::Handled(); })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
                        [
                            SNew(SHorizontalBox)
                            + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, MenuStyle::ButtonGap, 0)
                            [
                                SAssignNew(ShotList, SComboBox<TSharedPtr<FString>>)
                                .OptionsSource(&Options).MaxListHeight(210)
                                .OnComboBoxOpening_Lambda([this]
                                {
                                    if (!Controller.IsValid())
                                        return;
                                    // The custom shot has no entry in the list, so any preset picked there is a change.
                                    if (Controller->IsCustomShotSelected())
                                        ShotList->ClearSelection();
                                    else if (Options.IsValidIndex(Controller->GetSelectedReplayShot()))
                                        ShotList->SetSelectedItem(Options[Controller->GetSelectedReplayShot()]);
                                })
                                .OnGenerateWidget_Lambda([BodyFont](TSharedPtr<FString> Item) { return SNew(STextBlock).Font(BodyFont).Text(FText::FromString(*Item)); })
                                // A cleared selection passes no item, which selects nothing.
                                .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type) { if (Controller.IsValid()) Controller->SelectReplayShot(Options.IndexOfByKey(Item)); })
                                [ SNew(STextBlock).Font(BodyFont).Text(this, &SReplayShotPanel::SelectedName) ]
                            ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [
                                SNew(SButton).ButtonStyle(&Style.Button).IsFocusable(true).Text(FText::FromString(TEXT("Custom")))
                                .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->SelectCustomShot(); return FReply::Handled(); })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                        [
                            SNew(STextBlock).Font(BodyFont).WrapTextAt(488)
                            .Visibility_Lambda([this] { return IsCustom() ? EVisibility::Collapsed : EVisibility::Visible; })
                            .Text_Lambda([this] { FString Description = Controller.IsValid() ? UOpenLaunchMonitorSubsystem::GetReplayShotDescription(Controller->GetSelectedReplayShot()) : FString();
                                int32 SpeedEnd; if (Description.FindChar(TCHAR('|'), SpeedEnd))
                                    Description = GolfSettings::Speed(FCString::Atof(*Description) * .44704f) + TEXT("   ") + Description.Mid(SpeedEnd);
                                return FText::FromString(Description); })
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
                        [
                            SNew(SBox).Visibility_Lambda([this] { return IsCustom() ? EVisibility::Visible : EVisibility::Collapsed; })
                            [ CustomRows ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                        [
                            SNew(SButton).ButtonStyle(&Style.Button).IsFocusable(true).OnClicked(this, &SReplayShotPanel::HitShot)
                            .Text_Lambda([this] { return FText::FromString(IsCustom() ? TEXT("Hit custom shot [H]") : TEXT("Hit selected shot [H]")); })
                        ]
                    ]
                ]
            ]
        ]
    ];
    // clang-format on
}

TSharedRef<SWidget> SReplayShotPanel::MakeFieldRow(int32 Field)
{
    const FSlateFontInfo BodyFont = HudStyle::Font("Regular", 11);
    // clang-format off
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [ SNew(SBox).WidthOverride(130)[ SNew(STextBlock).Font(BodyFont).Text(FText::FromString(ReplayShotPanelDetail::FieldLabels[Field])) ] ]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [
            SAssignNew(Fields[Field], SEditableTextBox).Style(&Style.TextField.GetValue()).Font(BodyFont).Tag(CustomFieldTag(Field))
            .SelectAllTextWhenFocused(true).ClearKeyboardFocusOnCommit(false)
            .Text(this, &SReplayShotPanel::FieldText, Field)
            .OnTextCommitted_Lambda([this, Field](const FText& Text, ETextCommit::Type Commit)
            {
                CommitField(Field, Text);
                // Enter hands the keys back to play, so H hits the new value at once.
                if (Commit == ETextCommit::OnEnter && FSlateApplication::IsInitialized())
                    FSlateApplication::Get().SetAllUserFocusToGameViewport();
            })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
        [
            SNew(SBox).WidthOverride(48)
            [ SNew(STextBlock).Font(BodyFont).Text_Lambda([Field] { return FText::FromString(Field == 0 ? GolfSettings::SpeedUnit() : Field == 3 ? FString(TEXT("rpm")) : FString(TEXT("deg"))); }) ]
        ];
    // clang-format on
}

FText SReplayShotPanel::FieldText(int32 Field) const
{
    using ReplayShotPanelDetail::Number;
    const FOpenGolfCustomShot Shot = Controller.IsValid() ? Controller->GetCustomShot() : FOpenGolfCustomShot();
    switch (Field)
    {
    case 0:
        return FText::FromString(Number(Shot.BallSpeedMps * GolfSettings::SpeedFactor()));
    case 1:
        return FText::FromString(Number(Shot.VerticalLaunchDeg));
    case 2:
        return FText::FromString(Number(Shot.HorizontalLaunchDeg));
    case 3:
        return FText::FromString(FString::FromInt(Shot.TotalSpinRpm));
    default:
        return FText::FromString(Number(Shot.SpinAxisDeg));
    }
}

void SReplayShotPanel::CommitField(int32 Field, const FText& Text)
{
    if (!Controller.IsValid() || Field < 0 || Field >= CustomFieldCount)
        return;
    const FString Entry = Text.ToString().TrimStartAndEnd();
    double Value = 0.0;
    if (Entry.IsNumeric() && LexTryParseString(Value, *Entry))
    {
        // Far past any limit already, and keeps the conversions finite; SetCustomShot applies the real limits.
        Value = FMath::Clamp(Value, -1.0e6, 1.0e6);
        FOpenGolfCustomShot Shot = Controller->GetCustomShot();
        switch (Field)
        {
        case 0:
            Shot.BallSpeedMps = static_cast<float>(Value / GolfSettings::SpeedFactor());
            break;
        case 1:
            Shot.VerticalLaunchDeg = static_cast<float>(Value);
            break;
        case 2:
            Shot.HorizontalLaunchDeg = static_cast<float>(Value);
            break;
        case 3:
            Shot.TotalSpinRpm = static_cast<int32>(FMath::RoundToDouble(Value));
            break;
        default:
            Shot.SpinAxisDeg = static_cast<float>(Value);
            break;
        }
        Controller->SetCustomShot(Shot);
    }
    // Shows what was stored: the value as limited, or the previous one when the text was not a number.
    if (Fields[Field].IsValid())
        Fields[Field]->SetText(TAttribute<FText>::CreateSP(this, &SReplayShotPanel::FieldText, Field));
}

FText SReplayShotPanel::SelectedName() const
{
    if (IsCustom())
        return FText::FromString(ReplayShotPanelDetail::CustomLabel);
    const int32 Index = Controller.IsValid() ? Controller->GetSelectedReplayShot() : INDEX_NONE;
    return FText::FromString(Options.IsValidIndex(Index) ? *Options[Index] : FString());
}

bool SReplayShotPanel::IsCustom() const
{
    return Controller.IsValid() && Controller->IsCustomShotSelected();
}

bool SReplayShotPanel::IsEditingField() const
{
    for (const TSharedPtr<SEditableTextBox>& Field : Fields)
        if (Field.IsValid() && Field->HasKeyboardFocus())
            return true;
    return false;
}

bool SReplayShotPanel::IsLaunchMonitorConnected() const
{
    const UGameInstance* GameInstance = Controller.IsValid() ? Controller->GetGameInstance() : nullptr;
    const UOpenLaunchMonitorSubsystem* LaunchMonitor = GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    return LaunchMonitor && LaunchMonitor->GetConnectionState() != ELaunchMonitorConnectionState::Disconnected;
}

FReply SReplayShotPanel::HitShot()
{
    // A value still being typed counts, as if Enter had been pressed.
    for (int32 Field = 0; Field < CustomFieldCount; ++Field)
        if (Fields[Field].IsValid() && Fields[Field]->HasKeyboardFocus())
            CommitField(Field, Fields[Field]->GetText());
    if (Controller.IsValid())
        Controller->InjectReplayShot();
    return FReply::Handled();
}

FReply SReplayShotPanel::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (IsEditingField())
    {
        // Typing a value never presses a play key (H would hit, Tab change club); Tab moves between the fields.
        if (Event.GetKey() == EKeys::Tab)
        {
            int32 Focused = 0;
            while (Focused < CustomFieldCount - 1 && !(Fields[Focused].IsValid() && Fields[Focused]->HasKeyboardFocus()))
                ++Focused;
            const int32 Next = (Focused + (Event.IsShiftDown() ? CustomFieldCount - 1 : 1)) % CustomFieldCount;
            if (Fields[Next].IsValid())
                FSlateApplication::Get().SetKeyboardFocus(Fields[Next], EFocusCause::Navigation);
        }
        return FReply::Handled();
    }
    // The panel can hold keyboard focus, so it hands every play key to the controller's input, as the viewport would.
    if (!Event.IsRepeat() && Controller.IsValid() && Controller->ForwardUIKey(Event.GetKey(), true))
        return FReply::Handled();
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

FReply SReplayShotPanel::OnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // The release ends a held A/D or Shift, even one pressed before a field took focus.
    if (Controller.IsValid() && Controller->ForwardUIKey(Event.GetKey(), false))
        return FReply::Handled();
    return SCompoundWidget::OnKeyUp(Geometry, Event);
}
