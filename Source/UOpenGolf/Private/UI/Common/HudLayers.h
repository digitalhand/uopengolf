// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/HUD.h"
#include "UOpenGolfPlayerController.h"

/**
 * The range's viewport layers, back to front. Slate gives a click to the front-most widget that is itself hit-testable,
 * so every full-screen layer root and container passes clicks through (SelfHitTestInvisible) and only its controls
 * take them.
 */
namespace HudLayers
{
    /** Shot tiles, course header and wind indicator. The header menu takes clicks. */
    inline constexpr int32 ShotHud = 5;
    /** Club, game mode, recording and wind controls, over the shot HUD. */
    inline constexpr int32 Controls = 6;
    /** The test-shot panel, which hides the layers below while it is open. */
    inline constexpr int32 TestShotPanel = 10;

    /** The shot HUD and controls show unless the test-shot panel is open or showhud turned the HUD off. */
    inline bool IsShown(const AUOpenGolfPlayerController* Controller)
    {
        const AHUD* Hud = Controller ? Controller->GetHUD() : nullptr;
        return Controller && !Controller->IsMainMenuOpen() && !Controller->IsReplayPanelOpen() && (!Hud || Hud->bShowHUD);
    }
} // namespace HudLayers
