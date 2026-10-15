// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "ShotDisplay.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;

/**
 * Range HUD: shot tiles down the right edge (results, club data, then the raw launch-monitor
 * values) and a launch-monitor status box in the bottom-right corner. Clicks pass through it,
 * and it hides while the test-shot panel is open.
 */
class SRangeHud : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRangeHud) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    /** Reads the ball and the launch monitor once per frame; the text bindings read these copies. */
    void Refresh();
    EVisibility GetHudVisibility() const;
    TSharedRef<SWidget> MakeTile(int32 Index);
    TSharedRef<SWidget> MakeStatusCell(TAttribute<FText> Value, TAttribute<FSlateColor> Color, const TCHAR* Label) const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TArray<ShotDisplay::FTile> Tiles;
    FString Heading;
    FString ClubName;
    FText ConnectionText;
    FLinearColor ConnectionColor = FLinearColor::White;
    FString BatteryText;
    FString RecordText;
    FLinearColor RecordColor = FLinearColor::White;
    /** Recording only applies while a launch monitor is connected. */
    bool bShowRecord = false;
    FString KeyTip;

    // Translucent gray; the raw launch-monitor tiles are a little darker.
    FSlateRoundedBoxBrush TileBrush{FLinearColor::FromSRGBColor(FColor(72, 72, 76, 170)), 5.0f};
    FSlateRoundedBoxBrush LaunchBrush{FLinearColor::FromSRGBColor(FColor(44, 44, 48, 190)), 5.0f};
};
