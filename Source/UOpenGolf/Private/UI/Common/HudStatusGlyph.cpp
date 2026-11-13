// SPDX-License-Identifier: MIT
#include "UI/Common/HudStatusGlyph.h"

#include "Rendering/DrawElements.h"

namespace HudStatusGlyphDetail
{
    constexpr float Height = 16.0f;
    constexpr float LaunchMonitorWidth = 22.0f;
    constexpr float BatteryWidth = 22.0f;
    constexpr float RecordWidth = 16.0f;
    constexpr float Stroke = 1.5f;
    constexpr float DisabledAlpha = 0.4f;

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
        void Rect(float X, float Y, float W, float H) const
        {
            Line({{X, Y}, {X + W, Y}, {X + W, Y + H}, {X, Y + H}, {X, Y}});
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
    Level = Args._Level;
    Filled = Args._Filled;
}

FVector2D SHudStatusGlyph::ComputeDesiredSize(float) const
{
    using namespace HudStatusGlyphDetail;
    const float Width = Kind == EKind::LaunchMonitor ? LaunchMonitorWidth : Kind == EKind::Battery ? BatteryWidth : RecordWidth;
    return FVector2D(Width, Height);
}

int32 SHudStatusGlyph::OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    FLinearColor Tint = Color.Get(FLinearColor::White) * WidgetStyle.GetColorAndOpacityTint();
    Tint.A *= bParentEnabled ? 1.0f : HudStatusGlyphDetail::DisabledAlpha;
    switch (Kind)
    {
    case EKind::LaunchMonitor: PaintLaunchMonitor(Geometry, OutDrawElements, LayerId, Tint); break;
    case EKind::Battery: PaintBattery(Geometry, OutDrawElements, LayerId, Tint); break;
    default: PaintRecord(Geometry, OutDrawElements, LayerId, Tint); break;
    }
    return LayerId + 1;
}

void SHudStatusGlyph::PaintLaunchMonitor(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint) const
{
    // A launch monitor from the front: its case and camera lens, with a signal arc beside it.
    const HudStatusGlyphDetail::FPen Pen{Geometry, OutDrawElements, LayerId, Tint};
    Pen.Rect(1, 5, 14, 9);
    Pen.Arc(FVector2f(6.5f, 9.5f), 2.5f, 0, 360);
    Pen.Arc(FVector2f(15, 5), 5, -80, -10);
    Pen.Arc(FVector2f(15, 5), 2.5f, -80, -10);
}

void SHudStatusGlyph::PaintBattery(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint) const
{
    // The case, its terminal and a charge bar as long as the level; no bar when the level is unknown.
    const HudStatusGlyphDetail::FPen Pen{Geometry, OutDrawElements, LayerId, Tint};
    Pen.Rect(1, 3.5f, 17, 9);
    Pen.Box(Fill, 18.75f, 6, 2.5f, 4);
    const float Charge = Level.Get(-1.0f);
    if (Charge > 0.0f)
        Pen.Box(Fill, 3, 5.5f, 13 * FMath::Min(Charge, 1.0f), 5);
}

void SHudStatusGlyph::PaintRecord(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FLinearColor& Tint) const
{
    // Filled while recording, a ring otherwise.
    const HudStatusGlyphDetail::FPen Pen{Geometry, OutDrawElements, LayerId, Tint};
    if (Filled.Get(false))
        Pen.Box(Dot, 2, 2, 12, 12);
    else
        Pen.Arc(FVector2f(8, 8), 5.5f, 0, 360);
}
