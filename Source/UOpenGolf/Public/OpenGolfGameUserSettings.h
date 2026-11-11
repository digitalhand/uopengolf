// SPDX-License-Identifier: MIT
#pragma once
#include "GameFramework/GameUserSettings.h"
#include "OpenGolfGameUserSettings.generated.h"

/**
 * Game user settings with a render resolution for desktop-sized window modes (borderless, and
 * fullscreen where the window system cannot switch modes). The engine's desired screen size
 * derives the resolution scale from it, both when applied and when settings load.
 */
UCLASS()
class UOPENGOLF_API UOpenGolfGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()
public:
    /** Render at Size inside the desktop-sized window; the GPU scales the result to the display. */
    void SetRenderResolution(FIntPoint Size);
    /** Render at the window size again; the resolution scale is then set directly. */
    void ClearRenderResolution();
    bool UsesRenderResolution() const { return bUseDesiredScreenHeight; }
    /** The render resolution in use, otherwise the screen resolution. */
    FIntPoint GetRenderResolution() const;
};
