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
        int32 MotionBlur = 0, DepthOfField = 0;
        int32 LeftHanded = 0, DistanceUnits = 0, PuttingUnits = 0, SpeedUnits = 0;
        int32 AutoTee = 3, FixedCamera = 0, CupZoom = 1, Tracer = 1, ShotData = 2;
        int32 HudPercent = 100, TextPercent = 100, HighContrast = 0, ReducedMotion = 0;
        int32 CalculatedSpin = 0, Reconnect = 1, Stimp = 10, GimmieFeet = 0, WindMph = -1;
        /** Launch-monitor provider (ELaunchMonitorProvider: 0 Square Home, 1 Open Connect), its TCP port, and LAN access (0/1). */
        int32 Provider = 0, OpenConnectPort = LaunchMonitorDefaultOpenConnectPort, OpenConnectLan = 0;
        /** Index into TimeZones() for shot-history times; 0 is this computer's zone. */
        int32 TimeZone = 0;
        void Validate();
        void ReadConfig(const FString& Filename);
        bool WriteConfig(const FString& Filename) const;
        bool operator==(const FPreferences& Other) const;
        bool operator!=(const FPreferences& Other) const { return !(*this == Other); }
    };
    /** A time zone the player can choose: its menu label, IANA ID (empty is this computer's zone) and short name. */
    struct FTimeZoneChoice
    {
        const TCHAR* Label;
        const TCHAR* Id;
        const TCHAR* Short;
    };
    /** The time zones offered in Settings › Player. Stored by index, so new zones are only ever appended. */
    UOPENGOLF_API TConstArrayView<FTimeZoneChoice> TimeZones();
    /** The chosen zone of Get(). */
    UOPENGOLF_API const FTimeZoneChoice& ChosenTimeZone();
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
