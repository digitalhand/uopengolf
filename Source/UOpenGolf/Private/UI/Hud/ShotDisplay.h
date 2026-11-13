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

    /** "Shot 3 · rolling"; empty before the first launch, when the ball-ready glow says whether a shot can be taken. */
    FString Heading(const FOpenFairwayShotResult* Result);

    /** The club picker's standard abbreviation: DR, 3W, 7i, PW, LW, PT. */
    FString ClubLabel(ELaunchMonitorClub Club);

    /** "Hole 1  ·  Par 4  ·  400 yd", or "Practice  ·  Ball placement" while placing a ball or on the practice hole. */
    FString HoleHeading(int32 Par, const FString& Distance, bool bPractice);

    /** Caption under the launch-monitor icon: the connect key while disconnected, otherwise "Launch monitor". */
    FString LaunchMonitorCaption(ELaunchMonitorConnectionState State);

    /** "80%", or a dash when the launch monitor reports no battery level (negative). */
    FString BatteryLabel(int32 BatteryPercent);

    /** Accessible name of the launch-monitor icon (never shown): the state, the battery when known, what a click does and the key. */
    FString LaunchMonitorTip(ELaunchMonitorConnectionState State, int32 BatteryPercent);

    /** Beside the record icon: shots saved this session while recording, and any unsaved shots. Empty when neither. */
    FString RecordCount(bool bRecording, int32 ShotCount, int32 FailedRecordings);

    /** Accessible name of the record icon (never shown): where recorded shots are saved, which depends on being signed in, and how to toggle. */
    FString RecordTip(bool bRecording, bool bSignedIn);

    /** Error text for the status box: an unsaved-recording error first, then a launch-monitor error; empty when neither. */
    FString StatusError(const FString& LaunchMonitorError, int32 FailedRecordings, const FString& RecordingError);

    /** "Straight", or the aim in whole degrees off the target line: "12° L", "12° R". */
    FString AimLabel(float AimYawDeg);

    /** "v0.0.1" for project version "0.0.1"; empty when no version is set. */
    FString VersionLabel(const FString& ProjectVersion);

    /** ProjectVersion from the [/Script/EngineSettings.GeneralProjectSettings] game config. */
    FString ProjectVersion();
} // namespace ShotDisplay
