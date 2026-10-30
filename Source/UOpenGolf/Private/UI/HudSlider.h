// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/SlateDelegates.h"
#include "UI/HudStyle.h"
#include "Widgets/SLeafWidget.h"

/**
 * A HUD slider in the tile style: a thin track filled in the accent colour up to the value, and a round thumb. Click
 * or drag anywhere on it to set the value, in Step increments. Dims and ignores the mouse while disabled.
 */
class SHudSlider : public SLeafWidget
{
public:
    static constexpr float Width = 150.0f;
    static constexpr float Height = 20.0f;
    static constexpr float ThumbSize = 14.0f;

    SLATE_BEGIN_ARGS(SHudSlider) : _MinValue(0.0f), _MaxValue(1.0f), _Step(0.0f), _Value(0.0f) {}
    SLATE_ARGUMENT(float, MinValue)
    SLATE_ARGUMENT(float, MaxValue)
    /** Values are MinValue plus whole steps; 0 slides smoothly. */
    SLATE_ARGUMENT(float, Step)
    SLATE_ATTRIBUTE(float, Value)
    SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    /** The value under LocalX on a slider WidgetWidth wide: the thumb's travel stops half a thumb from each end. */
    static float ValueAt(float LocalX, float WidgetWidth, float MinValue, float MaxValue, float Step);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Width, Height); }
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
    void SetFromPointer(const FGeometry& Geometry, const FPointerEvent& Event);

    float MinValue = 0.0f;
    float MaxValue = 1.0f;
    float Step = 0.0f;
    TAttribute<float> Value;
    FOnFloatValueChanged OnValueChanged;

    // White fills tinted when drawn.
    FSlateRoundedBoxBrush Track{FLinearColor::White, 2.0f};
    FSlateRoundedBoxBrush Thumb{FLinearColor::White, ThumbSize * 0.5f};
};
