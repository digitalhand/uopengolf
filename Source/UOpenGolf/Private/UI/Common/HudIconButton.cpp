// SPDX-License-Identifier: MIT
#include "UI/Common/HudIconButton.h"

#include "Rendering/DrawElements.h"
#include "UI/Common/HudStatusGlyph.h"

void SHudIconButton::Construct(const FArguments& Args)
{
    Icon = Args._Icon;
    IsActive = Args._IsActive;
    OnClicked = Args._OnClicked;
}

int32 SHudIconButton::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    const auto PaintBrush = [&](const FSlateBrush& Brush, int32 Layer)
    {
        FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(), &Brush, ESlateDrawEffect::None,
            Brush.GetTint(WidgetStyle) * WidgetStyle.GetColorAndOpacityTint());
    };
    const bool bFocused = HasKeyboardFocus();
    if (bPressed || IsHovered() || bFocused)
        PaintBrush(bPressed ? Style.Pressed : Style.Hovered, LayerId);
    if (IsActive.Get(false) || bFocused)
        PaintBrush(Outline, LayerId + 1);

    const FVector2f GlyphSize(SHudStatusGlyph::Size, SHudStatusGlyph::Size);
    const FVector2f Offset = (FVector2f(Geometry.GetLocalSize()) - GlyphSize) * 0.5f;
    const FGeometry Glyph = Geometry.MakeChild(GlyphSize, FSlateLayoutTransform(Offset));
    // Tinted and dimmed as SHudStatusGlyph is, so a faded or disabled button fades its icon too.
    FLinearColor Tint = WidgetStyle.GetColorAndOpacityTint();
    Tint.A *= bParentEnabled && IsEnabled() ? 1.0f : SHudStatusGlyph::DisabledAlpha;
    SHudStatusGlyph::Paint(Icon == EIcon::Flag ? SHudStatusGlyph::EKind::Flag : SHudStatusGlyph::EKind::Ball,
        Glyph, OutDrawElements, LayerId + 2, Tint);
    return LayerId + 3;
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

FReply SHudIconButton::OnKeyDown(const FGeometry&, const FKeyEvent& Event)
{
    if (!IsEnabled()) return FReply::Unhandled();
    const FKey Key = Event.GetKey();
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar || Key == EKeys::Gamepad_FaceButton_Bottom)
    {
        if (!Event.IsRepeat()) OnClicked.ExecuteIfBound();
        return FReply::Handled();
    }
    return FReply::Unhandled();
}
