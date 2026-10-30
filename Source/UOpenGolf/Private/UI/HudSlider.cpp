// SPDX-License-Identifier: MIT
#include "UI/HudSlider.h"

#include "Rendering/DrawElements.h"

namespace
{
    constexpr float TrackHeight = 4.0f;
    /** The thumb's accent halo while hovered or dragged. */
    constexpr float HaloGrow = 6.0f;
    constexpr float DisabledOpacity = 0.45f;
} // namespace

void SHudSlider::Construct(const FArguments& Args)
{
    MinValue = Args._MinValue;
    MaxValue = Args._MaxValue;
    Step = Args._Step;
    Value = Args._Value;
    OnValueChanged = Args._OnValueChanged;
}

float SHudSlider::ValueAt(float LocalX, float WidgetWidth, float InMinValue, float InMaxValue, float InStep)
{
    const float Travel = FMath::Max(WidgetWidth - ThumbSize, 1.0f);
    const float Alpha = FMath::Clamp((LocalX - ThumbSize * 0.5f) / Travel, 0.0f, 1.0f);
    float Result = FMath::Lerp(InMinValue, InMaxValue, Alpha);
    if (InStep > 0.0f)
        Result = InMinValue + FMath::RoundToFloat((Result - InMinValue) / InStep) * InStep;
    return FMath::Clamp(Result, FMath::Min(InMinValue, InMaxValue), FMath::Max(InMinValue, InMaxValue));
}

int32 SHudSlider::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
    int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const
{
    const bool bEnabled = ShouldBeEnabled(bParentEnabled);
    const FVector2f Size = Geometry.GetLocalSize();
    const float Range = MaxValue - MinValue;
    const float Alpha = Range != 0.0f ? FMath::Clamp((Value.Get(MinValue) - MinValue) / Range, 0.0f, 1.0f) : 0.0f;
    const float ThumbX = FMath::Max(Size.X - ThumbSize, 0.0f) * Alpha;
    const float Opacity = WidgetStyle.GetColorAndOpacityTint().A * (bEnabled ? 1.0f : DisabledOpacity);
    auto Box = [&](const FSlateBrush& Brush, int32 Layer, float X, float Y, float W, float H, FLinearColor Color)
    {
        Color.A *= Opacity;
        FSlateDrawElement::MakeBox(OutDrawElements, Layer, Geometry.ToPaintGeometry(FVector2f(W, H), FSlateLayoutTransform(FVector2f(X, Y))), &Brush,
            ESlateDrawEffect::None, Color);
    };
    // The track runs between the thumb's two end positions, filled up to the thumb.
    const float TrackY = (Size.Y - TrackHeight) * 0.5f;
    const float TrackStart = ThumbSize * 0.5f;
    Box(Track, LayerId, TrackStart, TrackY, FMath::Max(Size.X - ThumbSize, 0.0f), TrackHeight, FLinearColor(1.0f, 1.0f, 1.0f, 0.22f));
    Box(Track, LayerId + 1, TrackStart, TrackY, ThumbX, TrackHeight, bEnabled ? HudStyle::AccentColor() : HudStyle::LabelColor());
    const float ThumbY = (Size.Y - ThumbSize) * 0.5f;
    if (bEnabled && (IsHovered() || HasMouseCapture()))
    {
        FLinearColor Halo = HudStyle::AccentColor();
        Halo.A = 0.35f;
        Box(Thumb, LayerId + 2, ThumbX - HaloGrow * 0.5f, ThumbY - HaloGrow * 0.5f, ThumbSize + HaloGrow, ThumbSize + HaloGrow, Halo);
    }
    Box(Thumb, LayerId + 3, ThumbX, ThumbY, ThumbSize, ThumbSize, FLinearColor::White);
    return LayerId + 4;
}

FReply SHudSlider::OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !IsEnabled())
        return FReply::Unhandled();
    SetFromPointer(Geometry, Event);
    return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SHudSlider::OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !HasMouseCapture())
        return FReply::Unhandled();
    return FReply::Handled().ReleaseMouseCapture();
}

FReply SHudSlider::OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event)
{
    if (!HasMouseCapture())
        return FReply::Unhandled();
    SetFromPointer(Geometry, Event);
    return FReply::Handled();
}

FCursorReply SHudSlider::OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const
{
    return IsEnabled() ? FCursorReply::Cursor(EMouseCursor::Hand) : FCursorReply::Unhandled();
}

void SHudSlider::SetFromPointer(const FGeometry& Geometry, const FPointerEvent& Event)
{
    const float NewValue = ValueAt(Geometry.AbsoluteToLocal(Event.GetScreenSpacePosition()).X, Geometry.GetLocalSize().X, MinValue, MaxValue, Step);
    if (NewValue != Value.Get(MinValue))
        OnValueChanged.ExecuteIfBound(NewValue);
}
