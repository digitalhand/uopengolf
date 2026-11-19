// SPDX-License-Identifier: MIT
// The gameplay flight table: drag and lift as curves in spin ratio W (surface speed over air speed), plus the
// measured drag rise at low Reynolds number. Fitted to published tour averages, launch-monitor shots and
// trajectory-calculator flights by Assets/Scripts/flight_fit.py (which reads this file); see Docs/Accuracy.md.
// The knots below W 0.16 were refined for driver and wood launches; the rest keep the original fit.
#pragma once

#include "BallData.h"
#include "CoreMath.h"

namespace OpenFairway::SpinRatioData
{
    constexpr int32 KnotCount = 10;
    constexpr float SpinRatio[KnotCount] = {0.0600000f, 0.0800000f, 0.1000000f, 0.1200000f, 0.1500000f, 0.1600000f, 0.2200000f, 0.3200000f, 0.5000000f, 0.8000000f};
    // Both curves rise with spin. Drag never drops below 0.17, the measured modern-ball minimum (Lyu et al. 2020).
    constexpr float Drag[KnotCount] = {0.1895485f, 0.2073769f, 0.2251196f, 0.2384511f, 0.2461000f, 0.2793000f, 0.2985000f, 0.3244000f, 0.3605000f, 0.3605000f};
    // Lift from zero at zero spin. Low-W lift is fitted to calculator trajectories, not a new measured ball curve.
    constexpr float Lift[KnotCount] = {0.1302894f, 0.1449717f, 0.1793615f, 0.1977254f, 0.2251639f, 0.2387429f, 0.2852000f, 0.3319000f, 0.3528000f, 0.4130000f};
    // Share of the measured modern-ball drag crisis (BallData.h: the drag rise below Re 100k) a spinning ball keeps.
    constexpr float CrisisScale = 0.6838f;
    constexpr float CrisisStartReynolds = 100000.0f;

    // The air flow the launch data covered; outside it the curves are held or extrapolated.
    constexpr float MinReynolds = 35000.0f;
    constexpr float MaxReynolds = 215000.0f;
    constexpr float MinSpinRatio = 0.06f;
    constexpr float MaxSpinRatio = 0.80f;

    inline float LiftAt(float W)
    {
        const float Clamped = FMath::Max(W, 0.0f);
        // Linear from no lift at zero spin to the first knot.
        return Clamped < SpinRatio[0] ? Lift[0] * Clamped / SpinRatio[0] : InterpolateHeld(SpinRatio, Lift, Clamped);
    }

    inline float DragAt(float Reynolds, float W)
    {
        const float Crisis = Reynolds < CrisisStartReynolds ? FMath::Max(BallData::DragAt(Reynolds) - BallData::DragAt(CrisisStartReynolds), 0.0f) : 0.0f;
        return InterpolateHeld(SpinRatio, Drag, FMath::Max(W, 0.0f)) + CrisisScale * Crisis;
    }

    inline bool Outside(float Reynolds, float W)
    {
        return Reynolds < MinReynolds || Reynolds > MaxReynolds || W < MinSpinRatio || W > MaxSpinRatio;
    }
} // namespace OpenFairway::SpinRatioData
