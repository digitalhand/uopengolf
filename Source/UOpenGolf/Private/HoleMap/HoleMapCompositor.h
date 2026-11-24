// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "HoleMapField.h"
#include "HoleMapFrame.h"

class UTexture2D;

/**
 * Finishes the hole map's top-down capture on the CPU, once per capture: hillshade and contours from the ground's
 * height, a dimmed surround away from play, and a mip chain so the minimap can zoom without aliasing. Every step
 * returns a new picture; pictures are sRGB, row 0 at the top (the frame's far end), and come out opaque.
 */
namespace HoleMapCompositor
{
    struct FPicture
    {
        int32 Width = 0;
        int32 Height = 0;
        TArray<FColor> Pixels;
    };

    /** Lit where the ground faces the map's top left, shaded where it faces away; flat ground is unchanged. */
    FPicture WithRelief(const FPicture& Image, const FHoleMapFrame& Frame, const FHoleMapField& Heights);

    /** The contour interval (cm) for heights spanning SpanCm. */
    double ContourInterval(double SpanCm);

    /** Darkened lines where the ground crosses each contour height. */
    FPicture WithContours(const FPicture& Image, const FHoleMapFrame& Frame, const FHoleMapField& Heights);

    /** Desaturated and darkened with distance (cm) from play; unchanged without a distance field. */
    FPicture WithSurroundDimmed(const FPicture& Image, const FHoleMapFrame& Frame, const FHoleMapField& DistanceFromPlay);

    /** Image, then each half-size level averaged in linear light, down to one pixel. */
    TArray<FPicture> BuildMips(const FPicture& Image);

    /** A transient sRGB texture holding every level of Mips, filtered between them. Null for an empty chain. */
    UTexture2D* MakeTexture(const TArray<FPicture>& Mips, FName Name);
} // namespace HoleMapCompositor
