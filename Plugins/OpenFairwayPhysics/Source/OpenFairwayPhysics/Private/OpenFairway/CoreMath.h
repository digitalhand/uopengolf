// SPDX-License-Identifier: MIT
// Small math shared by the OpenFairway core. Named (not anonymous) so the unity build can include every user once.
#pragma once

#include "CoreMinimal.h"

namespace OpenFairway
{
    /** Unclamped linear interpolation (the same arithmetic as FMath::Lerp). */
    inline float Lerp(float From, float To, float Weight)
    {
        return From + (To - From) * Weight;
    }

    inline float Dot(const FVector3f& A, const FVector3f& B)
    {
        return FVector3f::DotProduct(A, B);
    }

    inline FVector3f Cross(const FVector3f& A, const FVector3f& B)
    {
        return FVector3f::CrossProduct(A, B);
    }

    /** Smoothstep of an already normalized T, clamped to 0-1. */
    inline float SmoothStep01(float T)
    {
        const float Clamped = FMath::Clamp(T, 0.0f, 1.0f);
        return Clamped * Clamped * (3.0f - 2.0f * Clamped);
    }

    /** Smoothstep from Start to End; a degenerate (Start == End) range is a step at End. */
    inline float SafeSmoothStep01(float Value, float Start, float End)
    {
        const float Range = End - Start;
        if (FMath::Abs(Range) < 1e-6f)
        {
            return Value >= End ? 1.0f : 0.0f;
        }
        return SmoothStep01((Value - Start) / Range);
    }

    /** Smoothstep from Start to End; 0 for an empty or reversed range. */
    inline float SmoothRange(float Value, float Start, float End)
    {
        if (End <= Start)
        {
            return 0.0f;
        }
        const float T = FMath::Clamp((Value - Start) / (End - Start), 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    }

    /** Smooth 0 to 1 ramp; a zero-width ramp is a step at Start. */
    inline float RampUp(float Value, float Start, float End)
    {
        return End <= Start ? (Value >= Start ? 1.0f : 0.0f) : SmoothRange(Value, Start, End);
    }

    /** The unit contact normal. An invalid one is a caller error and falls back to up. */
    inline FVector3f ValidNormal(const FVector3f& Normal)
    {
        if (!FMath::IsFinite(Normal.X) || !FMath::IsFinite(Normal.Y) || !FMath::IsFinite(Normal.Z) || Normal.SizeSquared() < 0.000001f)
        {
            ensureMsgf(false, TEXT("OpenFairway: contact normal must be a finite nonzero vector."));
            return FVector3f(0.0f, 1.0f, 0.0f);
        }
        return Normal / Normal.Size();
    }

    /** The part of Vector in the plane with this unit normal (for spin: the part that rolls or skids the ball). */
    inline FVector3f AlongPlane(const FVector3f& Vector, const FVector3f& Normal)
    {
        return Vector - Dot(Vector, Normal) * Normal;
    }

    /** Piecewise-linear through (X[i], Y[i]), X ascending, holding the end values outside. */
    template <int32 N>
    float InterpolateHeld(const float (&X)[N], const float (&Y)[N], float Value)
    {
        if (Value <= X[0])
        {
            return Y[0];
        }
        for (int32 I = 1; I < N; ++I)
        {
            if (Value < X[I])
            {
                return Lerp(Y[I - 1], Y[I], (Value - X[I - 1]) / (X[I] - X[I - 1]));
            }
        }
        return Y[N - 1];
    }
} // namespace OpenFairway
