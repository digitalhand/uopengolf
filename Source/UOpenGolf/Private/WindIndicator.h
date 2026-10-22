// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "WindDisplay.h"
#include "Widgets/SLeafWidget.h"

class SWindIndicator : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SWindIndicator) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& Args) {}
    void SetWind(const WindDisplay::FState& InState);
    const WindDisplay::FState& GetWind() const { return State; }
    virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override { return FVector2D(176, 56); }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
        FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;

private:
    WindDisplay::FState State;
    FSlateRoundedBoxBrush PanelBrush{FLinearColor::FromSRGBColor(FColor(38, 44, 48, 210)), 8.0f};
    FSlateRoundedBoxBrush CircleBrush{FLinearColor::White, 24.0f};
};
