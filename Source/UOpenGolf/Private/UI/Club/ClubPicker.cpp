// SPDX-License-Identifier: MIT
#include "UI/Club/ClubPicker.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
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

namespace ClubPickerDetail
{
    /** Between the club's label and the button's right edge; the icon's own box gives the gap on its left. */
    constexpr float NameRightPadding = 12.0f;

    /** The short label (DR, 7i, PW); screen readers still hear the full name. */
    FText ClubLabel(ELaunchMonitorClub Club)
    {
        return FText::FromString(ShotDisplay::ClubLabel(Club));
    }

    /** The course tiles' number font, so the club reads at the size of Stroke and To Pin. */
    FSlateFontInfo NameFont() { return HudStyle::ValueFont(); }

    /** The widest club label in NameFont, so the button keeps its width as the club changes; 0 without a renderer. */
    float WidestNameWidth()
    {
        if (!FSlateApplication::IsInitialized() || !FSlateApplication::Get().GetRenderer())
            return 0.0f;
        const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        float Widest = 0.0f;
        for (int32 Index = 0; Index <= static_cast<int32>(ELaunchMonitorClub::Putter); ++Index)
            Widest = FMath::Max(Widest, static_cast<float>(Measure->Measure(ClubLabel(static_cast<ELaunchMonitorClub>(Index)), NameFont()).X));
        return FMath::CeilToFloat(Widest);
    }
} // namespace ClubPickerDetail

void SClubPicker::Construct(const FArguments& Args)
{
    using namespace ClubPickerDetail;
    Monitor = Args._Monitor;
    DrawerBrush = FSlateRoundedBoxBrush(HudStyle::LaunchTileColor(), HudStyle::TileRadius);
    ComboStyle = FCoreStyle::Get().GetWidgetStyle<FComboButtonStyle>("ComboButton");
    ComboStyle.SetMenuBorderBrush(FSlateNoResource()).SetMenuBorderPadding(FMargin(0));
    const FMargin RowPadding(8, 4);
    RowStyle = MenuStyle::Make().Button.SetNormalPadding(RowPadding).SetPressedPadding(RowPadding);
    ToggleStyle = HudStyle::ToolbarButton();
    const auto Current = [this] { return Monitor.IsValid() ? Monitor->GetClub() : ELaunchMonitorClub::Driver; };

    // Screen readers also hear the current club; the tooltip names the button only.
    const auto AccessibleName = [Current]
    {
        return FText::Format(FText::FromString(TEXT("Change club ({0})")), UOpenLaunchMonitorSubsystem::GetClubDisplayName(Current()));
    };

    const TSharedRef<SScrollBox> Clubs = SNew(SScrollBox)
        .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
        .NavigationDestination(EDescendantScrollDestination::Center);
    TArray<TSharedRef<SButton>> Rows;
    for (int32 Index = 0; Index <= static_cast<int32>(ELaunchMonitorClub::Putter); ++Index)
    {
        const auto Club = static_cast<ELaunchMonitorClub>(Index);
        // A row picks on the press. A press focuses the row and the list centres the focused row, so a row low in the list
        // used to slide out from under the pointer before the release and need a second click. A touch still pans.
        const TSharedRef<SButton> Row = SNew(SButton).ButtonStyle(&RowStyle)
            .ClickMethod(EButtonClickMethod::MouseDown).TouchMethod(EButtonTouchMethod::PreciseTap)
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
        .ToolTip_Lambda([this, ToolTip = TSharedPtr<IToolTip>(HudToolTip::Make(NSLOCTEXT("ClubPicker", "ChangeClub", "Change Club")))]
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
        .AccessibleText_Lambda(AccessibleName)
#endif
        .ButtonContent()
        [
            // The selected club, chosen by hand or by Auto club, is labelled beside the icon, so it reads without opening
            // the drawer.
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth()
            [
                SNew(SBox).WidthOverride(HudStyle::ToolbarButtonSize).HeightOverride(HudStyle::ToolbarButtonSize)
                .HAlign(HAlign_Center).VAlign(VAlign_Center)
                [SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Club)]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, NameRightPadding, 0)
            [
                SNew(SBox).MinDesiredWidth(WidestNameWidth())
                [
                    SNew(STextBlock).Tag(TEXT("ClubPickerName")).Font(NameFont()).ColorAndOpacity(HudStyle::TextColor())
                    .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor)
                    .Text_Lambda([Current] { return ClubLabel(Current()); })
                ]
            ]
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
