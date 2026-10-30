// SPDX-License-Identifier: MIT
#include "UI/HudIconButton.h"

#include "Rendering/DrawElements.h"

namespace
{
    /** The flag's red, as the range flag cloth (MI_Flag_Red). */
    const FLinearColor FlagColor = FLinearColor::FromSRGBColor(FColor(214, 58, 46));
} // namespace

void SHudIconButton::Construct(const FArguments& Args)
{
    Icon = Args._Icon;
    IsActive = Args._IsActive;
    OnClicked = Args._OnClicked;
}

int32 SHudIconButton::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    const FLinearColor Fill = bPressed ? HudStyle::LaunchTileColor() : IsHovered() ? HudStyle::HoverTileColor() : HudStyle::TileColor();
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(), &Disc, ESlateDrawEffect::None, Fill);
    if (IsActive.Get(false))
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), &Ring, ESlateDrawEffect::None, FLinearColor::White);
    if (Icon == EIcon::Flag)
        PaintFlag(Geometry, OutDrawElements, LayerId + 2);
    else
        PaintBall(Geometry, OutDrawElements, LayerId + 2);
    return LayerId + 3;
}

void SHudIconButton::PaintFlag(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
    // A flagstick in its cup: the cup's rim, the pole and a pennant.
    auto Box = [&](const FSlateBrush& Brush, float X, float Y, float W, float H, const FLinearColor& Color)
    {
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), &Brush,
            ESlateDrawEffect::None, Color);
    };
    Box(Pill, 13.0f, 31.0f, 18.0f, 5.0f, FLinearColor(0.92f, 0.92f, 0.9f));
    Box(Pill, 16.0f, 32.0f, 12.0f, 3.0f, FLinearColor(0.02f, 0.02f, 0.02f));
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(), TArray<FVector2f>{FVector2f(20.0f, 33.0f), FVector2f(20.0f, 9.0f)},
        ESlateDrawEffect::None, FLinearColor::White, true, 2.0f);
    FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, Geometry.ToPaintGeometry(),
        TArray<FVector2f>{FVector2f(21.0f, 9.5f), FVector2f(32.0f, 13.5f), FVector2f(21.0f, 17.5f), FVector2f(21.0f, 9.5f)}, ESlateDrawEffect::None,
        FlagColor, true, 2.5f);
    Box(Dot, 21.0f, 11.0f, 7.0f, 5.0f, FlagColor);
}

void SHudIconButton::PaintBall(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const
{
    // A ball over its shadow, with a few dimples.
    auto Box = [&](const FSlateBrush& Brush, float X, float Y, float W, float H, const FLinearColor& Color)
    {
        FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), &Brush,
            ESlateDrawEffect::None, Color);
    };
    Box(Pill, 13.0f, 31.0f, 18.0f, 5.0f, FLinearColor(0.0f, 0.0f, 0.0f, 0.35f));
    Box(BallFill, 12.0f, 10.0f, 20.0f, 20.0f, FLinearColor(0.96f, 0.96f, 0.94f));
    const FLinearColor Dimple(0.72f, 0.72f, 0.74f);
    for (const FVector2f& At : {FVector2f(17.0f, 15.0f), FVector2f(23.0f, 14.0f), FVector2f(20.0f, 20.0f), FVector2f(26.0f, 20.0f), FVector2f(16.0f, 22.0f)})
        Box(Dot, At.X, At.Y, 3.0f, 3.0f, Dimple);
}

FReply SHudIconButton::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
        return FReply::Unhandled();
    bPressed = true;
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SHudIconButton::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bPressed)
        return FReply::Unhandled();
    bPressed = false;
    if (Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
        OnClicked.ExecuteIfBound();
    return FReply::Handled().ReleaseMouseCapture();
}

FCursorReply SHudIconButton::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
    return FCursorReply::Cursor(EMouseCursor::Hand);
}
