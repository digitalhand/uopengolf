// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "HoleMapFrame.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SLeafWidget.h"

class AUOpenGolfGameMode;
class AUOpenGolfPlayerController;
class AUOpenGolfRange;

/**
 * The HUD's hole map, bottom left: the range's top-down capture (AUOpenGolfRange::GetHoleMapTexture) with the tee, the
 * pin flag and the ball drawn over it. At address the ball shows its aim line; off the map it becomes an arrow on the
 * edge pointing to it. A left click at address aims from the ball toward the clicked point; other buttons pass through
 * to the game, so a right click still cancels ball placement. Hidden (not drawn, not clickable) until the range has a
 * picture, so there is no empty frame or tooltip while it waits or under NullRHI.
 */
class SHoleMinimap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SHoleMinimap) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    /** Height in Slate units before the readable-DPI scale; the width follows the picture's aspect. */
    static constexpr float Height = 232.0f;

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Height * FHoleMapFrame::Aspect, Height); }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
    const AUOpenGolfGameMode* GetMode() const;
    const AUOpenGolfRange* GetRange() const;
    EVisibility GetMapVisibility() const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    /** The range's capture; the range owns the texture for the whole of Play. */
    FSlateBrush Aerial;
    FSlateRoundedBoxBrush BallDot{FLinearColor::White, BallDotSize * 0.5f, HudStyle::MapInkColor(), 1.5f};

    static constexpr float BallDotSize = 9.0f;
    /** The aim line's length on the map, whatever the zoom. */
    static constexpr float AimLineLength = 19.0f;
    /** How far inside the edge the ball marker, or its arrow, stays. */
    static constexpr float EdgeInset = 6.0f;
};
