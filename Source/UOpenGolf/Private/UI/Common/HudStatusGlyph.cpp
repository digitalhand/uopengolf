// SPDX-License-Identifier: MIT
#include "UI/Common/HudStatusGlyph.h"

#include "UI/Common/HudPen.h"

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
    const FHudPen Pen{Geometry, Elements, Layer, Tint};
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
    case EKind::Tee:
        Pen.Arc({8, 5}, 3.5f, 0, 360);
        Pen.Line({{4, 10}, {8, 12}, {12, 10}});
        Pen.Line({{8, 12}, {8, 15}});
        break;
    case EKind::Close:
        Pen.Line({{3, 3}, {13, 13}});
        Pen.Line({{13, 3}, {3, 13}});
        break;
    case EKind::Games:
        Pen.Arc({8, 8}, 6.5f, 0, 360);
        Pen.Arc({8, 8}, 3.f, 0, 360);
        break;
    case EKind::Exit:
        Pen.Line({{7, 2}, {2, 2}, {2, 14}, {7, 14}});
        Pen.Line({{6, 8}, {14, 8}, {10, 4}});
        Pen.Line({{14, 8}, {10, 12}});
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
