// SPDX-License-Identifier: MIT
#pragma once

#include "Math/Color.h"

/** Colors shared by the range HUD panels, so every tile, button and banner reads as one set. */
namespace HudStyle
{
    /** Translucent gray tile; the raw launch-monitor tiles use the darker LaunchTileColor. */
    inline FLinearColor TileColor()
    {
        return FLinearColor::FromSRGBColor(FColor(72, 72, 76, 170));
    }

    inline FLinearColor LaunchTileColor()
    {
        return FLinearColor::FromSRGBColor(FColor(44, 44, 48, 190));
    }

    inline FLinearColor HoverTileColor()
    {
        return FLinearColor::FromSRGBColor(FColor(98, 98, 104, 200));
    }

    /** Marks the active mode or tool. */
    inline FLinearColor AccentColor()
    {
        return FLinearColor::FromSRGBColor(FColor(110, 214, 120));
    }

    inline FLinearColor LabelColor()
    {
        return FLinearColor::FromSRGBColor(FColor(200, 200, 204));
    }

    inline const FLinearColor ShadowColor(0.0f, 0.0f, 0.0f, 0.6f);

    constexpr float TileRadius = 5.0f;
    /** Distance of corner panels from the viewport edge, in Slate units before the readable-DPI scale. */
    constexpr float Edge = 20.0f;
} // namespace HudStyle
