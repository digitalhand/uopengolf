// SPDX-License-Identifier: MIT
#pragma once

#include "LaunchMonitorTypes.h"
#include "OpenFairwayTypes.h"

/** Shot data formatted for the range HUD tiles; free of Slate so tests can check it. */
namespace ShotDisplay
{
    enum class ETileGroup : uint8
    {
        /** Physics results: distances, height and landing. */
        Result,
        /** Club delivery measured by the launch monitor. */
        Club,
        /** Raw launch-monitor ball values, shown last in a darker shade. */
        Launch
    };

    struct FTile
    {
        FString Label;
        FString Value;
        ETileGroup Group = ETileGroup::Result;
    };

    /**
     * Tiles in display order, two per row: Total and Carry first, then the other results, the
     * club data and the raw launch-monitor values. Labels and order never change. Missing values
     * show a dash, values that arrive later in the shot show an ellipsis, and launch-monitor
     * values derived from other measurements end in "*".
     */
    TArray<FTile> BuildTiles(const FOpenFairwayShotResult* Result, const FLaunchMonitorShot* Shot, const FLaunchMonitorClubData* Club);

    /** The club picker's standard abbreviation: DR, 3W, 7i, PW, LW, PT. */
    FString ClubLabel(ELaunchMonitorClub Club);

    /** The course heading's first hole line: "Hole 1". */
    FString HoleNumber(int32 Hole);

    /** The course heading's second hole line: "Par 4  ·  400 yd". */
    FString HoleDetails(int32 Par, const FString& Distance);

    /** Record button help, including where shots are saved and how to toggle it. */
    FString RecordTip(bool bRecording, bool bSignedIn);

    /** "v0.0.1" for project version "0.0.1"; empty when no version is set. */
    FString VersionLabel(const FString& ProjectVersion);

    /** ProjectVersion from the [/Script/EngineSettings.GeneralProjectSettings] game config. */
    FString ProjectVersion();
} // namespace ShotDisplay
