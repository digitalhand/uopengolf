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

    /** "Shot 3 · rolling", or "Waiting for a shot" before the first launch. */
    FString Heading(const FOpenFairwayShotResult* Result);

    /** Shot-recording status: "Off", or "On · 3" with the shots saved this session. */
    FString RecordingStatus(bool bRecording, int32 ShotCount);

    /**
     * The status box's bottom line: an unsaved-recording error first, then a launch-monitor
     * error (with the connect key while disconnected), otherwise the key tips.
     */
    FString StatusTip(ELaunchMonitorConnectionState State, const FString& LaunchMonitorError, int32 FailedRecordings,
        const FString& RecordingError);

    /** "Straight", or the aim in whole degrees off the target line: "12° L", "12° R". */
    FString AimLabel(float AimYawDeg);

    /** "v0.0.1" for project version "0.0.1"; empty when no version is set. */
    FString VersionLabel(const FString& ProjectVersion);

    /** ProjectVersion from the [/Script/EngineSettings.GeneralProjectSettings] game config. */
    FString ProjectVersion();
} // namespace ShotDisplay
