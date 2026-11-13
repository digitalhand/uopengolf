// SPDX-License-Identifier: MIT
// Shared by SMainMenu's translation units, MainMenu.cpp and MainMenuSettings.cpp.
#pragma once

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"

namespace MainMenuDetail
{
    inline const TCHAR* const Categories[] = {TEXT("Player"), TEXT("Simulator"), TEXT("Video"), TEXT("Audio"), TEXT("Camera & HUD"), TEXT("Accessibility"), TEXT("Game Defaults"), TEXT("Data & system")};
    constexpr int32 CategoryCount = UE_ARRAY_COUNT(Categories);
    inline UGameUserSettings* EngineSettings() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
    // SDL gives Borderless and Fullscreen the same desktop-fullscreen window (FLinuxWindow::SetWindowMode).
    constexpr bool BorderlessIsFullscreen = PLATFORM_LINUX != 0;
} // namespace MainMenuDetail
