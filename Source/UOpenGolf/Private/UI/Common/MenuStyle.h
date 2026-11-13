// SPDX-License-Identifier: MIT
#pragma once

#include "OpenGolfAccountStyle.h"

/**
 * The main menu's look: one rounded button family in HudStyle colors, shared text sizes and spacing.
 * The same style is passed to the OpenGolfOnline account widgets so the account page matches.
 */
namespace MenuStyle
{
    constexpr float ControlRadius = 6.0f;
    constexpr float OutlineWidth = 1.0f;
    constexpr int32 TitleSize = 32;
    constexpr int32 BodySize = 18;
    constexpr int32 NavSize = 16;
    constexpr int32 SmallSize = 13;
    constexpr int32 HelpSize = 12;
    constexpr float RowPadding = 12.0f;
    constexpr float RowGap = 8.0f;
    constexpr float ButtonGap = 8.0f;
    /** Width of the stacked buttons on the home, pause and confirmation pages. */
    constexpr float MenuButtonWidth = 320.0f;
    constexpr float SearchWidth = 300.0f;
    constexpr float NavWidth = 230.0f;
    /** The QR sign-in under the Settings categories: its square's side, 70% of the categories' width, and the gap above it. */
    constexpr float QrSize = NavWidth * 0.7f;
    constexpr float QrGap = 16.0f;
    /** Width of a settings row's control column. */
    constexpr float ControlWidth = 225.0f;
    /** Control column for a button whose label changes, such as Check for updates. */
    constexpr float ActionControlWidth = 300.0f;
    /** Control column for a folder path field, so most of the path is visible. */
    constexpr float PathControlWidth = 460.0f;

    /** Buttons, icons, sections and fonts from HudStyle. Fonts follow the Text size preference when a page is built. */
    FOpenGolfAccountStyle Make();
    /** Settings categories: no outline until hovered; the selected one uses the primary style. */
    FButtonStyle NavButton();
    /** A rounded text field like the buttons, with the accent outline while focused. */
    FEditableTextBoxStyle TextField();
    /** The Settings search box, on TextField. Built when the page is, so it follows the Text size preference. */
    FSearchBoxStyle SearchBox();
} // namespace MenuStyle
