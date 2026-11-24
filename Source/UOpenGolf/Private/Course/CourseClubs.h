// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "LaunchMonitorTypes.h"
#include "OpenFairwayTypes.h"
#include "Play/OpenGolfLieRules.h"

/**
 * The game's clubs in a course round: which kind of club each is for lie penalties, how far a PGA Tour player carries it,
 * and the club the round picks for the next shot (Settings › Player › Auto club).
 */
namespace CourseClubs
{
    /** What the club choice reads about the next shot. */
    struct FAdvice
    {
        float DistanceToCupM = 0.0f;
        EOpenFairwaySurface Surface = EOpenFairwaySurface::Fairway;
        /** On the tee box: the only place the driver is picked, and its lie has no penalty. */
        bool bOnTee = false;
        /** The round's lie rules; null plays every lie clean. */
        const UOpenGolfLieRules* Rules = nullptr;
    };

    EOpenGolfClubGroup Group(ELaunchMonitorClub Club);

    /** TrackMan's PGA Tour average carry for Club, in metres; 0 for the putter. */
    float PgaCarryM(ELaunchMonitorClub Club);

    /**
     * The club for the next shot: the putter on the green or fringe, the sand wedge in a greenside bunker, otherwise the
     * club whose PGA Tour carry, less its lie's ball-speed cut, is nearest the distance to the cup (the longer club when
     * two are as near). The driver is picked only on the tee.
     */
    ELaunchMonitorClub Recommend(const FAdvice& Advice);
} // namespace CourseClubs
