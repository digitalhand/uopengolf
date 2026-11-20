// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SLeafWidget.h"

/** A toolbar button with a drawn icon: the hole flag (play mode) or a ball (place the ball). */
class SHudIconButton : public SLeafWidget
{
public:
    enum class EIcon : uint8
    {
        Flag,
        Ball
    };

    static constexpr float Size = HudStyle::ToolbarButtonSize;

    SLATE_BEGIN_ARGS(SHudIconButton) : _Icon(EIcon::Flag) {}
    SLATE_ARGUMENT(EIcon, Icon)
    /** Draws the accent outline: the mode or tool is on. */
    SLATE_ATTRIBUTE(bool, IsActive)
    SLATE_EVENT(FSimpleDelegate, OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent&) override;

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& CullingRect, FSlateWindowElementList& OutDrawElements,
        int32 LayerId, const FWidgetStyle& WidgetStyle, bool bParentEnabled) const override;
    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(Size, Size); }
    virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FCursorReply OnCursorQuery(const FGeometry& Geometry, const FPointerEvent& Event) const override;

private:
    EIcon Icon = EIcon::Flag;
    TAttribute<bool> IsActive;
    FSimpleDelegate OnClicked;
    bool bPressed = false;

    const FButtonStyle Style = HudStyle::ToolbarButton();
    FSlateRoundedBoxBrush Outline{FLinearColor::Transparent, HudStyle::TileRadius, HudStyle::AccentColor(), 1.0f};
};
