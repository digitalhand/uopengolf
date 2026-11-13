// SPDX-License-Identifier: MIT
#include "UI/Common/PageHeader.h"

#include "SOpenGolfIconButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

TSharedRef<SWidget> PageHeader::Make(const FOpenGolfAccountStyle& Style, const TAttribute<FText>& Title, FSimpleDelegate OnBack, TSharedRef<SWidget> Right)
{
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 16, 0)
        [
            SNew(SOpenGolfIconButton)
                .Style(Style)
                .Icon(EOpenGolfAccountIcon::Back)
                .Tag(BackTag())
                .Label(FText::FromString(TEXT("Back")))
                .OnClicked(OnBack)
        ]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Text(Title).Font(Style.GetFont(Style.TitleSize)).ColorAndOpacity(Style.TextColor)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            Right
        ];
}
