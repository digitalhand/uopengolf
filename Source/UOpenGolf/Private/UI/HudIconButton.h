// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/HudStyle.h"
#include "Widgets/SLeafWidget.h"

/** A round HUD button with a drawn icon: the hole flag (play mode) or a ball (place the ball). */
class SHudIconButton : public SLeafWidget
{
public:
    enum class EIcon : uint8
    {
        Flag,
        Ball
    };

    static constexpr float Size = 44.0f;

    SLATE_BEGIN_ARGS(SHudIconButton) : _Icon(EIcon::Flag) {}
    SLATE_ARGUMENT(EIcon, Icon)
    /** Draws the accent ring: the mode or tool is on. */
    SLATE_ATTRIBUTE(bool, IsActive)
    SLATE_EVENT(FSimpleDelegate, OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
    void PaintFlag(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
    void PaintBall(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

    EIcon Icon = EIcon::Flag;
    TAttribute<bool> IsActive;
    FSimpleDelegate OnClicked;
    bool bPressed = false;

    // White fills tinted when drawn; the radius makes each a circle or pill at its drawn size.
    FSlateRoundedBoxBrush Disc{FLinearColor::White, Size * 0.5f};
    FSlateRoundedBoxBrush Ring{FLinearColor::Transparent, Size * 0.5f, HudStyle::AccentColor(), 2.5f};
    FSlateRoundedBoxBrush BallFill{FLinearColor::White, 10.0f};
    FSlateRoundedBoxBrush Dot{FLinearColor::White, 1.5f};
    FSlateRoundedBoxBrush Pill{FLinearColor::White, 2.5f};
};
