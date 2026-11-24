// SPDX-License-Identifier: MIT
#include "UI/Hud/HoleMapOverlay.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/WidgetStyle.h"
#include "UI/Common/HudStyle.h"

namespace HoleMapOverlayDetail
{
    constexpr float PaddingX = 6.0f;
    constexpr float PaddingY = 2.0f;
    /** Between the distance and the height change. */
    constexpr float Spacing = 5.0f;
    /** Chips keep this far inside the map's edge. */
    constexpr float EdgeInset = 2.0f;
    constexpr int32 CircleSegments = 24;

    FSlateFontInfo DistanceFont() { return HudStyle::Font("Bold", 10); }
    FSlateFontInfo HeightFont() { return HudStyle::Font("Regular", 9); }

    FVector2f Measure(const FString& Text, const FSlateFontInfo& Font)
    {
        if (Text.IsEmpty() || !FSlateApplication::IsInitialized())
            return FVector2f::ZeroVector;
        return FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Text, Font));
    }
} // namespace HoleMapOverlayDetail

TArray<FVector2f> HoleMapOverlay::Circle(FVector2f Center, float Radius)
{
    TArray<FVector2f> Points;
    Points.Reserve(HoleMapOverlayDetail::CircleSegments + 1);
    for (int32 Step = 0; Step <= HoleMapOverlayDetail::CircleSegments; ++Step)
    {
        const float Angle = 2.0f * UE_PI * Step / HoleMapOverlayDetail::CircleSegments;
        Points.Add(Center + FVector2f(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius);
    }
    return Points;
}

HoleMapOverlay::FChipLayout HoleMapOverlay::LayOutChip(const FChip& Chip)
{
    using namespace HoleMapOverlayDetail;
    FChipLayout Layout;
    Layout.DistanceSize = Measure(Chip.Distance, DistanceFont());
    Layout.HeightSize = Measure(Chip.Height, HeightFont());
    const float TextWidth = Layout.DistanceSize.X + (Chip.Height.IsEmpty() ? 0.0f : Spacing + Layout.HeightSize.X);
    Layout.Size = FVector2f(TextWidth + 2 * PaddingX, FMath::Max(Layout.DistanceSize.Y, Layout.HeightSize.Y) + 2 * PaddingY);
    return Layout;
}

FVector2f HoleMapOverlay::PlaceChip(FVector2f Anchor, float Gap, FVector2f ChipSize, FVector2f MapSize)
{
    using HoleMapOverlayDetail::EdgeInset;
    float X = Anchor.X + Gap;
    if (X + ChipSize.X > MapSize.X - EdgeInset)
        X = Anchor.X - Gap - ChipSize.X;
    const float MaxX = FMath::Max(EdgeInset, MapSize.X - ChipSize.X - EdgeInset);
    const float MaxY = FMath::Max(EdgeInset, MapSize.Y - ChipSize.Y - EdgeInset);
    return FVector2f(FMath::Clamp(X, EdgeInset, MaxX), FMath::Clamp(Anchor.Y - ChipSize.Y * 0.5f, EdgeInset, MaxY));
}

void HoleMapOverlay::DrawChip(FSlateWindowElementList& Out, const FGeometry& Geometry, int32 Layer, const FSlateBrush& Brush, FVector2f TopLeft,
    const FChip& Chip, const FChipLayout& Layout)
{
    using namespace HoleMapOverlayDetail;
    // A rounded box's fill is its tint, which MakeBox does not apply by itself (SBorder passes it the same way).
    FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(Layout.Size, FSlateLayoutTransform(TopLeft)), &Brush, ESlateDrawEffect::None,
        Brush.GetTint(FWidgetStyle()));
    // Each text is centred in the chip's height.
    const auto TextAt = [&](float X, const FVector2f& TextSize)
    {
        return Geometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TopLeft + FVector2f(PaddingX + X, (Layout.Size.Y - TextSize.Y) * 0.5f)));
    };
    FSlateDrawElement::MakeText(Out, Layer + 1, TextAt(0, Layout.DistanceSize), Chip.Distance, DistanceFont(), ESlateDrawEffect::None,
        HudStyle::TextColor());
    if (!Chip.Height.IsEmpty())
        FSlateDrawElement::MakeText(Out, Layer + 1, TextAt(Layout.DistanceSize.X + Spacing, Layout.HeightSize), Chip.Height, HeightFont(),
            ESlateDrawEffect::None, HudStyle::LabelColor());
}
