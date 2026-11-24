// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "HoleMapFrame.h"

/** Where a hole map field's sampler runs. */
enum class EHoleMapSampling : uint8
{
    /** One point after another on the calling thread: for samplers that touch the world, such as traces. */
    GameThread,
    /** Rows in parallel: only for samplers that read shared data and nothing else (pure math). */
    Parallel,
};

/**
 * A value over the ground of a hole map (ground height, distance from play), sampled once on a regular grid aligned to
 * the frame and read back bilinearly anywhere in it. The grid reaches one spacing beyond the frame's edges so reads
 * near them are interpolated rather than clamped. Default-constructed, it is invalid and reads zero.
 */
struct UOPENGOLF_API FHoleMapField
{
    /** Writes the value at a world XY position (cm); false where there is none (off the terrain). */
    using FSampler = TFunctionRef<bool(FVector2D World, double& Value)>;

    /**
     * Samples Frame every Spacing (cm), as Sampling says. Points the sampler misses take their sampled neighbours'
     * values, so a gap never shows as a cliff; with no samples at all the field is invalid.
     */
    static FHoleMapField Build(const FHoleMapFrame& Frame, double Spacing, FSampler Sampler, EHoleMapSampling Sampling = EHoleMapSampling::GameThread);

    bool IsValid() const { return !Values.IsEmpty(); }

    /** The value at a world XY position, bilinear between grid points; zero when invalid. */
    double At(FVector2D World) const;

    /** Change of the value per cm along world X and Y, from samples one spacing either side; zero when invalid. */
    FVector2D Gradient(FVector2D World) const;

    double GetMin() const { return Min; }
    double GetMax() const { return Max; }

private:
    double Value(int32 Column, int32 Row) const { return Values[Row * Columns + Column]; }
    /** World position of a grid point. */
    FVector2D PointAt(int32 Column, int32 Row) const;

    FHoleMapFrame Frame;
    double Spacing = 1;
    int32 Columns = 0;
    int32 Rows = 0;
    /** Row-major from the frame's left (-Right) and bottom (-Up) corner. */
    TArray<float> Values;
    double Min = 0;
    double Max = 0;
};
