// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "LaunchMonitorTypes.generated.h"

UENUM(BlueprintType)
enum class ELaunchMonitorConnectionState : uint8
{
    Disconnected,
    Scanning,
    Connecting,
    /** Connected and configuring; not yet armed for a shot. */
    Connected,
    /** Armed: the device will report the next shot. */
    Ready
};

/** Where a shot value came from. Missing values are never reported as zero measurements. */
UENUM(BlueprintType)
enum class ELaunchMonitorValueSource : uint8
{
    Missing,
    Measured,
    /** Calculated from other measured values (for example back/side spin from total spin and axis). */
    Derived
};

UENUM(BlueprintType)
enum class ELaunchMonitorShotType : uint8
{
    Unknown,
    Full,
    Putt
};

/** Clubs the Square launch monitor can be configured for. */
UENUM(BlueprintType)
enum class ELaunchMonitorClub : uint8
{
    Driver,
    Wood3 UMETA(DisplayName = "3 Wood"),
    Wood5 UMETA(DisplayName = "5 Wood"),
    Wood7 UMETA(DisplayName = "7 Wood"),
    Iron4 UMETA(DisplayName = "4 Iron"),
    Iron5 UMETA(DisplayName = "5 Iron"),
    Iron6 UMETA(DisplayName = "6 Iron"),
    Iron7 UMETA(DisplayName = "7 Iron"),
    Iron8 UMETA(DisplayName = "8 Iron"),
    Iron9 UMETA(DisplayName = "9 Iron"),
    PitchingWedge UMETA(DisplayName = "Pitching Wedge"),
    GapWedge UMETA(DisplayName = "Gap Wedge"),
    SandWedge UMETA(DisplayName = "Sand Wedge"),
    LobWedge UMETA(DisplayName = "Lob Wedge"),
    Putter
};

/**
 * One measured ball launch. Units are the device's: m/s, degrees and rpm.
 * Positive HLA, spin axis and side spin are to the right.
 */
USTRUCT(BlueprintType)
struct OPENLAUNCHMONITOR_API FLaunchMonitorShot
{
    GENERATED_BODY()

    /** Increases for every shot received in this game instance; club data refers to it. */
    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    int32 ShotNumber = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    FDateTime ReceivedUtc;

    /** "Square Home" or "Replay". */
    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    FString Source;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorShotType ShotType = ELaunchMonitorShotType::Unknown;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float BallSpeedMps = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float VerticalLaunchDeg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float HorizontalLaunchDeg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    int32 TotalSpinRpm = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float SpinAxisDeg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    int32 BackSpinRpm = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    int32 SideSpinRpm = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorValueSource BallSpeedSource = ELaunchMonitorValueSource::Missing;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorValueSource VerticalLaunchSource = ELaunchMonitorValueSource::Missing;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorValueSource HorizontalLaunchSource = ELaunchMonitorValueSource::Missing;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorValueSource TotalSpinSource = ELaunchMonitorValueSource::Missing;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorValueSource SpinAxisSource = ELaunchMonitorValueSource::Missing;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorValueSource BackSpinSource = ELaunchMonitorValueSource::Missing;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    ELaunchMonitorValueSource SideSpinSource = ELaunchMonitorValueSource::Missing;

    float GetBallSpeedMph() const { return BallSpeedMps * 2.2369363f; }
};

/** Club delivery for a shot. Fields the device did not measure have their bHas flag cleared. */
USTRUCT(BlueprintType)
struct OPENLAUNCHMONITOR_API FLaunchMonitorClubData
{
    GENERATED_BODY()

    /** The FLaunchMonitorShot::ShotNumber this club data belongs to. */
    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    int32 ShotNumber = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    bool bHasPath = false;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float PathDeg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    bool bHasFaceToTarget = false;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float FaceToTargetDeg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    bool bHasAngleOfAttack = false;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float AngleOfAttackDeg = 0.0f;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    bool bHasDynamicLoft = false;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float DynamicLoftDeg = 0.0f;

    /** Millimetres from face centre; negative toward the toe (right-handed). */
    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    bool bHasImpactHorizontal = false;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float ImpactHorizontalMm = 0.0f;

    /** Millimetres from face centre; negative is low. */
    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    bool bHasImpactVertical = false;

    UPROPERTY(BlueprintReadOnly, Category = "Launch Monitor")
    float ImpactVerticalMm = 0.0f;

    /** False for an untracked shot (no club sticker, or a strike the device declined). */
    bool HasReading() const
    {
        return bHasPath || bHasFaceToTarget || bHasAngleOfAttack || bHasDynamicLoft || bHasImpactHorizontal || bHasImpactVertical;
    }
};
