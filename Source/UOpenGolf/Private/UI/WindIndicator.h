// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/WindDisplay.h"
#include "Widgets/SLeafWidget.h"

class SWindIndicator : public SLeafWidget
{
public:
    /** The whole indicator: the speed, then the arrow's disc inside the panel at its right end. */
    static constexpr float PanelWidth = 176.0f;
    static constexpr float PanelHeight = 44.0f;
    static constexpr float CircleSize = 36.0f;
    static constexpr float CircleInset = (PanelHeight - CircleSize) * 0.5f;

    SLATE_BEGIN_ARGS(SWindIndicator) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& Args) {}
    void SetWind(const WindDisplay::FState& InState);
    const WindDisplay::FState& GetWind() const { return State; }
    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(PanelWidth, PanelHeight); }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
    WindDisplay::FState State;
    FSlateRoundedBoxBrush PanelBrush{FLinearColor::FromSRGBColor(FColor(38, 44, 48, 210)), 8.0f};
    FSlateRoundedBoxBrush CircleBrush{FLinearColor::White, CircleSize * 0.5f};
};
