// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/** Course-independent wind controls, shared by every course's HUD. */
namespace OpenGolfPlay
{
    /** The HUD sets the wind from calm to this, in whole mph; the console and the map can set more. */
    inline constexpr float MaxWindSettingMph = 30.0f;
    /** The speed the wind starts at when it is first switched on. */
    inline constexpr float DefaultWindSettingMph = 10.0f;
    inline constexpr float WindSpeedStepMph = 1.0f;
    inline constexpr float WindDirectionStepDeg = 5.0f;

    /**
     * The wind as the player sets it: on or off, a speed that is kept while it is off, and the direction it comes
     * from (0 headwind, 90 from the right, 180 tailwind, 270 from the left, as AUOpenGolfRange::WindFromDeg).
     */
    struct OPENGOLFCOURSE_API FOpenGolfWindSetting
    {
        bool bOn = false;
        float SpeedMph = DefaultWindSettingMph;
        float FromDeg = 0.0f;

        /** The speed the ball flies in: the set speed, or calm while off. */
        float AppliedMph() const { return bOn ? SpeedMph : 0.0f; }

        /** Whole mph from calm to MaxWindSettingMph. */
        void SetSpeedMph(float Mph);

        /** Wrapped to [0, 360) in WindDirectionStepDeg steps. */
        void SetFromDeg(float Deg);

        /** Takes wind set elsewhere (the map, Details or the console): on when it blows, keeping the last speed while calm. */
        void Adopt(float Mph, float InFromDeg);
    };

    /**
     * The wind against the target line as players say it: "Headwind", "Into · right to left", "Right to left",
     * "Helping · right to left", "Tailwind" and the mirror images, each covering 45°.
     */
    OPENGOLFCOURSE_API FString WindDirectionLabel(float FromDeg);
} // namespace OpenGolfPlay
