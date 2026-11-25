// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "HoleMap/HoleMapCompositor.h"
#include "HoleMap/HoleMapReadback.h"
#include "HoleMapField.h"
#include "HoleMapFrame.h"
#include "Tasks/Task.h"

class FOpenGolfPlayDistance;

/** What the worker thread finishes: built from copied data only, so the range can let go of it at any time. */
struct FHoleMapFinished
{
    FHoleMapField PlayDistance;
    TArray<HoleMapCompositor::FPicture> Mips;
    /** How long the task waited for a worker thread (ms). */
    double WaitedMs = 0;
    double DistanceMs = 0;
    double FinishMs = 0;
};

/**
 * A hole map on its way to the minimap: captured and read back without stalling the game thread, finished on a worker
 * thread, then published with its frame and heights in one step, so the minimap never mixes two holes.
 */
struct FHoleMapBuild
{
    /** The range's InvalidateHoleMap count when this started; a later invalidation makes the result stale. */
    uint32 Generation = 0;
    double StartedAt = 0;
    FHoleMapFrame Frame;
    FHoleMapField Heights;
    /** Kept from the last map when the frame is unchanged; otherwise sampled on the worker from PlayDistanceIndex. */
    FHoleMapField PlayDistance;
    TSharedPtr<const FOpenGolfPlayDistance> PlayDistanceIndex;
    /** The terrain actor's transform: the index works in its metres. */
    FTransform TerrainTransform;
    FHoleMapReadback Readback;
    UE::Tasks::TTask<FHoleMapFinished> Finishing;
    /** When the read-back pixels reached the game thread, in ms after StartedAt. */
    double PixelsMs = 0;
    /** Longest single game-thread step so far (ms). */
    double GameThreadMs = 0;
};
