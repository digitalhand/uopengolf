// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/** Asset paths for the course kit. Content lives under the plugin's /OpenGolfCourse mount. */
namespace OpenGolfCourse
{
    inline const TCHAR* FairwayMaterialPath = TEXT("/OpenGolfCourse/Surfaces/MI_Fairway.MI_Fairway");
    inline const TCHAR* RoughMaterialPath = TEXT("/OpenGolfCourse/Surfaces/MI_Rough.MI_Rough");
    inline const TCHAR* GreenMaterialPath = TEXT("/OpenGolfCourse/Surfaces/MI_Green.MI_Green");
    /** A putting green 20 feet in radius, its top 1 mm above the ground plane at its origin. */
    inline const TCHAR* GreenDiscMeshPath = TEXT("/OpenGolfCourse/Surfaces/SM_GreenDisc.SM_GreenDisc");
    inline constexpr float GreenDiscRadiusCm = 609.6f;
    inline const TCHAR* TurfMaterialPath = TEXT("/OpenGolfCourse/Surfaces/M_Turf.M_Turf");
    inline const TCHAR* GrassClumpPath = TEXT("/OpenGolfCourse/Grass/SM_GrassClump.SM_GrassClump");
    inline const TCHAR* GrassBladesMaterialPath = TEXT("/OpenGolfCourse/Grass/M_GrassBlades.M_GrassBlades");
    inline const TCHAR* FlagstickMeshPath = TEXT("/OpenGolfCourse/Flagstick/SM_Flagstick.SM_Flagstick");
    inline const TCHAR* FlagMaterialPath = TEXT("/OpenGolfCourse/Flagstick/MI_Flag_Red.MI_Flag_Red");
    inline const TCHAR* GolfBallMeshPath = TEXT("/OpenGolfCourse/Ball/SM_GolfBall.SM_GolfBall");

    /** Object path of the physical material for a surface name, such as "Rough" for PM_Rough. */
    inline FString SurfacePhysicalMaterialPath(const FString& SurfaceName)
    {
        return FString::Printf(TEXT("/OpenGolfCourse/Surfaces/PM_%s.PM_%s"), *SurfaceName, *SurfaceName);
    }
}
