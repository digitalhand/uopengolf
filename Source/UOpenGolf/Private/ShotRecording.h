// SPDX-License-Identifier: MIT
#pragma once

#include "LaunchMonitorTypes.h"
#include "OpenFairwayTypes.h"

/**
 * Saves launch-monitor shots as GSPro Open Connect JSON files, one per shot, so they can be
 * replayed in other tools and compared with this engine. Free of Slate and the world so tests
 * can drive it directly.
 */
namespace ShotRecording
{
    /** Shot-library club code: "driver", "3w", "7i", "pw" and so on. */
    FString ClubCode(ELaunchMonitorClub Club);

    /** <Documents>/UOpenGolf/Saved/ShotRecordings, where sessions are saved by default. */
    FString DefaultRootDir();

    /** "session_20261015-140322" for the given local time. */
    FString SessionFolderName(const FDateTime& Local);

    /** "club_driver_shot_1.json"; the index counts every shot in the session. */
    FString ShotFileName(ELaunchMonitorClub Club, int32 Index);

    /** Only real device shots while a launch monitor is connected are recorded, never replays. */
    bool IsRecordable(const FLaunchMonitorShot& Shot, ELaunchMonitorConnectionState State);

    struct FRecordedShot
    {
        FLaunchMonitorShot Shot;
        TOptional<FLaunchMonitorClubData> ClubData;
        ELaunchMonitorClub SelectedClub = ELaunchMonitorClub::Driver;
        bool bLeftHanded = false;
        FString Firmware;
        FDateTime RecordedLocal;
        TOptional<FOpenFairwayShotResult> Engine;
    };

    /**
     * GSPro-shaped JSON (mph, degrees, rpm) plus a "UOpenGolf" block with the selected club and
     * the engine's result in yards and feet. Unmeasured values are left out, never written as 0.
     */
    FString BuildShotJson(const FRecordedShot& Recorded, int32 Index);

    /**
     * Collects a recordable shot, its club data and the engine result, and writes the file when
     * the ball comes to rest (or when the shot is superseded, recording stops or play ends).
     * Each time recording is switched on, the next shot starts a new session folder.
     */
    class FShotRecorder
    {
    public:
        /** Defaults to DefaultRootDir(). */
        explicit FShotRecorder(FString InRootDir = FString());

        void SetEnabled(bool bNewEnabled);
        bool IsEnabled() const { return bEnabled; }

        /** Shots written to the current session. */
        int32 GetShotCount() const { return ShotCount; }

        /** Empty until the session's first shot is written. */
        const FString& GetSessionDir() const { return SessionDir; }

        void OnShot(const FLaunchMonitorShot& Shot, ELaunchMonitorConnectionState State, ELaunchMonitorClub Club, bool bLeftHanded,
            const FString& Firmware);
        void OnClubData(const FLaunchMonitorClubData& ClubData);
        void OnBallAtRest(const FOpenFairwayShotResult& Result);

        /** Writes the pending shot, if any. */
        void Flush();

    private:
        FString RootDir;
        FString SessionDir;
        int32 ShotCount = 0;
        bool bEnabled = false;
        TOptional<FRecordedShot> Pending;
    };
} // namespace ShotRecording
