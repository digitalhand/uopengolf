// SPDX-License-Identifier: MIT
#pragma once

/** How the hole map's picture is finished: relief, contours and the dimmed surround, kept together like HudStyle. */
namespace HoleMapLook
{
    /** Grid spacing (cm) of the height and distance-from-play fields; finer than the terrain's detail mesh. */
    constexpr double FieldSpacingCm = 300;

    /** Light for the relief, from the map's top left (the cartographic convention), this far above the horizon. */
    constexpr double LightAltitudeDeg = 45;
    /** Slopes are steepened this much for shading only, so gentle course contours still read from above. */
    constexpr double ReliefExaggeration = 3;
    /** How much of the hillshade is multiplied in: 0 none, 1 full. Flat ground is never changed. */
    constexpr double ReliefStrength = 0.35;

    /** Contour intervals (cm) to choose from, finest first; the finest with at most MaxContours lines is used. */
    constexpr double ContourIntervalsCm[] = {100, 200, 500, 1000, 2000, 5000};
    constexpr int32 MaxContours = 25;
    /** Every IndexContourEvery-th line is drawn darker, as on survey maps. */
    constexpr int32 IndexContourEvery = 5;
    constexpr double ContourDarkening = 0.16;
    constexpr double IndexContourDarkening = 0.3;
    /** Contours are this many picture rows wide per 700 rows, so they survive the minimap's downscaling. */
    constexpr int32 ContourRowsPerWidth = 700;

    /** The surround starts to dim this far (cm) from play and is fully dimmed at DimFullCm. */
    constexpr double DimStartCm = 2500;
    constexpr double DimFullCm = 6000;
    /**
     * Distance from play is sampled up to this far (cm). The dimming is full from DimFullCm on, and the field interpolates
     * between samples FieldSpacingCm apart, so any pixel the dimming can still tell apart uses samples below the cap:
     * the picture is the same as with exact distances.
     */
    constexpr double PlayDistanceCapCm = DimFullCm + 2 * FieldSpacingCm;
    /** Fully dimmed ground keeps this share of its saturation and of its brightness. */
    constexpr double DimSaturation = 0.7;
    constexpr double DimBrightness = 0.85;
} // namespace HoleMapLook
