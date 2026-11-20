// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Math/Color.h"
#include "LaunchMonitorTypes.h"
#include "OpenGolfSettings.h"
#include "Styling/CoreStyle.h"

/** Colors shared by the range HUD panels, so every tile, button and banner reads as one set. */
namespace HudStyle
{
    inline FLinearColor TextColor() { return FLinearColor::FromSRGBColor(FColor(240, 242, 233)); }
    inline FSlateFontInfo Font(const FName& Weight, int32 Size)
    { return FCoreStyle::GetDefaultFontStyle(Weight, FMath::RoundToInt(Size * GolfSettings::Get().TextPercent / 100.f)); }

    /** Translucent gray tile; the raw launch-monitor tiles use the darker LaunchTileColor. */
    inline FLinearColor TileColor()
    {
        return FLinearColor::FromSRGBColor(FColor(18, 25, 23, GolfSettings::Get().HighContrast ? 255 : 235));
    }

    inline FLinearColor LaunchTileColor()
    {
        return FLinearColor::FromSRGBColor(FColor(18, 25, 23, GolfSettings::Get().HighContrast ? 255 : 245));
    }

    inline FLinearColor HoverTileColor()
    {
        return FLinearColor::FromSRGBColor(FColor(40, 54, 44, 245));
    }

    /** Marks the active mode or tool. */
    inline FLinearColor AccentColor()
    {
        return FLinearColor::FromSRGBColor(FColor(172, 200, 154));
    }

    inline FLinearColor LabelColor()
    {
        return FLinearColor::FromSRGBColor(FColor(169, 181, 174));
    }

    /** Launch-monitor connection: accent when ready, gray when disconnected, amber in between. */
    inline FLinearColor StateColor(ELaunchMonitorConnectionState State)
    {
        switch (State)
        {
        case ELaunchMonitorConnectionState::Ready: return AccentColor();
        case ELaunchMonitorConnectionState::Disconnected: return FLinearColor(0.65f, 0.65f, 0.65f);
        default: return FLinearColor(1.0f, 0.85f, 0.3f);
        }
    }

    /** Shot recording: red while recording, white when off, amber while saves are failing. */
    inline FLinearColor RecordColor(bool bRecording, int32 FailedRecordings)
    {
        if (FailedRecordings > 0) return FLinearColor(1.0f, 0.6f, 0.15f);
        return bRecording ? FLinearColor(1.0f, 0.25f, 0.2f) : FLinearColor::White;
    }

    /** The glow around the ball at address: green once the launch monitor detects the ball, red until then. */
    inline FLinearColor BallReadyColor(bool bReady)
    {
        return bReady ? FLinearColor::FromSRGBColor(FColor(96, 214, 120)) : FLinearColor(1.0f, 0.25f, 0.2f);
    }

    inline const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.6f);

    /** Hole minimap: the dark outline under every mark, so white marks read on fairway and sand alike. */
    inline FLinearColor MapInkColor() { return FLinearColor(0.012f, 0.025f, 0.022f); }

    /** Hole minimap: the pin flag. */
    inline FLinearColor MapFlagColor() { return FLinearColor(1.0f, 0.74f, 0.24f); }

    constexpr float TileRadius = 4.0f;
    constexpr float ToolbarButtonSize = 44.0f;
    /** No background until hovered. Every state is set: a default FSlateBrush would draw a white box. */
    inline FButtonStyle ToolbarButton()
    {
        return FButtonStyle()
            .SetNormal(FSlateNoResource())
            .SetHovered(FSlateRoundedBoxBrush(HoverTileColor(), TileRadius))
            .SetPressed(FSlateRoundedBoxBrush(LaunchTileColor(), TileRadius))
            .SetDisabled(FSlateNoResource());
    }

    /** Distance of corner panels from the viewport edge, in Slate units before the readable-DPI scale. */
    constexpr float Edge = 20.0f;
} // namespace HudStyle
