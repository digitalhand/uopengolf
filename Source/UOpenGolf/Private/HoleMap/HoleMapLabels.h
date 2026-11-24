// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "OpenFairwayTypes.h"
#include "OpenGolfSettings.h"

/** Text for the hole map: height changes on its target chip and in its footer, and the lie in its header. */
namespace HoleMapLabels
{
    /** Heights read in metres when putts do: both are the short distances on and around the green. */
    inline bool MetricHeights() { return GolfSettings::Get().PuttingUnits != 0; }

    /**
     * A height change of DeltaM metres from the ball: "▲ 5' 1\"" up, "▼ 2'" down, "▲ 3\"" under a foot, in feet and inches
     * as golfers read elevation, or in metres to 0.1 m. Empty when it rounds to nothing (under half an inch).
     */
    inline FString HeightChange(double DeltaM, bool bMetric)
    {
        const TCHAR* Arrow = DeltaM > 0 ? TEXT("▲") : TEXT("▼");
        if (bMetric)
        {
            const double Metres = FMath::RoundToDouble(FMath::Abs(DeltaM) * 10) / 10;
            return Metres > 0 ? FString::Printf(TEXT("%s %.1f m"), Arrow, Metres) : FString();
        }
        const int32 Inches = FMath::RoundToInt32(FMath::Abs(DeltaM) * 39.3701);
        const int32 Feet = Inches / 12, Rest = Inches % 12;
        if (Inches == 0)
            return FString();
        if (Feet == 0)
            return FString::Printf(TEXT("%s %d\""), Arrow, Rest);
        return Rest == 0 ? FString::Printf(TEXT("%s %d'"), Arrow, Feet) : FString::Printf(TEXT("%s %d' %d\""), Arrow, Feet, Rest);
    }

    /** The footer's elevation to the pin: HeightChange, or "Level" when that rounds to nothing. */
    inline FString ElevationText(double DeltaM, bool bMetric)
    {
        const FString Change = HeightChange(DeltaM, bMetric);
        return Change.IsEmpty() ? FString(TEXT("Level")) : Change;
    }

    /** The lie as a golfer names it, for the map's header. */
    inline const TCHAR* SurfaceName(EOpenFairwaySurface Surface)
    {
        switch (Surface)
        {
        case EOpenFairwaySurface::FairwaySoft: return TEXT("Soft fairway");
        case EOpenFairwaySurface::Rough: return TEXT("Rough");
        case EOpenFairwaySurface::Firm: return TEXT("Firm ground");
        case EOpenFairwaySurface::Green: return TEXT("Green");
        case EOpenFairwaySurface::Sand: return TEXT("Bunker");
        case EOpenFairwaySurface::Fringe: return TEXT("Fringe");
        default: return TEXT("Fairway");
        }
    }

    /**
     * The header's lie: holed, on the tee box, or the surface the ball is on. The header never names the ball's motion;
     * while the ball is in the air it keeps the lie the shot was hit from (SHoleMapPanel::Refresh).
     */
    inline const TCHAR* LieText(bool bHoled, bool bOnTee, EOpenFairwaySurface Surface)
    {
        if (bHoled)
            return TEXT("Holed");
        return bOnTee ? TEXT("Tee") : SurfaceName(Surface);
    }
} // namespace HoleMapLabels
