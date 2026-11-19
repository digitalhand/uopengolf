// SPDX-License-Identifier: MIT
// Approximate manual digitization, not raw experimental data. See Docs/CarryAudit.md.
// Lyu et al. 2020 (CC BY 4.0), Figs 3-5; Lyu et al. 2018 (CC BY 4.0), Fig 4a.
// Coordinates rounded to ~0.01 in coefficient and ~0.005 in spin ratio.
#pragma once

#include "CoreMinimal.h"

namespace OpenFairway::ProV1Data
{
    struct FPoint
    {
        float X, Y;
    };

    // 2020 Fig 3, blue V1 series. Re versus Cd, 2250 +/-250 rpm.
    constexpr FPoint Drag[] = {{50000, .49f}, {65000, .33f}, {75000, .25f}, {100000, .215f},
        {125000, .18f}, {150000, .17f}, {175000, .19f}, {200000, .20f}};
    // 2020 Fig 4, trend curves through the V1 measurements; Fig 5 for Re=65k.
    // Zero anchors implement symmetry; they are not measured points.
    constexpr FPoint Lift50[] = {{0, 0}, {.04f, .075f}, {.08f, .13f}, {.10f, .135f}, {.15f, .09f},
        {.20f, .035f}, {.24f, 0}, {.28f, .01f}, {.32f, .06f}, {.36f, .15f}};
    constexpr FPoint Lift60[] = {{0, 0}, {.04f, .07f}, {.08f, .025f}, {.10f, 0}, {.125f, -.06f},
        {.15f, -.085f}, {.175f, -.09f}, {.20f, -.065f}, {.23f, 0}, {.25f, .05f}, {.28f, .10f}};
    constexpr FPoint Lift65[] = {{0, 0}, {.05f, .07f}, {.08f, 0}, {.10f, -.025f}, {.12f, -.03f},
        {.15f, 0}, {.18f, .06f}, {.22f, .14f}, {.25f, .18f}, {.30f, .235f}};
    constexpr FPoint Lift70[] = {{0, 0}, {.04f, .08f}, {.08f, .045f}, {.10f, .04f}, {.12f, .05f},
        {.16f, .085f}, {.20f, .14f}, {.24f, .20f}, {.28f, .25f}, {.32f, .30f}, {.35f, .335f}};
    constexpr FPoint Lift75[] = {{0, 0}, {.04f, .08f}, {.08f, .10f}, {.12f, .13f}, {.16f, .175f},
        {.20f, .21f}, {.24f, .255f}, {.28f, .30f}, {.32f, .345f}};
    // 2018 Fig 4a, V1 horizontal-dash series; overlapping markers add uncertainty (~0.02 Cl).
    constexpr FPoint LiftHigh[] = {{0, 0}, {.045f, .10f}, {.075f, .13f}, {.095f, .15f},
        {.11f, .17f}, {.12f, .16f}, {.18f, .25f}, {.32f, .32f}};

    /** Piecewise-linear, holding the end values outside the digitized range. */
    template <int N> float At(const FPoint (&Points)[N], float X)
    {
        if (X <= Points[0].X)
        {
            return Points[0].Y;
        }
        for (int I = 1; I < N; ++I)
        {
            if (X < Points[I].X)
            {
                return FMath::Lerp(Points[I - 1].Y, Points[I].Y, (X - Points[I - 1].X) / (Points[I].X - Points[I - 1].X));
            }
        }
        return Points[N - 1].Y;
    }

    /** Signed lift: interpolates the low-Re curves, then blends to the 2018 high-speed series by 90k (an assumption). */
    inline float Lift(float Re, float S)
    {
        const float Values[] = {At(Lift50, S), At(Lift60, S), At(Lift65, S), At(Lift70, S), At(Lift75, S), At(LiftHigh, S)};
        const float ReNodes[] = {50000, 60000, 65000, 70000, 75000, 90000};
        if (Re <= ReNodes[0])
        {
            return Values[0] * FMath::Clamp(Re / ReNodes[0], 0.0f, 1.0f);
        }
        for (int I = 1; I < 6; ++I)
        {
            if (Re < ReNodes[I])
            {
                return FMath::Lerp(Values[I - 1], Values[I], (Re - ReNodes[I - 1]) / (ReNodes[I] - ReNodes[I - 1]));
            }
        }
        return Values[5];
    }

    /** Outside the drag measurement: 2020 Fig 3 covers Re 50k-200k at 2000-2500 rpm only. */
    inline bool DragOutside(float Re, float Rpm)
    {
        return Re < 50000 || Re > 200000 || Rpm < 2000 || Rpm > 2500;
    }

    /** Outside the lift measurements. Conservative: the 75k-90k transition and the zero anchors are assumptions. */
    inline bool LiftOutside(float Re, float S, float Speed)
    {
        if (Re < 50000 || (Re > 75000 && Re < 90000))
        {
            return true;
        }
        if (Re >= 90000)
        {
            // 2018: 32-91 m/s.
            return Speed < 32 || Speed > 91 || S < .045f || S > .32f;
        }
        const float MaxS = Re <= 50000 ? .36f : Re <= 65000 ? .28f : Re <= 70000 ? .30f : .32f;
        return S < .04f || S > MaxS;
    }
}
