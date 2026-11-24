// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

class SWidget;

/** Pointers drawn by the HUD in place of the system's. */
namespace HudPointer
{
    /** Widget tag of the ball pointer, so tests can find it. */
    inline const TCHAR* const BallTag = TEXT("BallPointer");

    /** The HUD's ball icon at pointer size, with a shadow so it reads on grass; the ball's centre is the hot spot. */
    TSharedRef<SWidget> MakeBall();
} // namespace HudPointer
