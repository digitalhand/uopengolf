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
    /** Captured well above the HUD size, so the minimap can zoom in on the ball and the green and stay sharp. */
    static constexpr int32 TextureHeight = 2048;
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

    /** Both frames put map up the same way, so one can show part of the other's picture. */
    bool HasSameAxis(const FHoleMapFrame& Other) const { return Up.Equals(Other.Up, 1e-6); }

    /** The same up, and the centre and length within ToleranceCm: the same part of the world. */
    bool Equals(const FHoleMapFrame& Other, double ToleranceCm = 1.0) const
    {
        return HasSameAxis(Other) && Center.Equals(Other.Center, ToleranceCm) && FMath::IsNearlyEqual(Length, Other.Length, ToleranceCm);
    }

    /**
     * A view of part of Capture with the same up: the box around Points with Margin (cm) beyond each, grown to the
     * aspect and to MinimumViewLength (cm), then moved, and shrunk if it must, to lie inside Capture. No points is Capture.
     */
    static FHoleMapFrame Fit(const FHoleMapFrame& Capture, TArrayView<const FVector2D> Points, double Margin, double MinimumViewLength)
    {
        if (Points.IsEmpty())
            return Capture;
        // Bounds in Capture's own axes, measured from its centre: X across (along Right), Y along Up.
        FBox2D Bounds(ForceInit);
        for (const FVector2D& Point : Points)
        {
            const FVector2D Delta = Point - FVector2D(Capture.Center);
            Bounds += FVector2D(Delta | Capture.Right(), Delta | Capture.Up);
        }
        Bounds = Bounds.ExpandBy(Margin);
        const FVector2D Span = Bounds.GetSize();
        FHoleMapFrame View = Capture;
        View.Length = FMath::Min(Capture.Length, FMath::Max3(Span.Y, Span.X / Aspect, MinimumViewLength));
        const double SlackY = (Capture.Length - View.Length) * 0.5;
        const double SlackX = SlackY * Aspect;
        const FVector2D Middle = Bounds.GetCenter();
        const double Across = FMath::Clamp(Middle.X, -SlackX, SlackX);
        const double Along = FMath::Clamp(Middle.Y, -SlackY, SlackY);
        View.Center = Capture.Center + FVector(Capture.Right() * Across + Capture.Up * Along, 0);
        return View;
    }

    /** This frame's corners as UVs of Capture, which shares its up: the part of Capture's picture this frame shows. */
    FBox2D UVRegionWithin(const FHoleMapFrame& Capture) const
    {
        return FBox2D(Capture.ToUV(ToWorld(FVector2D(0, 0))), Capture.ToUV(ToWorld(FVector2D(1, 1))));
    }

    /** Part way from one frame to another of the same up; To when their ups differ. */
    static FHoleMapFrame Lerp(const FHoleMapFrame& From, const FHoleMapFrame& To, double Alpha)
    {
        if (!From.HasSameAxis(To))
            return To;
        FHoleMapFrame Frame = To;
        Frame.Center = FMath::Lerp(From.Center, To.Center, Alpha);
        Frame.Length = FMath::Lerp(From.Length, To.Length, Alpha);
        return Frame;
    }
};
