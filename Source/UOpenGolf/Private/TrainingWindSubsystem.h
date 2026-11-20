// SPDX-License-Identifier: MIT
#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "Play/OpenGolfWindSetting.h"
#include "TrainingWindSubsystem.generated.h"

/**
 * Training's wind, kept across level travel (a course round, the main menu) for the rest of the game session, so
 * returning to Training finds the wind the player left. Changing the Game Defaults starting wind forgets it, since that
 * preference then decides a new session's wind.
 */
UCLASS()
class UTrainingWindSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** Keeps Training's wind, with the starting-wind preference (GolfSettings WindMph) it was set under. */
    void Save(const OpenGolfPlay::FOpenGolfWindSetting& InWind, int32 SpeedPreference)
    {
        Wind = InWind;
        SavedPreference = SpeedPreference;
    }

    /** The kept wind; unset when none was kept or the preference has changed since (it is then forgotten). */
    TOptional<OpenGolfPlay::FOpenGolfWindSetting> Restore(int32 SpeedPreference)
    {
        if (SavedPreference != SpeedPreference)
            Wind.Reset();
        return Wind;
    }

private:
    TOptional<OpenGolfPlay::FOpenGolfWindSetting> Wind;
    int32 SavedPreference = -1;
};
