// SPDX-License-Identifier: MIT
// Game content loaded from C++ by path. Keep in step with the assets (Docs/ContentNaming.md).
#pragma once

#include "CoreMinimal.h"

namespace UOpenGolfAssetPaths
{
    /** Unlit additive line material shared by the shot tracer, the gimme ring and the training rings' fill. */
    inline const TCHAR* const ShotTrailMaterial = TEXT("/Game/UOpenGolf/Effects/M_ShotTrail.M_ShotTrail");
    /** The training rings' outline. */
    inline const TCHAR* const TargetRingLineMaterial = TEXT("/Game/UOpenGolf/Effects/M_TargetRingLine.M_TargetRingLine");
} // namespace UOpenGolfAssetPaths
