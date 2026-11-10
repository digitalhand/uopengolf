// SPDX-License-Identifier: MIT
#include "OpenGolfSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "HAL/IConsoleManager.h"

namespace GolfSettings
{
    namespace
    {
        FPreferences Current;
        const TCHAR* Section = TEXT("UOpenGolf.Preferences.v1");
        struct FField { const TCHAR* Name; int32 FPreferences::*Member; };
        const FField Fields[] = {
#define FIELD(Name) {TEXT(#Name), &FPreferences::Name}
            FIELD(MotionBlur), FIELD(DepthOfField), FIELD(LeftHanded), FIELD(DistanceUnits), FIELD(PuttingUnits), FIELD(SpeedUnits), FIELD(AutoTee),
            FIELD(FixedCamera), FIELD(CupZoom), FIELD(Tracer), FIELD(ShotData), FIELD(HudPercent), FIELD(TextPercent),
            FIELD(HighContrast), FIELD(ReducedMotion), FIELD(CalculatedSpin), FIELD(Reconnect), FIELD(Stimp), FIELD(GimmieFeet), FIELD(WindMph),
            FIELD(Provider), FIELD(OpenConnectPort), FIELD(OpenConnectLan)
#undef FIELD
        };
    }
    bool BypassStartupMenu() { return FParse::Param(FCommandLine::Get(), TEXT("NoMainMenu")); }
    void FPreferences::Validate()
    {
        for (int32* Value : {&MotionBlur, &DepthOfField, &LeftHanded, &DistanceUnits, &PuttingUnits, &FixedCamera, &CupZoom, &HighContrast, &ReducedMotion, &CalculatedSpin, &Reconnect, &Provider, &OpenConnectLan})
            *Value = FMath::Clamp(*Value, 0, 1);
        SpeedUnits = FMath::Clamp(SpeedUnits, 0, 2);
        Tracer = FMath::Clamp(Tracer, 0, 2); ShotData = FMath::Clamp(ShotData, 0, 2);
        if (AutoTee != 0 && AutoTee != 3 && AutoTee != 5 && AutoTee != 8) AutoTee = 3;
        if (HudPercent != 100 && HudPercent != 125 && HudPercent != 150) HudPercent = 100;
        if (TextPercent != 100 && TextPercent != 125 && TextPercent != 150) TextPercent = 100;
        Stimp = FMath::Clamp(Stimp, 8, 15);
        if (GimmieFeet != 0 && GimmieFeet != 3 && GimmieFeet != 5 && GimmieFeet != 8 && GimmieFeet != 10) GimmieFeet = 0;
        if (WindMph != -1 && WindMph != 0 && WindMph != 5 && WindMph != 10 && WindMph != 20) WindMph = -1;
        if (OpenConnectPort < 1 || OpenConnectPort > 65535) OpenConnectPort = LaunchMonitorDefaultOpenConnectPort;
    }
    bool FPreferences::operator==(const FPreferences& Other) const
    {
        for (const FField& Field : Fields) if (this->*Field.Member != Other.*Field.Member) return false;
        return true;
    }
    void FPreferences::ReadConfig(const FString& Filename)
    {
        FConfigFile Config; Config.Read(Filename);
        for (const FField& Field : Fields) Config.GetInt(Section, Field.Name, this->*Field.Member);
        Validate();
    }
    bool FPreferences::WriteConfig(const FString& Filename) const
    {
        FConfigFile Config; Config.Read(Filename); Config.bCanSaveAllSections = true;
        for (const FField& Field : Fields) Config.SetInt64(Section, Field.Name, this->*Field.Member);
        return Config.Write(Filename);
    }
    namespace
    {
        IConsoleVariable* FindCVar(const TCHAR* Name) { return IConsoleManager::Get().FindConsoleVariable(Name); }
        int32 CVarInt(const TCHAR* Name, int32 Fallback) { IConsoleVariable* Var = FindCVar(Name); return Var ? Var->GetInt() : Fallback; }
        void SetCVar(const TCHAR* Name, int32 Value) { if (IConsoleVariable* Var = FindCVar(Name)) Var->Set(Value, ECVF_SetByGameSetting); }
    }
    const FPreferences& Get() { return Current; }
    void Load()
    {
        Current = {};
        Current.MotionBlur = CVarInt(TEXT("r.MotionBlurQuality"), 0) > 0;
        Current.DepthOfField = CVarInt(TEXT("r.DepthOfFieldQuality"), 0) > 0;
        // Automation uses deterministic defaults and never reads personal preferences.
        if (!BypassStartupMenu())
            Current.ReadConfig(FPaths::GeneratedConfigDir() / TEXT("UOpenGolfPreferences.ini"));
        Current.Validate();
    }
    void ApplyRuntime(UWorld* World)
    {
        SetCVar(TEXT("r.MotionBlurQuality"), Current.MotionBlur ? 4 : 0);
        SetCVar(TEXT("r.DepthOfFieldQuality"), Current.DepthOfField ? 2 : 0);
        if (World && World->GetGameInstance())
            if (auto* Monitor = World->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
            {
                Monitor->SetLeftHanded(Current.LeftHanded != 0);
                Monitor->SetSpinMode(Current.CalculatedSpin ? ESquareSpinMode::Calculated : ESquareSpinMode::Measured);
                Monitor->SetAutoReconnect(Current.Reconnect != 0);
                Monitor->SetOpenConnectEndpoint(Current.OpenConnectPort, Current.OpenConnectLan != 0);
                // Choosing Open Connect starts listening, so a connector can attach without another step.
                const ELaunchMonitorProvider Provider = Current.Provider ? ELaunchMonitorProvider::OpenConnect : ELaunchMonitorProvider::SquareHome;
                if (Monitor->GetProvider() != Provider)
                {
                    Monitor->SetProvider(Provider);
                    if (Provider == ELaunchMonitorProvider::OpenConnect)
                        Monitor->Connect();
                }
            }
    }
    bool Commit(FPreferences Preferences, UWorld* World)
    {
        Preferences.Validate();
        // Automation runs on defaults and must never overwrite the player's own preferences file.
        if (!BypassStartupMenu() && !Preferences.WriteConfig(FPaths::GeneratedConfigDir() / TEXT("UOpenGolfPreferences.ini"))) return false;
        Current = Preferences;
        ApplyRuntime(World);
        return true;
    }
    float DistanceFactor(bool bShort) { return (bShort ? Current.PuttingUnits : Current.DistanceUnits) ? 1.f : (bShort ? 3.28084f : 1.0936133f); }
    FString DistanceUnit(bool bShort) { return (bShort ? Current.PuttingUnits : Current.DistanceUnits) ? TEXT("m") : (bShort ? TEXT("ft") : TEXT("yd")); }
    float SpeedFactor() { return Current.SpeedUnits == 0 ? 2.2369363f : Current.SpeedUnits == 1 ? 3.6f : 1.f; }
    FString SpeedUnit() { return Current.SpeedUnits == 0 ? TEXT("mph") : Current.SpeedUnits == 1 ? TEXT("km/h") : TEXT("m/s"); }
    FString Distance(float Meters, bool bShort) { return FString::Printf(TEXT("%.1f %s"), Meters * DistanceFactor(bShort), *DistanceUnit(bShort)); }
    FString RoundDistance(float Meters, bool bShort) { return FString::Printf(TEXT("%d %s"), FMath::RoundToInt(Meters * DistanceFactor(bShort)), *DistanceUnit(bShort)); }
    FString Speed(float Mps) { return FString::Printf(TEXT("%.1f %s"), Mps * SpeedFactor(), *SpeedUnit()); }
}
