// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class SHoleMinimap;

/**
 * The HUD's hole map, bottom left: a header naming the lie with, in a course round, what it takes off the shot ("Rough
 * −8%" and "Spin −25%"), the map (SHoleMinimap), and a footer with the pin's elevation above or below the ball in feet
 * and inches. Left clicks on the header and footer are absorbed so
 * they never place the ball in the world behind them; other buttons pass through, as on the map. Hidden with the map
 * until the range has a picture.
 */
class SHoleMapPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SHoleMapPanel) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
    EVisibility GetPanelVisibility() const;
    /** A fixed-height row of the panel, tagged for tests: Left fills, Right takes its own width. */
    static TSharedRef<SWidget> MakeBar(FName Tag, float Height, const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right);
    TSharedRef<SWidget> MakeHeader();
    TSharedRef<SWidget> MakeFooter();
    /** Rebuilds the header and footer text when what it shows has changed. */
    void Refresh();

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TSharedPtr<SHoleMinimap> Map;
    FSlateRoundedBoxBrush PanelBrush{HudStyle::MapPanelColor(), HudStyle::TileRadius + 2.0f, HudStyle::MapEdgeColor(), 1.0f};
    FSlateRoundedBoxBrush ShadowBrush{HudStyle::MapShadowColor(), HudStyle::TileRadius + 3.0f};

    FText Lie, LieCut, SpinCut, Elevation;
};
