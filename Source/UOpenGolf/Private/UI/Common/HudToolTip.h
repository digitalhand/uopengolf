// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"

class SToolTip;

/** Hover tooltips for the HUD's icon buttons, in the HUD tile colors rather than the engine's default style. */
namespace HudToolTip
{
    /** Widget tag of every HUD tooltip, so tests can tell it from an engine-styled one. */
    inline const TCHAR* const Tag = TEXT("HudToolTip");

    /** A dark tile with light text and an accent outline, wrapped to a narrow column; Text is read while it shows. */
    TSharedRef<SToolTip> Make(const TAttribute<FText>& Text);
} // namespace HudToolTip
