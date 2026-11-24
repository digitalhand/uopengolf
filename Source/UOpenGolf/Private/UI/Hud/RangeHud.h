// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/Common/HudStyle.h"
#include "UI/Hud/ShotDisplay.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class UOpenGolfWindIndicator;

/** Shot data, course header and settings, wind, version label and ball-ready glow. */
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
    /** The course header, then the shot tiles. */
    TSharedRef<SWidget> MakeColumn();
    TSharedRef<SWidget> MakeTile(int32 Index);
    /** One large course-heading line bound to Line, a member of this HUD; collapsed while it is empty. */
    TSharedRef<SWidget> MakeHoleLine(FName WidgetTag, const FText& Line);
    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TWeakObjectPtr<UOpenGolfWindIndicator> WindIndicator;
    TArray<ShotDisplay::FTile> Tiles;
    TArray<FText> TileValues, TileLabels;
    /** HoleNumber and HoleDetails are the course heading's two hole lines; both empty outside a course round. */
    FText Heading, HoleNumber, HoleDetails;
    bool bBallReady = false;
    /** Slate time of the next text rebuild; the first tick rebuilds at once. */
    double NextTextRefreshTime = 0.0;

    // Translucent gray; the raw launch-monitor tiles are a little darker.
    FSlateRoundedBoxBrush TileBrush{HudStyle::TileColor(), HudStyle::TileRadius};
    FSlateRoundedBoxBrush LaunchBrush{HudStyle::LaunchTileColor(), HudStyle::TileRadius};
    /** Total, Carry and Roll: the tile color with an accent outline. */
    FSlateRoundedBoxBrush ResultBrush{HudStyle::TileColor(),HudStyle::TileRadius,HudStyle::AccentColor().CopyWithNewOpacity(.45f),1.f};
    /** The settings cog: no background until hovered. */
    FButtonStyle CellStyle;
};
