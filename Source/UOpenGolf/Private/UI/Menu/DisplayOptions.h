// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

/**
 * Resolution and display-mode choices for Settings → Video. Pure functions over injected facts, so
 * the lists are the same on every platform given the same display. Resolution always means the
 * size the game renders at: the window size when Windowed, an exclusive mode when the display
 * reports modes, and otherwise a render size that the GPU scales to the desktop-sized window.
 */
namespace DisplayOptions
{
    /** Modes as shown in Settings. The values equal EWindowMode on Windows. */
    enum EUiMode : int32 { Fullscreen = 0, Borderless = 1, Windowed = 2 };

    struct FFacts
    {
        /** True where the window system treats Borderless and Fullscreen alike (Linux: FLinuxWindow::SetWindowMode). */
        bool bBorderlessIsFullscreen = false;
        /** Exclusive fullscreen modes the RHI reports; empty on Vulkan. */
        TArray<FIntPoint> RhiModes;
        FIntPoint Desktop = FIntPoint::ZeroValue;
        /** The primary display minus panels; a window must fit inside it. */
        FIntPoint WorkArea = FIntPoint::ZeroValue;
        /** False when no render resolution can be stored: desktop-sized modes then offer the desktop only. */
        bool bRenderResolution = true;
    };
    struct FModeChoice { TArray<FString> Labels; TArray<int32> Values; };
    /** What is sent to UGameUserSettings for a shown mode and resolution. */
    struct FRequest
    {
        int32 EngineMode = Borderless;
        FIntPoint ScreenResolution = FIntPoint::ZeroValue;
        /** Set in desktop-sized modes: the window stays at desktop size and the game renders at this size. */
        TOptional<FIntPoint> RenderResolution;
    };

    FModeChoice Modes(bool bBorderlessIsFullscreen);
    /** The shown mode for a saved engine mode: a saved Borderless shows as Fullscreen where the two are alike. */
    int32 ShownMode(int32 EngineMode, bool bBorderlessIsFullscreen);
    /** The engine mode for a shown mode: Fullscreen is sent as WindowedFullscreen where the two are alike. */
    int32 EngineMode(int32 UiMode, bool bBorderlessIsFullscreen);
    /** Whether the window fills the desktop regardless of the chosen resolution. */
    bool DesktopSized(int32 UiMode, const FFacts& Facts);
    /** The sizes offered for a mode, sorted by width then height, without duplicates. */
    TArray<FIntPoint> Resolutions(int32 UiMode, const FFacts& Facts, FIntPoint Current);
    /** Current when it is offered, otherwise the largest offered size, otherwise Current. */
    FIntPoint Choose(const TArray<FIntPoint>& Offered, FIntPoint Current);
    FRequest Plan(int32 UiMode, FIntPoint Resolution, const FFacts& Facts);
}
