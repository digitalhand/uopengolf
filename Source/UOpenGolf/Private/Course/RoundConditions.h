// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Play/OpenGolfRoundSetup.h"

class UGolfCourseDefinition;
class UWorld;
namespace GolfSettings { struct FPreferences; }

/**
 * How a course's round setup turns into play: its defaults, its random wind, the order of its holes, the conditions in
 * the world, and the course facts the setup page shows.
 */
namespace RoundConditions
{
    /** Scorecard yards to metres. */
    inline constexpr float MetersPerYard = 0.9144f;

    /** Holes in the course's layout; 0 for a course played on the range's own tee and cup. */
    int32 HoleCount(const UGolfCourseDefinition& Course);
    /** Tee sets on the first hole; 0 without a layout. */
    int32 TeeCount(const UGolfCourseDefinition& Course);

    /** The setup a new round of Course starts from: green speed and Gimmie from Game Defaults, the first tees, all holes, no wind. */
    OpenGolfPlay::FOpenGolfRoundSetup Defaults(const UGolfCourseDefinition& Course, const GolfSettings::FPreferences& Preferences);

    /** Gives the round a new random wind (OpenGolfPlay::FOpenGolfRoundSetup::RollWind); everything else is kept. */
    void RollWind(OpenGolfPlay::FOpenGolfRoundSetup& Setup);

    /** The name of tee set TeeIndex (by the first hole); NAME_None without a layout. */
    FName TeeSetName(const UGolfCourseDefinition& Course, int32 TeeIndex);

    /** Layout indices in play order for the setup's holes. */
    TArray<int32> HoleOrder(const OpenGolfPlay::FOpenGolfRoundSetup& Setup, const UGolfCourseDefinition& Course);

    /**
     * Bearing (OpenGolfPlay::BearingDeg) of the tee shot on layout hole LayoutIndex from tee set TeeSet: tee to its first
     * routing point, or to the cup. 0, the range's +X target line, for a course without a layout.
     */
    float TeeShotBearingDeg(const UGolfCourseDefinition& Course, int32 LayoutIndex, FName TeeSet);

    /** Par and published yards of the picked holes (numbered from 1) from tee set TeeSet. */
    int32 Par(const UGolfCourseDefinition& Course, const TArray<int32>& Holes);
    int32 Yards(const UGolfCourseDefinition& Course, const TArray<int32>& Holes, FName TeeSet);

    /** The whole course's par: every hole of its layout, or the single hole's. */
    int32 CoursePar(const UGolfCourseDefinition& Course);
    /** The whole course's length from tee set TeeSet, in metres: published yards, or the single hole's length. */
    float CourseLengthM(const UGolfCourseDefinition& Course, FName TeeSet);
    /** The course's elevation in the player's short units, such as "863 ft", or "Sea level". */
    FString ElevationText(const UGolfCourseDefinition& Course);

    /** Course rounds are played in daylight: sets the world's sky to its Day preset; false without environment controls. */
    bool ApplyDaylight(const UWorld* World);

    /** A whole wind speed in the player's units, such as "9 mph". */
    FString WindSpeedText(int32 Mph);
    /** Where the setup's world wind comes from against the tee shot of layout hole LayoutIndex, such as "Right to left". */
    FString WindDirectionText(const OpenGolfPlay::FOpenGolfRoundSetup& Setup, const UGolfCourseDefinition& Course, int32 LayoutIndex);
    /** The round's wind on layout hole LayoutIndex, such as "9 mph · Right to left", or "No wind". */
    FString WindLabel(const OpenGolfPlay::FOpenGolfRoundSetup& Setup, const UGolfCourseDefinition& Course, int32 LayoutIndex);
} // namespace RoundConditions
