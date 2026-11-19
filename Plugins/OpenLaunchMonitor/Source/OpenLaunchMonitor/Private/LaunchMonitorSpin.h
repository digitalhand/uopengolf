// SPDX-License-Identifier: MIT
// Spin components shared by the shot paths that are given only part of the spin (Open Connect, custom test shots).
#pragma once

#include "CoreMinimal.h"
#include "LaunchMonitorTypes.h"

namespace LaunchMonitorSpin
{
    inline bool Has(ELaunchMonitorValueSource Source) { return Source != ELaunchMonitorValueSource::Missing; }

    /** Fills whichever of total/axis or back/side the shot left out, marked Derived, as the Square decoder does. */
    inline void DeriveSpin(FLaunchMonitorShot& Shot)
    {
        if (Has(Shot.TotalSpinSource) && Has(Shot.SpinAxisSource))
        {
            const float AxisRadians = FMath::DegreesToRadians(Shot.SpinAxisDeg);
            if (!Has(Shot.BackSpinSource))
            {
                Shot.BackSpinRpm = FMath::RoundToInt32(Shot.TotalSpinRpm * FMath::Cos(AxisRadians));
                Shot.BackSpinSource = ELaunchMonitorValueSource::Derived;
            }
            if (!Has(Shot.SideSpinSource))
            {
                Shot.SideSpinRpm = FMath::RoundToInt32(Shot.TotalSpinRpm * FMath::Sin(AxisRadians));
                Shot.SideSpinSource = ELaunchMonitorValueSource::Derived;
            }
        }
        else if (Has(Shot.BackSpinSource) && Has(Shot.SideSpinSource))
        {
            if (!Has(Shot.TotalSpinSource))
            {
                Shot.TotalSpinRpm = FMath::RoundToInt32(FMath::Sqrt(float(FMath::Square(Shot.BackSpinRpm) + FMath::Square(Shot.SideSpinRpm))));
                Shot.TotalSpinSource = ELaunchMonitorValueSource::Derived;
            }
            if (!Has(Shot.SpinAxisSource))
            {
                Shot.SpinAxisDeg = FMath::RadiansToDegrees(FMath::Atan2(float(Shot.SideSpinRpm), float(Shot.BackSpinRpm)));
                Shot.SpinAxisSource = ELaunchMonitorValueSource::Derived;
            }
        }
    }
} // namespace LaunchMonitorSpin
