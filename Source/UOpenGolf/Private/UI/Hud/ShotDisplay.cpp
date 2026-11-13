// SPDX-License-Identifier: MIT
#include "UI/Hud/ShotDisplay.h"
#include "OpenGolfSettings.h"

#include "Misc/ConfigCacheIni.h"
#include "OpenFairway/ShotSimulator.h"

namespace
{
    const TCHAR* TileDash = TEXT("—");
    const TCHAR* TilePending = TEXT("…");

    /** One decimal place, rounded first so a value shown as zero never prints as "-0.0". */
    FString TileFixed1(float Value)
    {
        const float Rounded = FMath::RoundToFloat(Value * 10.0f) / 10.0f;
        return FString::Printf(TEXT("%.1f"), Rounded == 0.0f ? 0.0f : Rounded);
    }

    /** "L1.4" or "R1.4" (positive is right); a value that rounds to zero has no side. */
    FString TileLeftRight(float Value, const TCHAR* Unit)
    {
        const FString Magnitude = TileFixed1(FMath::Abs(Value));
        const TCHAR* Side = Magnitude == TEXT("0.0") ? TEXT("") : Value < 0.0f ? TEXT("L")
                                                                               : TEXT("R");
        return FString::Printf(TEXT("%s%s%s"), Side, *Magnitude, Unit);
    }

    FString TileLeftRightRpm(int32 Rpm)
    {
        return Rpm == 0 ? FString(TEXT("0")) : FString::Printf(TEXT("%s%d"), Rpm < 0 ? TEXT("L") : TEXT("R"), FMath::Abs(Rpm));
    }

    /** A launch-monitor value: a dash when it was not measured, "*" when derived from other values. */
    FString TileMeasured(ELaunchMonitorValueSource Source, const FString& Value)
    {
        switch (Source)
        {
        case ELaunchMonitorValueSource::Missing:
            return TileDash;
        case ELaunchMonitorValueSource::Derived:
            return Value + TEXT("*");
        default:
            return Value;
        }
    }

    FString TileOptional(bool bHas, float Value, const TCHAR* Unit)
    {
        return bHas ? TileFixed1(Value) + Unit : FString(TileDash);
    }
} // namespace

TArray<ShotDisplay::FTile> ShotDisplay::BuildTiles(const FOpenFairwayShotResult* Result, const FLaunchMonitorShot* Shot, const FLaunchMonitorClubData* Club)
{
    const float ShortFactor = GolfSettings::DistanceFactor(true);
    const float MpsPerMph = 1.f / GolfSettings::SpeedFactor();
    const float LongFactor = GolfSettings::DistanceFactor();

    TArray<FTile> Tiles;
    const auto Add = [&Tiles](const TCHAR* Label, const FString& Value, ETileGroup Group)
    { Tiles.Add(FTile{Label, Value, Group}); };

    // Live values change during the shot. Landing values wait for the first bounce and stay
    // dashes if the ball stopped without one.
    const FOpenFairwayShotResult NoResult;
    const FOpenFairwayShotResult& R = Result ? *Result : NoResult;
    const auto Live = [Result](const FString& Value)
    { return Result ? Value : FString(TileDash); };
    const auto AfterLanding = [Result](bool bKnown, const FString& Value)
    {
        if (!Result || (!bKnown && Result->bAtRest))
        {
            return FString(TileDash);
        }
        return bKnown ? Value : FString(TilePending);
    };
    const bool bInterrupted = R.TerminationReason == EOpenFairwayShotTermination::SafetyStop;
    Add(TEXT("Total (yd)"), bInterrupted ? FString(TileDash) : Live(TileFixed1(R.TotalM * LongFactor)), ETileGroup::Result);
    Add(TEXT("Carry (yd)"), AfterLanding(R.bLanded || (R.bAtRest && !bInterrupted), TileFixed1(R.CarryM * LongFactor)), ETileGroup::Result);
    Add(TEXT("Roll (yd)"), bInterrupted ? FString(TileDash) : AfterLanding(R.bLanded, TileFixed1(R.RollM * LongFactor)), ETileGroup::Result);
    Add(TEXT("Apex (ft)"), Live(TileFixed1(R.ApexM * ShortFactor)), ETileGroup::Result);
    Add(TEXT("Offline (yd)"), Live(TileLeftRight(R.LateralM * LongFactor, TEXT(""))), ETileGroup::Result);
    Add(TEXT("Hang Time (s)"), AfterLanding(R.bLanded, TileFixed1(R.HangTimeS)), ETileGroup::Result);
    Add(TEXT("Land Angle"), AfterLanding(R.bLanded, TileFixed1(R.LandingAngleDeg) + TEXT("°")), ETileGroup::Result);
    Add(TEXT("Land Speed (mph)"), AfterLanding(R.bLanded, TileFixed1(R.LandingSpeedMps / MpsPerMph)), ETileGroup::Result);

    const FLaunchMonitorClubData NoClub;
    const FLaunchMonitorClubData& C = Club ? *Club : NoClub;
    Add(TEXT("Club Path"), TileOptional(C.bHasPath, C.PathDeg, TEXT("°")), ETileGroup::Club);
    Add(TEXT("Face to Target"), TileOptional(C.bHasFaceToTarget, C.FaceToTargetDeg, TEXT("°")), ETileGroup::Club);
    Add(TEXT("Attack Angle"), TileOptional(C.bHasAngleOfAttack, C.AngleOfAttackDeg, TEXT("°")), ETileGroup::Club);
    Add(TEXT("Dynamic Loft"), TileOptional(C.bHasDynamicLoft, C.DynamicLoftDeg, TEXT("°")), ETileGroup::Club);

    // Every source of a default shot is Missing, so no shot shows dashes.
    const FLaunchMonitorShot NoShot;
    const FLaunchMonitorShot& S = Shot ? *Shot : NoShot;
    Add(TEXT("Ball Speed (mph)"), TileMeasured(S.BallSpeedSource, TileFixed1(S.BallSpeedMps * GolfSettings::SpeedFactor())), ETileGroup::Launch);
    Add(TEXT("VLA"), TileMeasured(S.VerticalLaunchSource, TileFixed1(S.VerticalLaunchDeg) + TEXT("°")), ETileGroup::Launch);
    Add(TEXT("Total Spin (rpm)"), TileMeasured(S.TotalSpinSource, FString::FromInt(S.TotalSpinRpm)), ETileGroup::Launch);
    Add(TEXT("HLA"), TileMeasured(S.HorizontalLaunchSource, TileLeftRight(S.HorizontalLaunchDeg, TEXT("°"))), ETileGroup::Launch);
    Add(TEXT("Back Spin (rpm)"), TileMeasured(S.BackSpinSource, FString::FromInt(S.BackSpinRpm)), ETileGroup::Launch);
    Add(TEXT("Side Spin (rpm)"), TileMeasured(S.SideSpinSource, TileLeftRightRpm(S.SideSpinRpm)), ETileGroup::Launch);
    Add(TEXT("Spin Axis"), TileMeasured(S.SpinAxisSource, TileLeftRight(S.SpinAxisDeg, TEXT("°"))), ETileGroup::Launch);
    for (FTile& Tile : Tiles)
    {
        Tile.Label.ReplaceInline(TEXT("(yd)"), *(TEXT("(") + GolfSettings::DistanceUnit() + TEXT(")")));
        Tile.Label.ReplaceInline(TEXT("(ft)"), *(TEXT("(") + GolfSettings::DistanceUnit(true) + TEXT(")")));
        Tile.Label.ReplaceInline(TEXT("(mph)"), *(TEXT("(") + GolfSettings::SpeedUnit() + TEXT(")")));
    }
    return Tiles;
}

FString ShotDisplay::Heading(const FOpenFairwayShotResult* Result)
{
    if (!Result)
    {
        return FString();
    }
    const TCHAR* State = Result->TerminationReason == EOpenFairwayShotTermination::SafetyStop ? TEXT("interrupted") : Result->bAtRest ? TEXT("at rest")
                                                                                                                  : Result->bLanded   ? TEXT("rolling")
                                                                                                                                      : TEXT("in flight");
    return FString::Printf(TEXT("Shot %d · %s"), Result->ShotNumber, State);
}

FString ShotDisplay::ClubLabel(ELaunchMonitorClub Club)
{
    switch (Club)
    {
    case ELaunchMonitorClub::Driver: return TEXT("DR");
    case ELaunchMonitorClub::Wood3: return TEXT("3W");
    case ELaunchMonitorClub::Wood5: return TEXT("5W");
    case ELaunchMonitorClub::Wood7: return TEXT("7W");
    case ELaunchMonitorClub::Iron4: return TEXT("4i");
    case ELaunchMonitorClub::Iron5: return TEXT("5i");
    case ELaunchMonitorClub::Iron6: return TEXT("6i");
    case ELaunchMonitorClub::Iron7: return TEXT("7i");
    case ELaunchMonitorClub::Iron8: return TEXT("8i");
    case ELaunchMonitorClub::Iron9: return TEXT("9i");
    case ELaunchMonitorClub::PitchingWedge: return TEXT("PW");
    case ELaunchMonitorClub::GapWedge: return TEXT("GW");
    case ELaunchMonitorClub::SandWedge: return TEXT("SW");
    case ELaunchMonitorClub::LobWedge: return TEXT("LW");
    case ELaunchMonitorClub::Putter: return TEXT("PT");
    }
    return TEXT("—");
}

FString ShotDisplay::HoleHeading(int32 Par, const FString& Distance, bool bPractice)
{
    return bPractice ? FString(TEXT("Practice  ·  Ball placement")) : FString::Printf(TEXT("Hole 1  ·  Par %d  ·  %s"), Par, *Distance);
}

FString ShotDisplay::LaunchMonitorCaption(ELaunchMonitorConnectionState State)
{
    return State == ELaunchMonitorConnectionState::Disconnected ? TEXT("Connect [C]") : TEXT("Launch monitor");
}

FString ShotDisplay::BatteryLabel(int32 BatteryPercent)
{
    return BatteryPercent < 0 ? FString(TEXT("—")) : FString::Printf(TEXT("%d%%"), BatteryPercent);
}

FString ShotDisplay::LaunchMonitorTip(ELaunchMonitorConnectionState State, int32 BatteryPercent)
{
    FString Tip = StaticEnum<ELaunchMonitorConnectionState>()->GetDisplayNameTextByValue(static_cast<int64>(State)).ToString();
    if (BatteryPercent >= 0)
        Tip += FString::Printf(TEXT(" · battery %d%%"), BatteryPercent);
    // The click opens Settings → Simulator, where Square Home or an Open Connect connector is chosen.
    return Tip + (State == ELaunchMonitorConnectionState::Disconnected ? TEXT(". Click for launch monitor settings. C connects.")
                                                                         : TEXT(". Click for launch monitor settings. T disconnects."));
}

FString ShotDisplay::RecordCount(bool bRecording, int32 ShotCount, int32 FailedRecordings)
{
    const FString Unsaved = FailedRecordings > 0 ? FString::Printf(TEXT("%d unsaved"), FailedRecordings) : FString();
    if (!bRecording)
        return Unsaved;
    return Unsaved.IsEmpty() ? FString::FromInt(ShotCount) : FString::Printf(TEXT("%d · %s"), ShotCount, *Unsaved);
}

FString ShotDisplay::RecordTip(bool bRecording, bool bSignedIn)
{
    if (!bRecording)
        return bSignedIn ? TEXT("Not recording. Click or press F9 to save shots on this computer and to your online profile.")
                         : TEXT("Not recording. Click or press F9 to save shots on this computer.");
    return bSignedIn ? TEXT("Recording: shots are saved on this computer and to your online profile. Click or press F9 to stop.")
                     : TEXT("Recording: shots are saved on this computer. Sign in to also save them to your online profile. Click or press F9 to stop.");
}

FString ShotDisplay::StatusError(const FString& LaunchMonitorError, int32 FailedRecordings, const FString& RecordingError)
{
    return FailedRecordings > 0 ? RecordingError + TEXT(" - retrying") : LaunchMonitorError;
}

FString ShotDisplay::VersionLabel(const FString& ProjectVersion)
{
    const FString Trimmed = ProjectVersion.TrimStartAndEnd();
    return Trimmed.IsEmpty() ? FString() : TEXT("v") + Trimmed;
}

FString ShotDisplay::ProjectVersion()
{
    // Read the config directly so the game module needs no EngineSettings dependency.
    FString Version;
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), Version, GGameIni);
    return Version;
}
