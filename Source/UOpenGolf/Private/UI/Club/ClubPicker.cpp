// SPDX-License-Identifier: MIT
#include "UI/Club/ClubPicker.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UI/Common/HudStyle.h"
#include "UI/Club/ClubGlyph.h"
#include "UI/Common/MenuStyle.h"
#include "UI/Hud/ShotDisplay.h"
#include "OpenGolfSettings.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"

namespace ClubPickerDetail
{
    /** The club face on the closed picker, smaller than in the list so the heading keeps its room. */
    const FVector2D ButtonImageSize(42, 30);
    /** Room for the widest abbreviation (GW) at 100% text size. */
    constexpr float LabelWidth = 28.0f;
    constexpr int32 LabelSize = 12;

    FText Label(ELaunchMonitorClub Club) { return FText::FromString(ShotDisplay::ClubLabel(Club)); }
    /** The label's box grows with the Text size preference, as its font does. */
    float ScaledLabelWidth() { return LabelWidth * GolfSettings::Get().TextPercent / 100.0f; }
} // namespace ClubPickerDetail

void SClubPicker::Construct(const FArguments& Args)
{
    Monitor=Args._Monitor;
    Style=MenuStyle::Make();
    auto Items=SNew(SVerticalBox);
    for(int32 I=0;I<=static_cast<int32>(ELaunchMonitorClub::Putter);++I)
    {
        const auto Club=static_cast<ELaunchMonitorClub>(I);
        Items->AddSlot().AutoHeight()[SNew(SButton).ButtonStyle(&Style.Button).OnClicked_Lambda([this,Club]{if(Monitor.IsValid())Monitor->SetClub(Club);Picker->SetIsOpen(false);return FReply::Handled().SetUserFocus(Picker.ToSharedRef(),EFocusCause::Navigation);})
            [SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth()[SNew(SClubGlyph).Club(Club)]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6,0,0,0)
                [SNew(STextBlock).Font(HudStyle::Font("Bold",ClubPickerDetail::LabelSize)).Text(ClubPickerDetail::Label(Club))]]];
    }
    auto Current=[this]{return Monitor.IsValid()?Monitor->GetClub():ELaunchMonitorClub::Driver;};
    ChildSlot[SAssignNew(Picker,SComboButton).ButtonStyle(&Style.Button)
        .ButtonContent()[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(SBox).WidthOverride(ClubPickerDetail::ButtonImageSize.X).HeightOverride(ClubPickerDetail::ButtonImageSize.Y)
                    [SNew(SClubGlyph).Club_Lambda(Current)]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4,0,0,0)
                [SNew(SBox).WidthOverride_Lambda([]{return FOptionalSize(ClubPickerDetail::ScaledLabelWidth());})
                    [SNew(STextBlock).Font(HudStyle::Font("Bold",ClubPickerDetail::LabelSize)).Text_Lambda([Current]{return ClubPickerDetail::Label(Current());})]]]
        .MenuContent()[SNew(SBox).MaxDesiredHeight(440)[SNew(SScrollBox)+SScrollBox::Slot()[Items]]]];
}
