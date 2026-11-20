// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Widgets/SLeafWidget.h"

/** Small line icons for the HUD buttons. */
class SHudStatusGlyph : public SLeafWidget
{
public:
    enum class EKind : uint8
    {
        Settings,
        Record,
        Club,
        Flag,
        Ball,
        Tee,
        Exit
    };

    SLATE_BEGIN_ARGS(SHudStatusGlyph) : _Kind(EKind::Record), _Color(FLinearColor::White), _Filled(false) {}
    SLATE_ARGUMENT(EKind, Kind)
    SLATE_ATTRIBUTE(FLinearColor, Color)
    /** The recording dot is filled while recording and an outline otherwise. */
    SLATE_ATTRIBUTE(bool, Filled)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override;
    static constexpr float Size = 16.0f;
    /** Opacity of an icon on a disabled control. */
    static constexpr float DisabledAlpha = 0.4f;
    static void Paint(EKind Kind, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
        const FLinearColor& Tint, bool bFilled = false);

private:
    EKind Kind = EKind::Record;
    TAttribute<FLinearColor> Color;
    TAttribute<bool> Filled;

};
