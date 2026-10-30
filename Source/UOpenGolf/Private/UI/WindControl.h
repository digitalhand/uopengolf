// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/SlateDelegates.h"
#include "UI/HudStyle.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class SBorder;
class SOverlay;
class UOpenGolfWindIndicator;

/**
 * Wind controls on the top-centre wind indicator: a click on the indicator (or W) slides out a panel under it with
 * the wind's on/off switch, its speed (calm to 30 mph) and the direction it comes from, each set by sliding. The
 * indicator itself stays in the click-through shot HUD; this layer lies over it and follows where it is drawn.
 */
class SWindControl : public SCompoundWidget
{
public:
    /** The panel slides out, and back in, over this long. */
    static constexpr float SlideSeconds = 0.15f;
    static constexpr float PanelWidth = 280.0f;

    SLATE_BEGIN_ARGS(SWindControl) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_ARGUMENT(TWeakObjectPtr<UOpenGolfWindIndicator>, WindIndicator)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

    /** How far the panel is out, from 0 (hidden) to 1. */
    float GetOpenness() const { return Openness; }

private:
    /** Finds where the indicator is drawn, in this layer's scaled space; until it is, the Blueprint's top-centre spot. */
    void TrackIndicator();
    FMargin GetColumnPadding() const;
    TSharedRef<SWidget> MakePanel();
    TSharedRef<SWidget> MakeSliderRow(const TCHAR* Label, float MaxValue, float Step, TAttribute<float> Value, FOnFloatValueChanged OnChanged,
        TAttribute<FText> ValueText);
    FText GetDirectionText() const;
    bool IsWindOn() const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TWeakObjectPtr<UOpenGolfWindIndicator> WindIndicator;
    TSharedPtr<SOverlay> Layer;
    TSharedPtr<SBorder> ClickArea;
    /** The indicator's top-left corner and width in the layer's scaled space. */
    FVector2D IndicatorPosition = FVector2D::ZeroVector;
    float IndicatorWidth = 0.0f;
    float Openness = 0.0f;

    FSlateRoundedBoxBrush TileBrush{HudStyle::TileColor(), HudStyle::TileRadius};
    FSlateRoundedBoxBrush HoverOutline{FLinearColor::Transparent, 8.0f, HudStyle::AccentColor(), 1.5f};
};
