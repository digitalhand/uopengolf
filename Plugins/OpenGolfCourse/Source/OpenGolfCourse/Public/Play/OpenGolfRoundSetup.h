// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

struct FRandomStream;

/**
 * A course round's setup: the choices made before play (tees, green speed, Gimmie, holes), which every course offers the
 * same way, and the round's wind, which is rolled at random rather than chosen.
 */
namespace OpenGolfPlay
{
    /** The hole choices the setup offers; Custom is any other set of ticked holes. */
    enum class EOpenGolfHoleSet : uint8
    {
        All,
        Front,
        Back,
        Custom
    };

    /** A round's random wind runs from calm to this, in whole mph. */
    inline constexpr int32 MaxRoundWindMph = 13;
    inline constexpr int32 MinRoundStimpFeet = 8;
    inline constexpr int32 MaxRoundStimpFeet = 15;
    inline constexpr int32 ShortGimmieFeet = 3;
    inline constexpr int32 LongGimmieFeet = 6;
    /** Holes in a nine: the front nine is 1-9 and the back nine the rest. */
    inline constexpr int32 HolesPerNine = 9;

    struct OPENGOLFCOURSE_API FOpenGolfRoundSetup
    {
        /** The course's tee set, by its index on the first hole. */
        int32 TeeIndex = 0;
        int32 GreenStimpFeet = 10;
        /** Whole mph, 0 (no wind) to MaxRoundWindMph. */
        int32 WindSpeedMph = 0;
        /**
         * Where the wind comes from in the world, in whole degrees clockwise from +X, as AOpenFairwayBall::WindFromDeg.
         * It stays fixed over the course, so it plays differently on each hole.
         */
        float WindFromDeg = 0.0f;
        bool bGimmie = false;
        /** ShortGimmieFeet or LongGimmieFeet; kept while the Gimmie is off. */
        int32 GimmieFeet = ShortGimmieFeet;
        /** The holes to play, numbered from 1, ascending; empty on a course without a hole layout. */
        TArray<int32> Holes;

        /** The wind speed the round plays. */
        float WindMph() const { return static_cast<float>(WindSpeedMph); }
        /** The Gimmie radius the round concedes within; 0 while it is off. */
        int32 AppliedGimmieFeet() const { return bGimmie ? GimmieFeet : 0; }

        /** A random wind for the round: a whole speed from 0 to MaxRoundWindMph and a whole direction from 0 to 359°. */
        void RollWind(FRandomStream& Stream);

        /**
         * Moves every value to one the setup offers. HoleCount is the course's holes (0 for a course without a layout);
         * holes it lacks are dropped, and an empty list plays every hole.
         */
        void Validate(int32 HoleCount, int32 TeeCount);

        /** URL options carrying the setup through level travel, such as "?TeeIndex=1?Stimp=11?WindMph=3?WindFrom=217?...". */
        FString ToOptions() const;

        /** The setup in a travel URL; anything missing or unreadable keeps the value in Defaults. Validated. */
        static FOpenGolfRoundSetup FromOptions(const FString& Options, const FOpenGolfRoundSetup& Defaults, int32 HoleCount, int32 TeeCount);

        bool operator==(const FOpenGolfRoundSetup& Other) const;
        bool operator!=(const FOpenGolfRoundSetup& Other) const { return !(*this == Other); }
    };

    /** True when a course has a back nine to offer: more than one nine. */
    OPENGOLFCOURSE_API bool HasNines(int32 HoleCount);
    /** The holes of a set on a course of HoleCount holes; Custom (and nines on a course without them) is every hole. */
    OPENGOLFCOURSE_API TArray<int32> HolesFor(EOpenGolfHoleSet Set, int32 HoleCount);
    /** Which set a list of ascending holes is. */
    OPENGOLFCOURSE_API EOpenGolfHoleSet DetectHoleSet(const TArray<int32>& Holes, int32 HoleCount);
    /** Ascending holes as runs, such as "1-9,12". */
    OPENGOLFCOURSE_API FString FormatHoles(const TArray<int32>& Holes);
    /** A FormatHoles list read back: sorted, without repeats or holes outside 1..HoleCount. */
    OPENGOLFCOURSE_API TArray<int32> ParseHoles(const FString& Text, int32 HoleCount);

    /** "11 ft · Medium": 8-9 ft Slow, 10-11 Medium, 12-13 Fast, 14-15 Tour. */
    OPENGOLFCOURSE_API FString GreenSpeedLabel(int32 StimpFeet);

    /**
     * A world wind direction (clockwise from +X) read against a tee shot whose bearing is TeeShotBearingDeg
     * (OpenGolfPlay::BearingDeg): 0 is a headwind, 90 from the right, 180 a tailwind, 270 from the left.
     */
    OPENGOLFCOURSE_API float RoundWindRelativeFromDeg(float WorldFromDeg, float TeeShotBearingDeg);
} // namespace OpenGolfPlay
