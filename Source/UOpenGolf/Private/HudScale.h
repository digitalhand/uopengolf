// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "Engine/World.h"

namespace HudScale
{
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
