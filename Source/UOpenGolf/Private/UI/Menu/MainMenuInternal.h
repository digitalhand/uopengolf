// SPDX-License-Identifier: MIT
// Shared by the menu's translation units: SMainMenu (MainMenu.cpp, MainMenuSettings.cpp, MainMenuCourses.cpp) and
// SCourseBrowser.
#pragma once

#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "OpenGolfAccountStyle.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/MenuStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace MainMenuDetail
{
    inline const TCHAR* const Categories[] = {TEXT("Player"), TEXT("Launch Monitor"), TEXT("Video"), TEXT("Audio"), TEXT("Camera"), TEXT("Accessibility"), TEXT("Game Defaults"), TEXT("Data & system"), TEXT("Environment")};
    constexpr int32 CategoryCount = UE_ARRAY_COUNT(Categories);
    inline UGameUserSettings* EngineSettings() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
    // SDL gives Borderless and Fullscreen the same desktop-fullscreen window (FLinuxWindow::SetWindowMode).
    constexpr bool BorderlessIsFullscreen = PLATFORM_LINUX != 0;

    /** Wrapping menu text; its font follows the Text size preference while it shows. */
    inline TSharedRef<STextBlock> MakeText(const FText& Value, int32 Size = MenuStyle::BodySize, bool bMuted = false)
    {
        return SNew(STextBlock).Text(Value).Font_Lambda([Size] { return HudStyle::Font("Regular", Size); })
            .ColorAndOpacity(bMuted ? HudStyle::LabelColor() : HudStyle::TextColor()).AutoWrapText(true);
    }

    /** A menu button with a text label; the primary one is highlighted. Style must outlive the button. */
    inline TSharedRef<SButton> MakeButton(const FOpenGolfAccountStyle& Style, const FText& Label, TFunction<void()> Action,
        bool bPrimary = false, int32 Size = MenuStyle::BodySize)
    {
        return SNew(SButton).ButtonStyle(bPrimary ? &Style.PrimaryButton : &Style.Button)
            .OnClicked_Lambda([Action = MoveTemp(Action)] { Action(); return FReply::Handled(); })
            [
                SNew(STextBlock).Text(Label).Font(HudStyle::Font("Regular", Size))
                .ColorAndOpacity(bPrimary ? Style.PrimaryLabelColor : Style.TextColor)
            ];
    }
} // namespace MainMenuDetail
