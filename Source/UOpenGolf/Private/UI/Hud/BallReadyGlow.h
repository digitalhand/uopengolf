// SPDX-License-Identifier: MIT
#pragma once

#include "Widgets/SLeafWidget.h"

class AUOpenGolfPlayerController;

/**
 * A soft glow hugging the ball's outline while it waits at address. It is red until the launch monitor detects the
 * ball and green once it does (HudStyle::BallReadyColor). The HUD draws it over the ball's on-screen silhouette, so it
 * needs no material and follows every camera move. It hides while the ball moves, while it waits to be teed up
 * again, while a ball is being placed and on a practice hole. Fills its parent; tagged BallReadyGlow.
 */
class SBallReadyGlow : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SBallReadyGlow) : _Ready(false) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    /** Whether the launch monitor reports the ball detected and ready. */
    SLATE_ATTRIBUTE(bool, Ready)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D::ZeroVector; }

private:
    /** Shown while the ball is at address, off a practice hole, and no ball is being placed. */
    EVisibility GetGlowVisibility() const;
    /** The ball's on-screen centre and radius in this widget's local units; false when it is behind the camera. */
    bool FindBall(const FGeometry& Geometry, FVector2D& OutCentre, float& OutRadius) const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TAttribute<bool> Ready;
};
