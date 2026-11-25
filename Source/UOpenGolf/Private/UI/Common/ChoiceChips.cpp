// SPDX-License-Identifier: MIT
#include "UI/Common/ChoiceChips.h"
#include "OpenGolfAccountStyle.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/MenuStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr float ChipGap = 4.0f;
    /** Every chip is at least this wide, so short labels (3 ft, 6 ft) still make a comfortable target. */
    constexpr float MinChipWidth = 72.0f;
} // namespace

FName SChoiceChips::ChipName(FName ChipTag, int32 Index)
{
    return FName(*FString::Printf(TEXT("%s.%d"), *ChipTag.ToString(), Index));
}

void SChoiceChips::Construct(const FArguments& Args)
{
    checkf(Args._Style, TEXT("SChoiceChips needs the menu's style"));
    Style = Args._Style;
    Selected = Args._Selected;
    OnSelected = Args._OnSelected;
    // Each chip is as wide as its label needs, and the row wraps when the page is narrow, so no label is ever cut off.
    const TSharedRef<SWrapBox> Row = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(ChipGap, ChipGap));
    for (int32 Index = 0; Index < Args._Labels.Num(); ++Index)
    {
        const FText Label = Args._Labels[Index];
        auto IsPicked = [this, Index] { return Selected.Get() == Index; };
        // clang-format off
        const TSharedRef<SButton> Chip = SNew(SButton).ButtonStyle(&Style->Button).HAlign(HAlign_Center).Tag(ChipName(Args._ChipTag, Index))
#if WITH_ACCESSIBILITY
            .AccessibleText_Lambda([IsPicked, Label] { return IsPicked() ? FText::Format(NSLOCTEXT("Menu", "PickedChoice", "{0}, selected"), Label) : Label; })
#endif
            .OnClicked_Lambda([this, Index]
            {
                OnSelected.ExecuteIfBound(Index);
                return FReply::Handled();
            })
            [
                SNew(STextBlock).Text(Label).Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); })
                .ColorAndOpacity_Lambda([this, IsPicked] { return FSlateColor(IsPicked() ? Style->PrimaryLabelColor : Style->TextColor); })
            ];
        // clang-format on
        Chips.Add(Chip);
        Row->AddSlot()[SNew(SBox).MinDesiredWidth(MinChipWidth)[Chip]];
    }
    ChildSlot[Row];
    RestyleChips();
}

void SChoiceChips::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    RestyleChips();
}

void SChoiceChips::RestyleChips()
{
    // A button's style is not an attribute, so the highlight moves here when the pick changes.
    const int32 Picked = Selected.Get();
    if (Picked == StyledSelection)
        return;
    StyledSelection = Picked;
    for (int32 Index = 0; Index < Chips.Num(); ++Index)
        Chips[Index]->SetButtonStyle(Index == Picked ? &Style->PrimaryButton : &Style->Button);
}
