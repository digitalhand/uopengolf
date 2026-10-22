// SPDX-License-Identifier: MIT
#include "WindIndicator.h"

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
    FSlateDrawElement::MakeBox(Elements, Layer, Geometry.ToPaintGeometry(FVector2f(176, 44), FSlateLayoutTransform()), &PanelBrush,
        ESlateDrawEffect::None, PanelBrush.GetTint(Style) * Tint);
    FSlateDrawElement::MakeBox(Elements, Layer + 1, Geometry.ToPaintGeometry(FVector2f(48, 48), FSlateLayoutTransform(FVector2f(116, 8))),
        &CircleBrush, ESlateDrawEffect::None, Tint);

    const FString Label = State.SpeedLabel();
    const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 16);
    const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(Label, Font);
    FSlateDrawElement::MakeText(Elements, Layer + 1,
        Geometry.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f((116 - TextSize.X) * 0.5, (44 - TextSize.Y) * 0.5))),
        Label, Font, ESlateDrawEffect::None, Tint);

    const FVector2D Center(140, 32);
    const FVector2D Direction = State.Direction;
    const FVector2D Side(-Direction.Y, Direction.X);
    const FLinearColor Arrow = (State.SpeedMph > 0.01f ? FLinearColor::FromSRGBColor(FColor(34, 140, 180)) : FLinearColor(0.5f, 0.55f, 0.57f)) * Tint;
    const FVector2D Tip = Center + Direction * 12;
    FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(),
        TArray<FVector2D>{Center - Direction * 11, Tip}, ESlateDrawEffect::None, Arrow, true, 3.5f);
    FSlateDrawElement::MakeLines(Elements, Layer + 2, Geometry.ToPaintGeometry(),
        TArray<FVector2D>{Center + Direction * 2 + Side * 9, Tip, Center + Direction * 2 - Side * 9},
        ESlateDrawEffect::None, Arrow, true, 3.5f);
    return Layer + 2;
}
