// SPDX-License-Identifier: MIT
#include "UI/Common/HudStatusGlyph.h"

#include "Rendering/DrawElements.h"

namespace HudStatusGlyphDetail
{
    constexpr float Stroke = 1.5f;

    /** Draws a tinted polyline in the glyph's local units. */
    struct FPen
    {
        const FGeometry& Geometry;
        FSlateWindowElementList& Elements;
        int32 Layer;
        FLinearColor Tint;

        void Line(const TArray<FVector2f>& Points) const
        {
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Stroke);
        }
        /** Part of a circle, from one angle to another in degrees (0 points right, clockwise on screen). */
        void Arc(FVector2f Center, float Radius, float FromDeg, float ToDeg) const
        {
            TArray<FVector2f> Points;
            constexpr int32 Steps = 16;
            for (int32 Step = 0; Step <= Steps; ++Step)
            {
                const float Angle = FMath::DegreesToRadians(FMath::Lerp(FromDeg, ToDeg, float(Step) / Steps));
                Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
            }
            Line(Points);
        }
        void Box(const FSlateBrush& Brush, float X, float Y, float W, float H) const
        {
            FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), &Brush,
                ESlateDrawEffect::None, Tint);
        }
    };
} // namespace HudStatusGlyphDetail

void SHudStatusGlyph::Construct(const FArguments& Args)
{
    Kind = Args._Kind;
    Color = Args._Color;
    Filled = Args._Filled;
}

FVector2D SHudStatusGlyph::ComputeDesiredSize(float) const
{
    return FVector2D(Size, Size);
}

int32 SHudStatusGlyph::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    FLinearColor Tint = Color.Get(FLinearColor::White) * WidgetStyle.GetColorAndOpacityTint();
    Tint.A *= bParentEnabled ? 1.0f : DisabledAlpha;
    Paint(Kind, Geometry, OutDrawElements, LayerId, Tint, Filled.Get(false));
    return LayerId + 1;
}

void SHudStatusGlyph::Paint(EKind Kind, const FGeometry& Geometry, FSlateWindowElementList& Elements, int32 Layer,
    const FLinearColor& Tint, bool bFilled)
{
    const HudStatusGlyphDetail::FPen Pen{Geometry, Elements, Layer, Tint};
    switch (Kind)
    {
    case EKind::Settings:
    {
        static const TArray<FVector2f> Outline = []
        {
            TArray<FVector2f> Points;
            constexpr int32 Steps = 32;
            Points.SetNumUninitialized(Steps + 1);
            for (int32 Index = 0; Index < Steps; ++Index)
            {
                const float Angle = Index * (2.0f * UE_PI / Steps);
                const float Radius = Index % 4 < 2 ? 7.0f : 5.5f;
                Points[Index] = FVector2f(8, 8) + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
            }
            // Close the outline without appending a reference into the same array.
            Points.Last() = Points[0];
            return Points;
        }();
        Pen.Line(Outline);
        Pen.Arc({8, 8}, 2.3f, 0, 360);
        break;
    }
    case EKind::Club:
        Pen.Line({{13, 1.5f}, {8, 11}, {4, 11}, {2, 12.5f}, {2, 14}, {7.5f, 14}, {9, 12.5f}});
        break;
    case EKind::Flag:
        Pen.Line({{5, 13.5f}, {5, 2}, {13, 5}, {5, 8}});
        Pen.Line({{2, 14}, {9, 14}});
        break;
    case EKind::Ball:
        Pen.Arc({8, 8}, 5.5f, 0, 360);
        Pen.Line({{6, 6}, {7, 6}});
        Pen.Line({{9, 8}, {10, 8}});
        Pen.Line({{6, 10}, {7, 10}});
        break;
    case EKind::Record:
        if (bFilled)
        {
            static const FSlateRoundedBoxBrush Dot{FLinearColor::White, 6.0f};
            Pen.Box(Dot, 2, 2, 12, 12);
        }
        else
            Pen.Arc({8, 8}, 5.5f, 0, 360);
        break;
    }
}
