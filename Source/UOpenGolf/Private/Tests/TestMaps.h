// SPDX-License-Identifier: MIT
// The maps tests load. Change a map's path here, in Config/DefaultEngine.ini and in Config/DefaultGame.ini together.
#pragma once

#include "CoreMinimal.h"

namespace TestMaps
{
    /** The bundled startup and training map: GameDefaultMap and EditorStartupMap in Config/DefaultEngine.ini. */
    inline const TCHAR* const Range = TEXT("/Game/UOpenGolf/Maps/L_SavannaRange");

    /** Hunter Ranch's course map, from the HunterRanch course plugin (Plugins/OpenGolfCourse/Scripts/build_course.py). */
    inline const TCHAR* const HunterRanch = TEXT("/HunterRanch/Course/L_HunterRanch");
} // namespace TestMaps
