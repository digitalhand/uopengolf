// SPDX-License-Identifier: MIT
#include "Play/OpenGolfFastForward.h"

namespace OpenGolfPlay
{
    void FOpenGolfFastForward::Press(double RealSeconds, bool bRolling)
    {
        if (bHeld)
            return;
        bHeld = true;
        PressedAt = RealSeconds;
        bPressedRolling = bRolling;
    }

    void FOpenGolfFastForward::Release(double RealSeconds)
    {
        if (!bHeld)
            return;
        bHeld = false;
        if (bPressedRolling && RealSeconds - PressedAt <= FastForwardTapSeconds)
            bSkipping = true;
    }

    void FOpenGolfFastForward::Reset()
    {
        bHeld = false;
        bPressedRolling = false;
        bSkipping = false;
    }

    float FOpenGolfFastForward::Rate(bool bRolling) const
    {
        if (!bRolling)
            return 1.0f;
        return bSkipping ? FastForwardSkipRate : bHeld ? FastForwardHoldRate : 1.0f;
    }
} // namespace OpenGolfPlay
