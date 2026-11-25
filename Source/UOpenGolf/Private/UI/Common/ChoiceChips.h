// SPDX-License-Identifier: MIT
#pragma once

#include "Framework/SlateDelegates.h"
#include "Widgets/SCompoundWidget.h"

class SButton;
struct FOpenGolfAccountStyle;

/**
 * A row of buttons with one picked, for a short menu choice (Wind: None, Calm, Windy). Each button is as wide as its
 * label, and the row wraps on a narrow page. The picked button has the menu's primary style; each button is tagged
 * <Tag>.<index> for tests and tools.
 */
class SChoiceChips : public SCompoundWidget
{
public:
    // Pointer arguments need a default: SLATE_ARGUMENT leaves them uninitialized when a caller omits them.
    SLATE_BEGIN_ARGS(SChoiceChips) : _Style(nullptr), _Selected(INDEX_NONE) {}
    /** The menu's button and text style; it must outlive the chips. */
    SLATE_ARGUMENT(const FOpenGolfAccountStyle*, Style)
    SLATE_ARGUMENT(TArray<FText>, Labels)
    /** The picked index; INDEX_NONE picks none. */
    SLATE_ATTRIBUTE(int32, Selected)
    SLATE_ARGUMENT(FName, ChipTag)
    SLATE_EVENT(FOnInt32ValueChanged, OnSelected)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

    /** The tag of chip Index under ChipTag. */
    static FName ChipName(FName ChipTag, int32 Index);

private:
    void RestyleChips();

    const FOpenGolfAccountStyle* Style = nullptr;
    TAttribute<int32> Selected;
    FOnInt32ValueChanged OnSelected;
    TArray<TSharedRef<SButton>> Chips;
    int32 StyledSelection = INDEX_NONE - 1;
};
