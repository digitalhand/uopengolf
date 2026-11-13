// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SLeafWidget.h"

/** A small drawn icon for the HUD status box: the launch monitor, its battery, or the recording dot. */
class SHudStatusGlyph : public SLeafWidget
{
public:
    enum class EKind : uint8
    {
        LaunchMonitor,
        Battery,
        Record
    };

    SLATE_BEGIN_ARGS(SHudStatusGlyph) : _Kind(EKind::Record), _Color(FLinearColor::White), _Level(-1.0f), _Filled(false) {}
    SLATE_ARGUMENT(EKind, Kind)
    SLATE_ATTRIBUTE(FLinearColor, Color)
    /** Battery charge from 0 to 1; below 0 the charge is unknown and only the outline is drawn. */
    SLATE_ATTRIBUTE(float, Level)
    /** The recording dot is filled while recording and an outline otherwise. */
    SLATE_ATTRIBUTE(bool, Filled)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override;

private:
    void PaintLaunchMonitor(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint) const;
    void PaintBattery(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint) const;
    void PaintRecord(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint) const;

    EKind Kind = EKind::Record;
    TAttribute<FLinearColor> Color;
    TAttribute<float> Level;
    TAttribute<bool> Filled;

    // White fills tinted when drawn. Outlines are drawn as lines: a rounded box's outline ignores the tint.
    FSlateRoundedBoxBrush Fill{FLinearColor::White, 1.0f};
    FSlateRoundedBoxBrush Dot{FLinearColor::White, 6.0f};
};
