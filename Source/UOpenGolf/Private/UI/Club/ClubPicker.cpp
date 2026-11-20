// SPDX-License-Identifier: MIT
#include "UI/Club/ClubPicker.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/HudStatusGlyph.h"
#include "UI/Common/HudToolTip.h"
#include "UI/Club/ClubGlyph.h"
#include "UI/Common/MenuStyle.h"
#include "UI/Hud/ShotDisplay.h"
#include "OpenGolfSettings.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/IToolTip.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

namespace
{
    FText ClubLabel(ELaunchMonitorClub Club)
    {
        return FText::FromString(ShotDisplay::ClubLabel(Club));
    }
}

void SClubPicker::Construct(const FArguments& Args)
{
    Monitor = Args._Monitor;
    DrawerBrush = FSlateRoundedBoxBrush(HudStyle::LaunchTileColor(), HudStyle::TileRadius);
    ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");
    ComboStyle.SetMenuBorderBrush(FSlateNoResource()).SetMenuBorderPadding(FMargin(0));
    const FMargin RowPadding(8, 4);
    RowStyle = MenuStyle::Make().Button.SetNormalPadding(RowPadding).SetPressedPadding(RowPadding);
    ToggleStyle = HudStyle::ToolbarButton();
    const auto Current = [this] { return Monitor.IsValid() ? Monitor->GetClub() : ELaunchMonitorClub::Driver; };

    const auto Tip = [Current]
    {
        return FText::Format(FText::FromString(TEXT("Select club ({0})")), UOpenLaunchMonitorSubsystem::GetClubDisplayName(Current()));
    };

    const TSharedRef<SScrollBox> Clubs = SNew(SScrollBox)
        .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
        .NavigationDestination(EDescendantScrollDestination::Center);
    TArray<TSharedRef<SButton>> Rows;
    for (int32 Index = 0; Index <= static_cast<int32>(ELaunchMonitorClub::Putter); ++Index)
    {
        const auto Club = static_cast<ELaunchMonitorClub>(Index);
        const TSharedRef<SButton> Row = SNew(SButton).ButtonStyle(&RowStyle)
#if WITH_ACCESSIBILITY
            .AccessibleText(UOpenLaunchMonitorSubsystem::GetClubDisplayName(Club))
#endif
            .OnClicked_Lambda([this, Club]
            {
                if (Monitor.IsValid()) Monitor->SetClub(Club);
                Picker->SetIsOpen(false);
                return FReply::Handled().SetUserFocus(Picker.ToSharedRef(), EFocusCause::Navigation);
            })
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [
                    SNew(SBox).WidthOverride(34).HeightOverride(24)[SNew(SClubGlyph).Club(Club)]
                ]
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(6, 0, 0, 0)
                [
                    SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).Text(ClubLabel(Club))
                    .ColorAndOpacity_Lambda([Current, Club]
                    {
                        return FSlateColor(Current() == Club ? HudStyle::AccentColor() : HudStyle::TextColor());
                    })
                ]
            ];
        Clubs->AddSlot()[Row];
        Rows.Add(Row);
    }

    ChildSlot
    [
        SAssignNew(Picker, SComboButton).ComboButtonStyle(&ComboStyle).ButtonStyle(&ToggleStyle).HasDownArrow(false)
        .ContentPadding(FMargin(0)).HAlign(HAlign_Center).VAlign(VAlign_Center)
        // No tooltip over the open drawer. A tooltip with its own content is never empty, so it is withheld instead.
        .ToolTip_Lambda([this, ToolTip = TSharedPtr<IToolTip>(HudToolTip::Make(TAttribute<FText>::CreateLambda(Tip)))]
        {
            return Picker.IsValid() && Picker->IsOpen() ? TSharedPtr<IToolTip>() : ToolTip;
        })
        .MenuPlacement(MenuPlacement_BelowAnchor)
        .OnComboBoxOpened_Lambda([this, Clubs, Current, Rows = MoveTemp(Rows)]
        {
            const int32 Selected = static_cast<int32>(Current());
            Picker->SetMenuContentWidgetToFocus(Rows[Rows.IsValidIndex(Selected) ? Selected : 0]);
            if (GolfSettings::Get().ReducedMotion)
                DrawerSlide.JumpToEnd();
            else
                DrawerSlide.Play(Clubs);
        })
#if WITH_ACCESSIBILITY
        .AccessibleText_Lambda(Tip)
#endif
        .ButtonContent()
        [
            SNew(SBox).WidthOverride(HudStyle::ToolbarButtonSize).HeightOverride(HudStyle::ToolbarButtonSize)
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Club)]
        ]
        .MenuContent()
        [
            SNew(SBox).MinDesiredWidth(96).MaxDesiredHeight(310).Clipping(EWidgetClipping::ClipToBoundsAlways)
            [
                SNew(SBorder).BorderImage(&DrawerBrush).Padding(3)
                .RenderTransform_Lambda([this]
                {
                    return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0, -18.0f * (1.0f - DrawerSlide.GetLerp()))));
                })
                [Clubs]
            ]
        ]
    ];
}
