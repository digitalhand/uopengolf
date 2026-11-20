// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * The orthographic portrait frame of the hole map, shared by the range's top-down capture and the minimap's drawing and
 * hit testing, so a click lands where the picture shows it. Map up is the tee-to-cup direction. UV (0,0) is the
 * picture's top-left corner, U grows to the right and V grows down; Length is the world height the picture covers (cm).
 */
struct FHoleMapFrame
{
    /** Width over height of the picture, the render target and the HUD widget. */
    static constexpr double Aspect = 0.625;
    static constexpr int32 TextureHeight = 768;
    static constexpr int32 TextureWidth = int32(TextureHeight * Aspect);
    /** The smallest height (cm) a frame covers, for a tee on top of its cup. */
    static constexpr double MinimumLength = 100.0;

    FVector Center = FVector::ZeroVector;
    FVector2D Up = FVector2D(1, 0);
    double Length = 1;

    /**
     * Frames the tee and cup with EndMargin (cm) beyond each, at least MinimumWidth (cm) across, shifted SideOffset (cm)
     * to the right of the tee-to-cup line.
     */
    static FHoleMapFrame FromHole(FVector Tee, FVector Cup, double EndMargin, double MinimumWidth, double SideOffset)
    {
        const FVector2D Axis(Cup - Tee);
        FHoleMapFrame Frame;
        if (!Axis.IsNearlyZero())
            Frame.Up = Axis.GetSafeNormal();
        Frame.Center = (Tee + Cup) * 0.5 + FVector(Frame.Right() * SideOffset, 0);
        Frame.Length = FMath::Max3(MinimumLength, Axis.Size() + 2 * EndMargin, MinimumWidth / Aspect);
        return Frame;
    }

    /** World direction of the picture's +U (to the right of the tee-to-cup line). */
    FVector2D Right() const { return FVector2D(-Up.Y, Up.X); }

    /** A world XY position as picture UV; outside [0,1] when off the map. */
    FVector2D ToUV(FVector2D World) const
    {
        const FVector2D Delta = World - FVector2D(Center);
        return FVector2D(0.5 + (Delta | Right()) / (Length * Aspect), 0.5 - (Delta | Up) / Length);
    }

    /** A picture UV as a world XY position; the inverse of ToUV. */
    FVector2D ToWorld(FVector2D UV) const
    {
        return FVector2D(Center) + Right() * ((UV.X - 0.5) * Length * Aspect) + Up * ((0.5 - UV.Y) * Length);
    }
};
