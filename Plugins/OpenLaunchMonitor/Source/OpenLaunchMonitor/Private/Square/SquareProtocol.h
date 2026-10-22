// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/square/SquareProtocol.cs,
// SquareCommandBuilder.cs, SquareShotMetrics.cs and SquareConnectionOptions.cs
// at 92eb021abe28d8eb1abe43f87767d3eb73e5939e.
#pragma once

#include "CoreMinimal.h"

/** Where a decoded value came from. */
enum class ESquareValueSource : uint8
{
    Missing,
    Packet,
    Derived
};

enum class ESquareShotType : uint8
{
    Unknown,
    Full,
    Putt
};

/** Ball metrics from a `11 02` frame, in device units (spin axis and side spin: positive = right). */
struct FSquareShotMetrics
{
    float BallSpeedMps = 0.0f;
    float VerticalAngleDeg = 0.0f;
    float HorizontalAngleDeg = 0.0f;
    int32 TotalSpinRpm = 0;
    float SpinAxisDeg = 0.0f;
    int32 BackSpinRpm = 0;
    int32 SideSpinRpm = 0;
    ESquareShotType ShotType = ESquareShotType::Unknown;

    ESquareValueSource BallSpeedSource = ESquareValueSource::Missing;
    ESquareValueSource VerticalAngleSource = ESquareValueSource::Missing;
    ESquareValueSource HorizontalAngleSource = ESquareValueSource::Missing;
    ESquareValueSource TotalSpinSource = ESquareValueSource::Missing;
    ESquareValueSource SpinAxisSource = ESquareValueSource::Missing;
    ESquareValueSource BackSpinSource = ESquareValueSource::Missing;
    ESquareValueSource SideSpinSource = ESquareValueSource::Missing;
};

/**
 * Club metrics from a `11 07` frame. An unset field was not measured (sentinel,
 * putter, or not reported by this device). Impact is mm from face centre.
 */
struct FSquareClubMetrics
{
    TOptional<float> PathDeg;
    TOptional<float> FaceDeg;
    TOptional<float> AttackDeg;
    TOptional<float> DynamicLoftDeg;
    TOptional<float> ImpactHorizontalMm;
    TOptional<float> ImpactVerticalMm;

    /** False for an untracked shot (no club sticker, or a strike the device declined). */
    bool HasReading() const;
};

struct FSquareSensorData
{
    bool bBallReady = false;
    bool bBallDetected = false;
    int32 PositionX = 0;
    int32 PositionY = 0;
    int32 PositionZ = 0;
};

/** Characteristic UUIDs and timings for the Square launch monitor. */
struct FSquareConnectionOptions
{
    FString DeviceNamePrefix = TEXT("SquareGolf");
    FString CommandCharacteristicUuid = TEXT("86602101-6b7e-439a-bdd1-489a3213e9bb");
    FString EventCharacteristicUuid = TEXT("86602102-6b7e-439a-bdd1-489a3213e9bb");
    FString BatteryCharacteristicUuid = TEXT("00002a19-0000-1000-8000-00805f9b34fb");
    FString FirmwareCharacteristicUuid = TEXT("86602003-6b7e-439a-bdd1-489a3213e9bb");
    float ConnectionClubDelaySeconds = 2.0f;
    float ConnectionReadyDelaySeconds = 3.0f;
    float HeartbeatIntervalSeconds = 5.0f;
    float ScanTimeoutSeconds = 30.0f;
};

namespace SquareProtocol
{
    inline const TCHAR* DriverClubCode = TEXT("0204");
    inline const TCHAR* PutterClubCode = TEXT("0107");

    bool IsSensorFrame(TConstArrayView<uint8> Data);
    bool IsShotFrame(TConstArrayView<uint8> Data);
    bool IsClubFrame(TConstArrayView<uint8> Data);

    bool TryParseSensor(TConstArrayView<uint8> Data, FSquareSensorData& OutSensor);
    /** False for a non-shot frame and for an implausible reading. */
    bool TryParseShot(TConstArrayView<uint8> Data, FSquareShotMetrics& OutMetrics);
    bool TryParseClub(TConstArrayView<uint8> Data, FSquareClubMetrics& OutMetrics);

    TArray<uint8> Heartbeat(uint8 Sequence);
    /** Arms ball detection. SpinMode: 1 = measured, 0 = calculated. */
    TArray<uint8> DetectBall(uint8 Sequence, int32 Mode, int32 SpinMode);
    /** Selects the club. ClubCode is four hex digits such as "0204"; Handedness 0 = right, 1 = left. */
    TArray<uint8> Club(uint8 Sequence, const FString& ClubCode, int32 Handedness);
    /** Asks the device for the `11 07` club frame of the last shot. */
    TArray<uint8> RequestClubMetrics(uint8 Sequence);

    /** Parses an even-length hex string; returns false for malformed input. */
    bool FromHex(const FString& Hex, TArray<uint8>& OutBytes);
    FString ToHex(TConstArrayView<uint8> Bytes);

    /** Firmware characteristic text, or the `lm` value when it is JSON. */
    FString ParseFirmware(TConstArrayView<uint8> Bytes);
} // namespace SquareProtocol
