// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/HudStyle.h"
#include "Widgets/SLeafWidget.h"

/** An on/off switch in the HUD style: a pill in the accent colour with its knob right when on, gray with it left when off. */
class SHudSwitch : public SLeafWidget
{
public:
    static constexpr float Width = 36.0f;
    static constexpr float Height = 20.0f;

    SLATE_BEGIN_ARGS(SHudSwitch) {}
    SLATE_ATTRIBUTE(bool, IsOn)
    /** A click; the owner flips the state IsOn reads. */
    SLATE_EVENT(FSimpleDelegate, OnToggled)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Width, Height); }
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
    TAttribute<bool> IsOn;
    FSimpleDelegate OnToggled;
    bool bPressed = false;

    // White fills tinted when drawn.
    FSlateRoundedBoxBrush Pill{FLinearColor::White, Height * 0.5f};
    FSlateRoundedBoxBrush Knob{FLinearColor::White, (Height - 4.0f) * 0.5f};
};
