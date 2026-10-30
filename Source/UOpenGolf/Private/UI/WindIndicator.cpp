// SPDX-License-Identifier: MIT
#include "UI/WindIndicator.h"

#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Rendering/DrawElementTypes.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

void SWindIndicator::SetWind(const WindDisplay::FState& InState)
{
    if (State.SpeedMph != InState.SpeedMph || !State.Direction.Equals(InState.Direction))
    {
        State = InState;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
}

int32 SWindIndicator::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect,
    FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const
{
    const FLinearColor Tint = Style.GetColorAndOpacityTint();
    FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(PanelWidth, PanelHeight), FSlateLayoutTransform()), &PanelBrush,
        ESlateDrawEffect::None, PanelBrush.GetTint(Style) * Tint);
    // The arrow's disc sits inside the panel at its right end, and the speed is centred in the space left of it.
    const float CircleX = PanelWidth - CircleInset - CircleSize;
    FSlateDrawElement::MakeBox(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2f(CircleSize, CircleSize), FSlateLayoutTransform(FVector2f(CircleX, CircleInset))),
        &CircleBrush, ESlateDrawEffect::None, Tint);

    const FString Label = State.SpeedLabel();
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 16);
    const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);
    FSlateDrawElement::MakeText(Elements, Layer + 1,
        Geometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f((CircleX - TextSize.X) * 0.5, (PanelHeight - TextSize.Y) * 0.5))),
        Label, Font, ESlateDrawEffect::None, Tint);

    // The arrow stays a little inside the disc's rim whichever way it points.
    const FVector2D Center(CircleX + CircleSize * 0.5f, PanelHeight * 0.5f);
    const FVector2D Direction = State.Direction;
    const FVector2D Side(-Direction.Y, Direction.X);
    const FLinearColor Arrow = (State.SpeedMph > 0.01f ? FLinearColor::FromSRGBColor(FColor(34, 140, 180)) : FLinearColor(0.5f, 0.55f, 0.57f)) * Tint;
    const FVector2D Tip = Center + Direction * 9;
    FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(),
        TArray<FVector2D>{Center - Direction * 8, Tip}, ESlateDrawEffect::None, Arrow, true, 3.0f);
    FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(),
        TArray<FVector2D>{Center + Direction * 1.5 + Side * 7, Tip, Center + Direction * 1.5 - Side * 7},
        ESlateDrawEffect::None, Arrow, true, 3.0f);
    return Layer + 2;
}
