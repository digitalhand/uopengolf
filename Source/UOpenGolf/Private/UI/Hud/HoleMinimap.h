// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "HoleMapFrame.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SLeafWidget.h"

class AOpenFairwayBall;
struct FHudPen;
class AUOpenGolfGameMode;
class AUOpenGolfPlayerController;
class AUOpenGolfRange;

/**
 * The map inside the HUD's hole map panel (SHoleMapPanel): the range's finished top-down picture
 * (AUOpenGolfRange::GetHoleMapTexture), framed to the ball and the cup and easing between framings, with the tee, the
 * pin and the ball drawn over it. The pin carries a chip with its distance; the panel's footer shows its elevation. At
 * address the aim runs from the ball to a target ring with a chip of its distance and height change, then a thin line
 * runs on to the pin; a target on the pin merges into it. Off the view the ball becomes an arrow on the edge pointing
 * to it. A left click at address aims from the ball toward the clicked point and puts the target there; other buttons
 * pass through to the game, so a right click still cancels ball placement. Hidden (not drawn, not clickable) until the
 * range has a picture, so there is no empty frame or tooltip while it waits or under NullRHI.
 */
class SHoleMinimap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHoleMinimap) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    /** Size in Slate units before the readable-DPI scale; the width follows the picture's aspect. */
    static constexpr float Height = 300.0f;
    static constexpr float Width = float(Height * FHoleMapFrame::Aspect);

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Width, Height); }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

    /** The part of the hole shown now: clicks and marks use it. */
    const FHoleMapFrame& GetView() const { return View; }

    /** Visible once the range has a picture. Hidden, not Collapsed: SScaleBox stops updating a collapsed child. */
    EVisibility GetMapVisibility() const;

private:
    const AUOpenGolfGameMode* GetMode() const;
    const AUOpenGolfRange* GetRange() const;
    /** Eases the view toward the ball and the cup, holding it during flight; a new capture or Reduced motion snaps. */
    void UpdateView(const AUOpenGolfRange& Range, const AOpenFairwayBall& Ball, const FVector& Cup, float DeltaTime);
    /** A world position on the map, in local units of a widget of Size. */
    FVector2f ToMap(const FVector2D& World, FVector2f Size) const { return FVector2f(View.ToUV(World)) * Size; }
    /**
     * At address: the aim line from the ball's Marker to the target ring and the ring's chip, kept off PinChipBox, then a
     * thin line on to the Pin; or one line to the pin when the target is on it.
     */
    void PaintAim(const FHudPen& Pen, int32 ChipLayer, const AUOpenGolfRange& Range, const AOpenFairwayBall& Ball, FVector2f Marker,
        FVector2f Pin, const FBox2f& PinChipBox) const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    /** The range's finished picture; the range owns the texture for the whole of Play. */
    FSlateBrush Aerial;
    FSlateRoundedBoxBrush BallDot{FLinearColor::White, BallDotSize * 0.5f, HudStyle::MapInkColor(), 1.5f};
    FSlateRoundedBoxBrush ChipBrush{HudStyle::MapChipColor(), 4.0f};
    /** The view drawn now, the view it is easing to, and the capture both are part of. */
    FHoleMapFrame View;
    FHoleMapFrame Goal;
    FHoleMapFrame Capture;
    bool bHasView = false;

    static constexpr float BallDotSize = 9.0f;
    /** How far inside the edge the ball marker, or its arrow, stays. */
    static constexpr float EdgeInset = 6.0f;
    /** Ground (cm) shown beyond the ball and the cup. */
    static constexpr double ViewMarginCm = 2500.0;
    /** How quickly the view eases to a new framing, per second: about 0.4 s to settle. */
    static constexpr double ViewEaseRate = 8.0;
    static constexpr float TargetRadius = 7.0f;
    /** A target ring closer than this to the pin on the map merges into the pin's chip. */
    static constexpr float MergeDistance = 10.0f;
};
