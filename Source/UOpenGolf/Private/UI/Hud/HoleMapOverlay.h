// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

class FSlateWindowElementList;
struct FGeometry;
struct FSlateBrush;

/** The marks the hole map draws over its picture that are more than a line: rings and distance chips. */
namespace HoleMapOverlay
{
    /** A chip's text: the distance in bold, then the height change from the ball (empty when level). */
    struct FChip
    {
        FString Distance;
        FString Height;
    };

    /** Points round a circle, first point repeated at the end so a polyline closes it. */
    TArray<FVector2f> Circle(FVector2f Center, float Radius);

    /** A chip's measured text and the box around it, in local units at the HUD's text size. */
    struct FChipLayout
    {
        FVector2f Size = FVector2f::ZeroVector;
        FVector2f DistanceSize = FVector2f::ZeroVector;
        FVector2f HeightSize = FVector2f::ZeroVector;
    };

    /** Measures Chip once, for placing it and then drawing it. */
    FChipLayout LayOutChip(const FChip& Chip);

    /**
     * Top-left corner for a chip of ChipSize (its layout's Size) beside Anchor: Gap to its right and centred on it when it fits inside a map
     * of MapSize, else Gap to its left; always kept inside the map.
     */
    FVector2f PlaceChip(FVector2f Anchor, float Gap, FVector2f ChipSize, FVector2f MapSize);

    /**
     * Draws Chip, laid out as Layout, with its top-left corner at TopLeft: Brush behind (on Layer) and the text above it
     * (Layer + 1).
     */
    void DrawChip(FSlateWindowElementList& Out, const FGeometry& Geometry, int32 Layer, const FSlateBrush& Brush, FVector2f TopLeft,
        const FChip& Chip, const FChipLayout& Layout);
} // namespace HoleMapOverlay
