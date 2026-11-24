// SPDX-License-Identifier: MIT
// The course the game ships with, for tests that play a round without level travel.
#pragma once

#include "GolfCourseDefinition.h"

namespace BundledCourse
{
    /** Created by uopengolf.CreateCourseAssets. */
    inline const TCHAR* const Path = TEXT("/Game/UOpenGolf/Courses/DA_SavannaRange.DA_SavannaRange");

    /** Loads the definition (its map is never loaded); null when the asset is missing. */
    inline const UGolfCourseDefinition* Load() { return LoadObject<UGolfCourseDefinition>(nullptr, Path); }
} // namespace BundledCourse
