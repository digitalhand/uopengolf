// SPDX-License-Identifier: MIT
// Derived from open-lm-connector (MIT) addons/square/SquareProtocol.cs and
// SquareCommandBuilder.cs at 92eb021abe28d8eb1abe43f87767d3eb73e5939e.
#include "Square/SquareProtocol.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr int32 ShotFrameLength = 17;
    constexpr int32 ClubHeaderLength = 3;
    constexpr int32 ClubFieldsOffset = 3;
    constexpr int32 ImpactHorizontalField = 4;
    constexpr int32 ImpactVerticalField = 5;
    constexpr float MaxImpactHorizontalMm = 40.0f;
    constexpr float MaxImpactVerticalMm = 30.0f;
    // The device sends 0x8000 for a field it could not measure this shot.
    constexpr int16 InvalidReadingSentinel = static_cast<int16>(0x8000);
    // Club frames mark unmeasured fields with 0xFFFF.
    constexpr int16 ClubSentinel = -1;
    // A putt or topped shot can launch slightly downward; a quarter turn or more is garbage.
    constexpr float MaxAbsVerticalAngleDeg = 90.0f;

    int16 ReadInt16LE(TConstArrayView<uint8> Data, int32 Offset)
    {
        return static_cast<int16>(static_cast<uint16>(Data[Offset]) | (static_cast<uint16>(Data[Offset + 1]) << 8));
    }

    int32 ReadInt32LE(TConstArrayView<uint8> Data, int32 Offset)
    {
        return static_cast<int32>(static_cast<uint32>(Data[Offset]) | (static_cast<uint32>(Data[Offset + 1]) << 8) |
                                  (static_cast<uint32>(Data[Offset + 2]) << 16) | (static_cast<uint32>(Data[Offset + 3]) << 24));
    }

    bool ReadInt16(TConstArrayView<uint8> Data, int32 Offset, int16& OutValue)
    {
        const int16 Raw = ReadInt16LE(Data, Offset);
        OutValue = Raw == InvalidReadingSentinel ? 0 : Raw;
        return Raw != InvalidReadingSentinel;
    }

    bool ReadScaledInt16(TConstArrayView<uint8> Data, int32 Offset, float Scale, float& OutValue)
    {
        int16 Raw = 0;
        const bool bValid = ReadInt16(Data, Offset, Raw);
        OutValue = Raw / Scale;
        return bValid;
    }

    ESquareValueSource Source(bool bValid)
    {
        return bValid ? ESquareValueSource::Packet : ESquareValueSource::Missing;
    }

    bool IsFieldFlagged(uint8 Mask, int32 Field)
    {
        return (Mask & (1 << Field)) != 0;
    }

    TOptional<float> ReadClubField(TConstArrayView<uint8> Data, int32 Field)
    {
        const int32 Offset = ClubFieldsOffset + Field * 2;
        if (Data.Num() < Offset + 2)
        {
            return {};
        }
        const int16 Raw = ReadInt16LE(Data, Offset);
        if (Raw == ClubSentinel || Raw == InvalidReadingSentinel)
        {
            return {};
        }
        return Raw / 100.0f;
    }

    // A non-zero mask must flag the field. A zero mask falls back to the sentinel
    // alone, so a device that never fills the mask cannot lose readings.
    TOptional<float> ReadClubAngle(TConstArrayView<uint8> Data, int32 Field)
    {
        const uint8 Mask = Data[2];
        if (Mask != 0 && !IsFieldFlagged(Mask, Field))
        {
            return {};
        }
        return ReadClubField(Data, Field);
    }

    // Impact must be flagged: a device that zero-pads these bytes would otherwise
    // report a false centre strike.
    TOptional<float> ReadClubImpact(TConstArrayView<uint8> Data, int32 Field, float MaxAbsMm)
    {
        if (!IsFieldFlagged(Data[2], Field))
        {
            return {};
        }
        const TOptional<float> Value = ReadClubField(Data, Field);
        return Value.IsSet() && FMath::Abs(Value.GetValue()) <= MaxAbsMm ? Value : TOptional<float>();
    }

    bool IsPlausible(const FSquareShotMetrics& Metrics)
    {
        return Metrics.BallSpeedMps > 0.0f && Metrics.BallSpeedMps < 250.0f && Metrics.TotalSpinRpm >= 0 &&
               Metrics.TotalSpinRpm < 30000 && FMath::Abs(Metrics.VerticalAngleDeg) < MaxAbsVerticalAngleDeg;
    }

    TArray<uint8> CommandFromHex(const FString& Hex)
    {
        TArray<uint8> Bytes;
        verify(SquareProtocol::FromHex(Hex, Bytes));
        return Bytes;
    }
} // namespace

bool FSquareClubMetrics::HasReading() const
{
    return PathDeg.IsSet() || FaceDeg.IsSet() || AttackDeg.IsSet() || DynamicLoftDeg.IsSet() ||
           ImpactHorizontalMm.IsSet() || ImpactVerticalMm.IsSet();
}

bool SquareProtocol::IsSensorFrame(TConstArrayView<uint8> Data)
{
    return Data.Num() >= ShotFrameLength && Data[0] == 0x11 && Data[1] == 0x01;
}

bool SquareProtocol::IsShotFrame(TConstArrayView<uint8> Data)
{
    return Data.Num() >= ShotFrameLength && Data[0] == 0x11 && Data[1] == 0x02;
}

bool SquareProtocol::IsClubFrame(TConstArrayView<uint8> Data)
{
    return Data.Num() >= ClubHeaderLength && Data[0] == 0x11 && Data[1] == 0x07;
}

bool SquareProtocol::TryParseSensor(TConstArrayView<uint8> Data, FSquareSensorData& OutSensor)
{
    if (!IsSensorFrame(Data))
    {
        return false;
    }
    OutSensor.bBallReady = Data[3] == 0x01 || Data[3] == 0x02;
    OutSensor.bBallDetected = Data[4] == 0x01;
    OutSensor.PositionX = ReadInt32LE(Data, 5);
    OutSensor.PositionY = ReadInt32LE(Data, 9);
    OutSensor.PositionZ = ReadInt32LE(Data, 13);
    return true;
}

bool SquareProtocol::TryParseShot(TConstArrayView<uint8> Data, FSquareShotMetrics& OutMetrics)
{
    if (!IsShotFrame(Data))
    {
        return false;
    }

    FSquareShotMetrics Metrics;
    // Byte 2 is opaque metadata on the Home: 0x37 on full swings, 0x13 on putts.
    Metrics.ShotType = Data[2] == 0x37 ? ESquareShotType::Full : Data[2] == 0x13 ? ESquareShotType::Putt
                                                                                 : ESquareShotType::Unknown;

    Metrics.BallSpeedSource = Source(ReadScaledInt16(Data, 3, 100.0f, Metrics.BallSpeedMps));
    Metrics.VerticalAngleSource = Source(ReadScaledInt16(Data, 5, 100.0f, Metrics.VerticalAngleDeg));
    Metrics.HorizontalAngleSource = Source(ReadScaledInt16(Data, 7, 100.0f, Metrics.HorizontalAngleDeg));
    int16 TotalSpin = 0;
    int16 BackSpin = 0;
    int16 SideSpin = 0;
    const bool bTotalSpinValid = ReadInt16(Data, 9, TotalSpin);
    // The device reports spin axis positive-left; decode to positive-right.
    const bool bSpinAxisValid = ReadScaledInt16(Data, 11, -100.0f, Metrics.SpinAxisDeg);
    const bool bBackSpinValid = ReadInt16(Data, 13, BackSpin);
    const bool bSideSpinValid = ReadInt16(Data, 15, SideSpin);
    Metrics.TotalSpinSource = Source(bTotalSpinValid);
    Metrics.SpinAxisSource = Source(bSpinAxisValid);
    Metrics.BackSpinSource = Source(bBackSpinValid);
    Metrics.SideSpinSource = Source(bSideSpinValid);
    Metrics.TotalSpinRpm = TotalSpin;
    Metrics.BackSpinRpm = BackSpin;
    // Side spin shares the axis convention; flip it too.
    Metrics.SideSpinRpm = bSideSpinValid ? -SideSpin : 0;

    // When total spin and axis are known but a component was not measured, derive it.
    if (bTotalSpinValid && bSpinAxisValid)
    {
        const float AxisRadians = FMath::DegreesToRadians(Metrics.SpinAxisDeg);
        if (!bBackSpinValid)
        {
            Metrics.BackSpinRpm = FMath::RoundToInt32(TotalSpin * FMath::Cos(AxisRadians));
            Metrics.BackSpinSource = ESquareValueSource::Derived;
        }
        if (!bSideSpinValid)
        {
            Metrics.SideSpinRpm = FMath::RoundToInt32(TotalSpin * FMath::Sin(AxisRadians));
            Metrics.SideSpinSource = ESquareValueSource::Derived;
        }
    }

    OutMetrics = Metrics;
    return IsPlausible(Metrics);
}

bool SquareProtocol::TryParseClub(TConstArrayView<uint8> Data, FSquareClubMetrics& OutMetrics)
{
    if (!IsClubFrame(Data))
    {
        return false;
    }
    OutMetrics.PathDeg = ReadClubAngle(Data, 0);
    OutMetrics.FaceDeg = ReadClubAngle(Data, 1);
    OutMetrics.AttackDeg = ReadClubAngle(Data, 2);
    OutMetrics.DynamicLoftDeg = ReadClubAngle(Data, 3);
    OutMetrics.ImpactHorizontalMm = ReadClubImpact(Data, ImpactHorizontalField, MaxImpactHorizontalMm);
    OutMetrics.ImpactVerticalMm = ReadClubImpact(Data, ImpactVerticalField, MaxImpactVerticalMm);
    return true;
}

TArray<uint8> SquareProtocol::Heartbeat(uint8 Sequence)
{
    return CommandFromHex(FString::Printf(TEXT("1183%02X0000000000"), Sequence));
}

TArray<uint8> SquareProtocol::DetectBall(uint8 Sequence, int32 Mode, int32 SpinMode)
{
    return CommandFromHex(FString::Printf(TEXT("1181%02X0%d1%d00000000"), Sequence, Mode, SpinMode));
}

TArray<uint8> SquareProtocol::Club(uint8 Sequence, const FString& ClubCode, int32 Handedness)
{
    return CommandFromHex(FString::Printf(TEXT("1182%02X%s0%d000000"), Sequence, *ClubCode, Handedness == 1 ? 1 : 0));
}

TArray<uint8> SquareProtocol::RequestClubMetrics(uint8 Sequence)
{
    return CommandFromHex(FString::Printf(TEXT("1187%02X000000000000"), Sequence));
}

bool SquareProtocol::FromHex(const FString& Hex, TArray<uint8>& OutBytes)
{
    OutBytes.Reset();
    if (Hex.Len() % 2 != 0)
    {
        return false;
    }
    for (int32 Index = 0; Index < Hex.Len(); Index += 2)
    {
        if (!FChar::IsHexDigit(Hex[Index]) || !FChar::IsHexDigit(Hex[Index + 1]))
        {
            OutBytes.Reset();
            return false;
        }
        OutBytes.Add(static_cast<uint8>((FParse::HexDigit(Hex[Index]) << 4) | FParse::HexDigit(Hex[Index + 1])));
    }
    return true;
}

FString SquareProtocol::ToHex(TConstArrayView<uint8> Bytes)
{
    return BytesToHex(Bytes.GetData(), Bytes.Num());
}

FString SquareProtocol::ParseFirmware(TConstArrayView<uint8> Bytes)
{
    const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
    FString Text(Converted.Length(), Converted.Get());
    Text.TrimStartAndEndInline();

    TSharedPtr<FJsonObject> Json;
    FString LaunchMonitorVersion;
    if (Text.StartsWith(TEXT("{")) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Json) && Json.IsValid() &&
        Json->TryGetStringField(TEXT("lm"), LaunchMonitorVersion))
    {
        return LaunchMonitorVersion;
    }
    return Text;
}
