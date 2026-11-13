// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/Common/HudStyle.h"
#include "UI/Hud/ShotDisplay.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class UOpenGolfWindIndicator;

/**
 * Range HUD: shot tiles down the right edge (results, club data, then the raw launch-monitor
 * values) in a column of fixed width, wind at the top center, a status box in the bottom-right corner, the game
 * version in the bottom-left corner and the ball-ready glow around the ball at address. The status box shows aim,
 * the launch monitor with its battery (click for its settings) and, on the range, the record toggle. Clicks pass through to the
 * game except on the club picker and those two status buttons, and it hides while the test-shot panel is open.
 * Nothing has a tooltip; captions say what each control does.
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
    /** The heading with the club picker, then the shot tiles. */
    TSharedRef<SWidget> MakeColumn();
    TSharedRef<SWidget> MakeTile(int32 Index);
    TSharedRef<SWidget> MakeStatusBox();
    TSharedRef<SWidget> MakeLaunchMonitorButton();
    TSharedRef<SWidget> MakeRecordButton();
    /** A status button's content: its icons over a caption, aligned with the text cells. */
    TSharedRef<SWidget> MakeIconCell(const TSharedRef<SWidget>& Icons, const TAttribute<FText>& Caption) const;
    TSharedRef<SWidget> MakeStatusCell(TAttribute<FText> Value, TAttribute<FSlateColor> Color, const TCHAR* Label) const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TWeakObjectPtr<UOpenGolfWindIndicator> WindIndicator;
    TArray<ShotDisplay::FTile> Tiles;
    TArray<FText> TileValues, TileLabels;
    FString Heading;
    FString AimText;
    ELaunchMonitorConnectionState ConnectionState = ELaunchMonitorConnectionState::Disconnected;
    /** Negative while the launch monitor reports no battery level. */
    int32 BatteryPercent = -1;
    /** The launch monitor detects the ball: the glow around it turns green. */
    bool bBallReady = false;
    /** Training on the range, the only place shots are recorded and the record toggle shows. */
    bool bRangeMode = true;
    bool bRecording = false;
    bool bSignedIn = false;
    int32 RecordedCount = 0;
    /** Recorded shots whose files could not be written yet; they are retried. */
    int32 FailedCount = 0;
    /** An unsaved-recording or launch-monitor error; empty when there is none. */
    FString ErrorText;
    /** Slate time of the next text rebuild; the first tick rebuilds at once. */
    double NextTextRefreshTime = 0.0;

    // Translucent gray; the raw launch-monitor tiles are a little darker.
    FSlateRoundedBoxBrush TileBrush{HudStyle::TileColor(), HudStyle::TileRadius};
    FSlateRoundedBoxBrush LaunchBrush{HudStyle::LaunchTileColor(), HudStyle::TileRadius};
    /** The status buttons: no background until hovered. */
    FSlateRoundedBoxBrush ResultBrush{HudStyle::TileColor(),HudStyle::TileRadius,HudStyle::AccentColor().CopyWithNewOpacity(.45f),1.f};
    FButtonStyle CellStyle;
};
