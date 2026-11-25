// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

namespace OpenGolfPlay
{
    /** Holding the fast-forward key plays a rolling ball this many times faster. */
    inline constexpr float FastForwardHoldRate = 4.0f;
    /**
     * A tap skips to where the ball stops at this rate: a 30 s roll is over in half a second. The world's
     * MaxGlobalTimeDilation (Config/DefaultGame.ini) must allow it.
     */
    inline constexpr float FastForwardSkipRate = 60.0f;
    /** A press released within this (real seconds) is a tap. */
    inline constexpr double FastForwardTapSeconds = 0.25;

    /**
     * The fast-forward key as the player uses it, in real time: held, the roll plays at FastForwardHoldRate; tapped,
     * it skips to rest at FastForwardSkipRate until Reset. Only a ball rolling on the ground is sped up, and only a tap
     * pressed while it rolls skips; a key held from the flight plays the roll fast once the ball lands.
     */
    struct OPENGOLFCOURSE_API FOpenGolfFastForward
    {
        void Press(double RealSeconds, bool bRolling);
        void Release(double RealSeconds);
        /** The shot ended, a new one started, the ball was moved or a menu opened: normal speed until the next press. */
        void Reset();

        /** How much faster than normal the world should run now. */
        float Rate(bool bRolling) const;
        bool IsHeld() const { return bHeld; }
        bool IsSkipping() const { return bSkipping; }

    private:
        double PressedAt = 0.0;
        bool bHeld = false;
        bool bPressedRolling = false;
        bool bSkipping = false;
    };
} // namespace OpenGolfPlay
