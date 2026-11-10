// SPDX-License-Identifier: MIT
#pragma once

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

    inline const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.6f);

    constexpr float TileRadius = 4.0f;
    /** Distance of corner panels from the viewport edge, in Slate units before the readable-DPI scale. */
    constexpr float Edge = 20.0f;
} // namespace HudStyle
