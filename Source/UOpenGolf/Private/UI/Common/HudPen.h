// SPDX-License-Identifier: MIT
#pragma once

#include "Layout/Geometry.h"
#include "Rendering/DrawElements.h"

/**
 * Draws tinted polylines, arcs and boxes on one layer in a widget's local units, for HUD widgets that paint themselves
 * (the status glyphs and the hole minimap). Points are FVector2f, Slate's own type, so lines are moved into the draw
 * list rather than converted.
 */
struct FHudPen
{
    const FGeometry& Geometry;
    FSlateWindowElementList& Elements;
    int32 Layer;
    FLinearColor Tint = FLinearColor::White;
    float Stroke = 1.5f;

    void Line(TArray<FVector2f> Points) const { Line(MoveTemp(Points), Tint, Stroke); }

    void Line(TArray<FVector2f> Points, const FLinearColor& Color, float Width) const
    {
        FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Color, true, Width);
    }

    /** A line drawn twice for contrast on any background: Ink at InkWidth on this layer, Fill at FillWidth on the next. */
    void OutlinedLine(TArray<FVector2f> Points, const FLinearColor& Ink, float InkWidth, const FLinearColor& Fill, float FillWidth) const
    {
        Line(Points, Ink, InkWidth);
        FSlateDrawElement::MakeLines(Elements, Layer + 1, Geometry.ToPaintGeometry(), MoveTemp(Points), ESlateDrawEffect::None, Fill, true, FillWidth);
    }

    /** Part of a circle, from one angle to another in degrees (0 points right, clockwise on screen). */
    void Arc(FVector2f Center, float Radius, float FromDeg, float ToDeg) const
    {
        constexpr int32 Steps = 16;
        TArray<FVector2f> Points;
        Points.Reserve(Steps + 1);
        for (int32 Step = 0; Step <= Steps; ++Step)
        {
            const float Angle = FMath::DegreesToRadians(FMath::Lerp(FromDeg, ToDeg, float(Step) / Steps));
            Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
        }
        Line(MoveTemp(Points));
    }

    /** Brush at (X, Y), W by H, tinted. */
    void Box(const FSlateBrush& Brush, float X, float Y, float W, float H) const
    {
        FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), &Brush,
            ESlateDrawEffect::None, Tint);
    }
};
