// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "LaunchMonitorTypes.h"
class UWorld;

namespace GolfSettings
{
    /** Player preferences; Game Defaults are copied into a session, never read mid-shot. */
    struct UOPENGOLF_API FPreferences
    {
        FString DiagnosticDirectory, RecordingDirectory;
        /**
         * The built-in avatar the player picked (an AvatarCatalog id), and the account key it was picked for. Empty: the
         * account picture, else the default avatar. Another account signed in on this computer ignores the pick.
         */
        FString AvatarId, AvatarAccount;
        int32 DiagnosticLevel = 3, RecordingDetail = 1;
        int32 MotionBlur = 0, DepthOfField = 0;
        int32 LeftHanded = 0, DistanceUnits = 0, PuttingUnits = 0, SpeedUnits = 0;
        int32 AutoTee = 3, FixedCamera = 0, CupZoom = 1, Tracer = 1, ShotData = 2;
        int32 HudPercent = 100, TextPercent = 100, HighContrast = 0, ReducedMotion = 0;
        int32 CalculatedSpin = 0, Reconnect = 1, Stimp = 10, GimmieFeet = 0, WindMph = -1;
        /** Course rounds pick the club for each shot from its lie and distance (0/1). */
        int32 AutoClub = 1;
        /** Fairway speed for the whole game, in tenths of a stimp foot: 60, 74 (the fitted standard), 80, 90 or 100. */
        int32 FairwayStimpTenths = 74;
        /** Launch-monitor provider (ELaunchMonitorProvider: 0 Square Home, 1 Open Connect), its TCP port, and LAN access (0/1). */
        int32 Provider = 0, OpenConnectPort = LaunchMonitorDefaultOpenConnectPort, OpenConnectLan = 0;
        void Validate();
        void ReadConfig(const FString& Filename);
        bool WriteConfig(const FString& Filename) const;
        bool operator==(const FPreferences& Other) const;
        bool operator!=(const FPreferences& Other) const { return !(*this == Other); }
    };
    /** The preferences' fairway speed as a stimp reading in feet (AOpenFairwayBall::FairwayStimpFeet). */
    UOPENGOLF_API float FairwayStimpFeet(const FPreferences& P);
    UOPENGOLF_API FString DiagnosticPath(const FPreferences& P);
    UOPENGOLF_API FString RecordingPath(const FPreferences& P);
    UOPENGOLF_API FString ValidateDataDirectories(const FPreferences& P);
    void ShutdownDiagnostics();
    UOPENGOLF_API const FPreferences& Get();
    UOPENGOLF_API void Load();
    UOPENGOLF_API void ApplyRuntime(UWorld* World);
    UOPENGOLF_API bool Commit(FPreferences Preferences, UWorld* World);
    UOPENGOLF_API FString Distance(float Meters, bool bShort = false);
    /** Whole units ("152 yd", "18 ft"), for compact labels such as distance to the pin. */
    UOPENGOLF_API FString RoundDistance(float Meters, bool bShort = false);
    UOPENGOLF_API FString Speed(float Mps);
    UOPENGOLF_API float DistanceFactor(bool bShort = false);
    UOPENGOLF_API FString DistanceUnit(bool bShort = false);
    UOPENGOLF_API float SpeedFactor();
    UOPENGOLF_API FString SpeedUnit();
    UOPENGOLF_API bool BypassStartupMenu();
}
