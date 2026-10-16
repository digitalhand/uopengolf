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

    /** Write and close a same-directory temporary file, then rename without replacing a final file. */
    bool WriteRecordingFile(const FString& Path, const FString& Json, FString& Error);

    /** In-memory collection and retry queue. Clock and atomic writer are injectable for tests. */
    class FShotRecorder
    {
    public:
        using FWriter = TFunction<bool(const FString&, const FString&, FString&)>;
        explicit FShotRecorder(FString InRootDir = FString(), TFunction<double()> InClock = {}, FWriter InWriter = {});
        void SetEnabled(bool bNewEnabled);
        bool IsEnabled() const { return bEnabled; }
        int32 GetShotCount() const { return CurrentSession ? CurrentSession->Saved : 0; }
        const FString& GetSessionDir() const { return SessionDir; }
        int32 GetFailedSaveCount() const;
        const FString& GetLastError() const { return LastError; }
        void OnShot(const FLaunchMonitorShot& Shot, ELaunchMonitorConnectionState State, ELaunchMonitorClub Club,
            bool bLeftHanded, const FString& Firmware);
        void BindSimulation(int32 DeviceShot, int32 SimulationShot);
        void CancelSimulation();
        void OnClubData(const FLaunchMonitorClubData& ClubData);
        void OnBallAtRest(const FOpenFairwayShotResult& Result);
        void Tick();
        /** Finalize available data and make one final attempt, irrespective of retry delay. */
        void Flush();
        void Shutdown();

    private:
        struct FSession
        {
            FString Directory;
            int32 NextIndex = 0;
            int32 Saved = 0;
        };
        struct FPending
        {
            FRecordedShot Record;
            TSharedPtr<FSession> Session;
            int32 Index = 0;
            int32 SimulationShot = INDEX_NONE;
            double ClubDeadline = 0;
            bool bSimulationDone = false;
            bool bFinalized = false;
        };
        void Process(bool bForceRetry = false);
        FString RootDir, SessionDir, LastError;
        TSharedPtr<FSession> CurrentSession;
        TArray<FPending> Pending;
        TFunction<double()> Clock;
        FWriter Writer;
        double NextRetry = 0;
        bool bEnabled = false;
    };
} // namespace ShotRecording
