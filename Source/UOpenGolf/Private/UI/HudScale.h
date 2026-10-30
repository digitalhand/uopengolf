// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"

namespace HudScale
{
    /** Gap under the bottom-corner panels (Test shots button, status box), leaving room for the version label. */
    constexpr float BottomPanelGap = 24.0f;

    /** DPI scale for viewport panels: undoes viewport shrinking so they stay readable in small editor viewports. */
    inline float Readable(const UWorld* World)
    {
        FVector2D Size;
        if (World && World->GetGameViewport())
        {
            World->GetGameViewport()->GetViewportSize(Size);
            const float Scale = GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(FIntPoint(Size.X, Size.Y));
            return 1.0f / FMath::Clamp(Scale, 0.1f, 1.0f);
        }
        return 1.0f;
    }
} // namespace HudScale
