// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "ShotDisplay.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class UOpenGolfWindIndicator;

/**
 * Range HUD: shot tiles down the right edge (results, club data, then the raw launch-monitor
 * values), wind at the top center, a launch-monitor status box in the bottom-right corner and the game version in the
 * bottom-left corner. Clicks pass through it,
 * and it hides while the test-shot panel is open.
 */
class SRangeHud : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRangeHud) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_ARGUMENT(TWeakObjectPtr<UOpenGolfWindIndicator>, WindIndicator)
    SLATE_ARGUMENT(TSharedPtr<SWidget>, WindContent)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    /** Updates the wind arrow, and with bText reads the ball and launch monitor into the copies the text bindings read. */
    void Refresh(bool bText);
    EVisibility GetHudVisibility() const;
    TSharedRef<SWidget> MakeTile(int32 Index);
    TSharedRef<SWidget> MakeStatusCell(TAttribute<FText> Value, TAttribute<FSlateColor> Color, const TCHAR* Label) const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TWeakObjectPtr<UOpenGolfWindIndicator> WindIndicator;
    TArray<ShotDisplay::FTile> Tiles;
    FString Heading;
    FString ClubName;
    FText ConnectionText;
    FLinearColor ConnectionColor = FLinearColor::White;
    FString BatteryText;
    FString AimText;
    FString RecordText;
    FLinearColor RecordColor = FLinearColor::White;
    /** Recording only applies while a launch monitor is connected. */
    bool bShowRecord = false;
    FString KeyTip;
    /** Slate time of the next text rebuild; the first tick rebuilds at once. */
    double NextTextRefreshTime = 0.0;

    // Translucent gray; the raw launch-monitor tiles are a little darker.
    FSlateRoundedBoxBrush TileBrush{FLinearColor::FromSRGBColor(FColor(72, 72, 76, 170)), 5.0f};
    FSlateRoundedBoxBrush LaunchBrush{FLinearColor::FromSRGBColor(FColor(44, 44, 48, 190)), 5.0f};
};
