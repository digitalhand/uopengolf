// SPDX-License-Identifier: MIT
#pragma once
#include "HAL/PlatformTime.h"

/** Game-thread-only measurements, enabled explicitly by the rendered automation workload. */
namespace RangePerformance
{
    struct FSample
    {
        double Seconds = 0;
        double MaxSeconds = 0;
        int32 Count = 0;
        void Add(double Elapsed) { Seconds += Elapsed; MaxSeconds = FMath::Max(MaxSeconds, Elapsed); ++Count; }
    };
    inline bool bEnabled = false;
    inline FSample Physics, Trail, Hud;
    inline void Reset() { Physics = {}; Trail = {}; Hud = {}; }
    class FScope
    {
    public:
        explicit FScope(FSample& InSample) : Sample(InSample), Start(bEnabled ? FPlatformTime::Seconds() : 0) {}
        ~FScope() { if (Start != 0) Sample.Add(FPlatformTime::Seconds() - Start); }
    private:
        FSample& Sample;
        double Start;
    };
}
