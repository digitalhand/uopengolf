// SPDX-License-Identifier: MIT
#include "UI/HudSwitch.h"

#include "Rendering/DrawElements.h"

namespace
{
    constexpr float KnobInset = 2.0f;
} // namespace

void SHudSwitch::Construct(const FArguments& Args)
{
    IsOn = Args._IsOn;
    OnToggled = Args._OnToggled;
}

int32 SHudSwitch::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    const bool bOn = IsOn.Get(false);
    const float Opacity = WidgetStyle.GetColorAndOpacityTint().A;
    FLinearColor Fill = bOn ? HudStyle::AccentColor() : (IsHovered() ? HudStyle::HoverTileColor() : HudStyle::LaunchTileColor());
    Fill.A *= Opacity;
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId, Geometry.ToPaintGeometry(FVector2f(Width, Height), FSlateLayoutTransform()), &Pill,
        ESlateDrawEffect::None, Fill);
    const float KnobSize = Height - KnobInset * 2.0f;
    const float KnobX = bOn ? Width - KnobInset - KnobSize : KnobInset;
    FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
        Geometry.ToPaintGeometry(FVector2f(KnobSize, KnobSize), FSlateLayoutTransform(FVector2f(KnobX, KnobInset))), &Knob, ESlateDrawEffect::None,
        FLinearColor(1.0f, 1.0f, 1.0f, Opacity));
    return LayerId + 2;
}

FReply SHudSwitch::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton)
        return FReply::Unhandled();
    bPressed = true;
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SHudSwitch::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !bPressed)
        return FReply::Unhandled();
    bPressed = false;
    if (Geometry.IsUnderLocation(Event.GetScreenSpacePosition()))
        OnToggled.ExecuteIfBound();
    return FReply::Handled().ReleaseMouseCapture();
}

FCursorReply SHudSwitch::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
    return FCursorReply::Cursor(EMouseCursor::Hand);
}
