// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/HudStyle.h"
#include "UI/HudIconButton.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;

/**
 * Play controls in the top-left corner, in the same tiles as the shot HUD: the hole flag switches between the range and
 * playing the hole (G); while playing, a ball button places the ball (B) and a row of tiles shows the hole, strokes,
 * distance to the pin and putts. A banner names the score when the ball drops. Hidden with the test-shot panel.
 */
class SPlayModeBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPlayModeBar) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    void Refresh();
    EVisibility GetBarVisibility() const;
    bool IsHoleMode() const { return bHoleMode; }
    TSharedRef<SWidget> MakeIconButton(SHudIconButton::EIcon Icon, TAttribute<bool> IsActive, FSimpleDelegate OnClicked, TAttribute<FText> Caption);
    TSharedRef<SWidget> MakeTile(TAttribute<FText> Value, const TCHAR* Label);

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    bool bHoleMode = false;
    bool bPlacing = false;
    FText HoleText;
    FText StrokeText;
    FText ToPinText;
    FText PuttsText;
    FText OverCupText;
    FText BannerTitle;
    FText BannerDetail;
    float BannerOpacity = 0.0f;

    FSlateRoundedBoxBrush TileBrush{HudStyle::TileColor(), HudStyle::TileRadius};
    FSlateRoundedBoxBrush BannerBrush{HudStyle::LaunchTileColor(), 8.0f};
};
