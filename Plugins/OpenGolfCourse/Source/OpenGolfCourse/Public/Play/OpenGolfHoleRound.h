// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * One hole's strokes, after ogolf game/scoring/HoleRound.cs: every shot that comes to rest is a stroke, a shot hit from
 * the green is also a putt, and the hole ends when the ball drops. A round in which the ball was placed by hand is
 * practice and has no score name.
 */
struct OPENGOLFCOURSE_API FOpenGolfHoleRound
{
    int32 Par = 4;
    int32 Strokes = 0;
    int32 Putts = 0;
    bool bHoled = false;
    bool bPractice = false;

    void Start(int32 InPar);
    void RecordStroke(bool bFromGreen, bool bInCup);

    /** "Par", "Birdie", "+3" and so on for a finished hole; "Holed in 3" for practice; empty before the hole ends. */
    FString ResultName() const;

    /** The name for Strokes against Par: "Hole in one", "Albatross", "Eagle", "Birdie", "Par", "Bogey", ... */
    static FString ScoreName(int32 Strokes, int32 Par);
};
