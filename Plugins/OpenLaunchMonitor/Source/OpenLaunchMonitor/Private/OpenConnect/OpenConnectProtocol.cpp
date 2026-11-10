// SPDX-License-Identifier: MIT
#include "OpenConnect/OpenConnectProtocol.h"

#include "Dom/JsonObject.h"
#include "LaunchMonitorLimits.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    // The same limits as the Square decoder, so every provider drops the same implausible shots.
    using namespace LaunchMonitorLimits;

    /** Longest DeviceID kept; a connector name is a few words. */
    constexpr int32 MaxDeviceIdChars = 64;

    bool Number(const FJsonObject& Object, const TCHAR* Name, double& OutValue)
    {
        return Object.TryGetNumberField(Name, OutValue);
    }

    /**
     * Reads a number that must be finite and within [Min, Max]. It is checked before any conversion,
     * so a huge or non-finite value can never overflow a rounding or reach the physics.
     * False only when the field is present and bad; a missing field leaves OutValue unset.
     */
    bool ReadBounded(const FJsonObject& Object, const TCHAR* Name, double Min, double Max, TOptional<double>& OutValue)
    {
        double Value = 0;
        if (!Number(Object, Name, Value))
            return true;
        if (!FMath::IsFinite(Value) || Value < Min || Value > Max)
            return false;
        OutValue = Value;
        return true;
    }

    bool SetFloat(const FJsonObject& Object, const TCHAR* Name, double AbsMax, float& OutValue, ELaunchMonitorValueSource& OutSource)
    {
        TOptional<double> Value;
        if (!ReadBounded(Object, Name, -AbsMax, AbsMax, Value))
            return false;
        if (Value.IsSet())
        {
            OutValue = static_cast<float>(Value.GetValue());
            OutSource = ELaunchMonitorValueSource::Measured;
        }
        return true;
    }

    bool SetRpm(const FJsonObject& Object, const TCHAR* Name, double Min, int32& OutValue, ELaunchMonitorValueSource& OutSource)
    {
        TOptional<double> Value;
        if (!ReadBounded(Object, Name, Min, MaxSpinRpm, Value))
            return false;
        if (Value.IsSet())
        {
            OutValue = FMath::RoundToInt32(Value.GetValue());
            OutSource = ELaunchMonitorValueSource::Measured;
        }
        return true;
    }

    /** Club fields are optional extras: an implausible or non-finite one is dropped, not the shot. */
    void SetClub(const FJsonObject& Object, const TCHAR* Name, double Min, double Max, bool& bOutHas, float& OutValue)
    {
        TOptional<double> Value;
        bOutHas = ReadBounded(Object, Name, Min, Max, Value) && Value.IsSet();
        OutValue = bOutHas ? static_cast<float>(Value.GetValue()) : 0.0f;
    }

    bool Has(ELaunchMonitorValueSource Source) { return Source != ELaunchMonitorValueSource::Missing; }

    /** Fills whichever of total/axis or back/side the connector left out, as the Square decoder does. */
    void DeriveSpin(FLaunchMonitorShot& Shot)
    {
        if (Has(Shot.TotalSpinSource) && Has(Shot.SpinAxisSource))
        {
            const float AxisRadians = FMath::DegreesToRadians(Shot.SpinAxisDeg);
            if (!Has(Shot.BackSpinSource))
            {
                Shot.BackSpinRpm = FMath::RoundToInt32(Shot.TotalSpinRpm * FMath::Cos(AxisRadians));
                Shot.BackSpinSource = ELaunchMonitorValueSource::Derived;
            }
            if (!Has(Shot.SideSpinSource))
            {
                Shot.SideSpinRpm = FMath::RoundToInt32(Shot.TotalSpinRpm * FMath::Sin(AxisRadians));
                Shot.SideSpinSource = ELaunchMonitorValueSource::Derived;
            }
        }
        else if (Has(Shot.BackSpinSource) && Has(Shot.SideSpinSource))
        {
            if (!Has(Shot.TotalSpinSource))
            {
                Shot.TotalSpinRpm = FMath::RoundToInt32(FMath::Sqrt(float(FMath::Square(Shot.BackSpinRpm) + FMath::Square(Shot.SideSpinRpm))));
                Shot.TotalSpinSource = ELaunchMonitorValueSource::Derived;
            }
            if (!Has(Shot.SpinAxisSource))
            {
                Shot.SpinAxisDeg = FMath::RadiansToDegrees(FMath::Atan2(float(Shot.SideSpinRpm), float(Shot.BackSpinRpm)));
                Shot.SpinAxisSource = ELaunchMonitorValueSource::Derived;
            }
        }
    }

    FString Write(const TSharedRef<FJsonObject>& Object)
    {
        FString Json;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
        FJsonSerializer::Serialize(Object, Writer);
        return Json;
    }
} // namespace

bool FOpenConnectFramer::Append(const uint8* Data, int32 Count)
{
    Buffer.Append(Data, Count);
    if (Buffer.Num() > MaxBufferBytes)
    {
        Buffer.Reset();
        return false;
    }
    return true;
}

bool FOpenConnectFramer::Next(FString& OutJson)
{
    // Braces and quotes are ASCII, and UTF-8 never uses ASCII bytes inside a multi-byte character,
    // so matching on bytes is safe. Braces inside strings are skipped, honoring escapes.
    int32 Depth = 0;
    int32 Start = INDEX_NONE;
    bool bInString = false;
    bool bEscape = false;
    for (int32 Index = 0; Index < Buffer.Num(); ++Index)
    {
        const uint8 Byte = Buffer[Index];
        if (bEscape)
        {
            bEscape = false;
            continue;
        }
        if (bInString)
        {
            if (Byte == '\\')
                bEscape = true;
            else if (Byte == '"')
                bInString = false;
            continue;
        }
        if (Byte == '"' && Depth > 0)
        {
            bInString = true;
        }
        else if (Byte == '{')
        {
            if (Depth++ == 0)
                Start = Index;
        }
        else if (Byte == '}' && Depth > 0 && --Depth == 0)
        {
            const int32 Length = Index - Start + 1;
            const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Buffer.GetData() + Start), Length);
            OutJson = FString::ConstructFromPtrSize(Text.Get(), Text.Length());
            Buffer.RemoveAt(0, Index + 1, EAllowShrinking::No);
            return true;
        }
    }
    if (Start == INDEX_NONE)
    {
        // Nothing but whitespace or junk: drop it so a stream that never opens an object cannot grow.
        Buffer.Reset();
    }
    else if (Start > 0)
    {
        Buffer.RemoveAt(0, Start, EAllowShrinking::No);
    }
    return false;
}

namespace OpenConnectProtocol
{
    FString SanitizeText(const FString& Text, int32 MaxChars)
    {
        FString Clean;
        Clean.Reserve(FMath::Min(Text.Len(), MaxChars));
        for (const TCHAR Character : Text)
        {
            const uint32 Code = static_cast<uint32>(Character);
            // C0/C1 controls (a newline forges log lines) and bidirectional overrides (text shown reversed).
            const bool bControl = Code < 0x20 || (Code >= 0x7F && Code < 0xA0);
            const bool bBidi = (Code >= 0x202A && Code <= 0x202E) || (Code >= 0x2066 && Code <= 0x2069);
            if (bControl || bBidi)
                continue;
            if (Clean.Len() >= MaxChars)
                break;
            Clean.AppendChar(Character);
        }
        // Never end on half of a UTF-16 surrogate pair.
        if (!Clean.IsEmpty() && FMath::IsWithinInclusive(static_cast<uint32>(Clean[Clean.Len() - 1]), 0xD800u, 0xDBFFu))
            Clean.LeftChopInline(1, EAllowShrinking::No);
        return Clean;
    }

    bool Decode(const FString& Json, FOpenConnectMessage& OutMessage, FString& OutError)
    {
        OutMessage = FOpenConnectMessage();
        TSharedPtr<FJsonObject> Root;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<TCHAR>::Create(Json), Root) || !Root.IsValid())
        {
            OutError = TEXT("Bad JSON data");
            return false;
        }

        FString DeviceId;
        if (Root->TryGetStringField(TEXT("DeviceID"), DeviceId))
            OutMessage.DeviceId = SanitizeText(DeviceId, MaxDeviceIdChars);
        TOptional<double> DeviceShotNumber;
        if (ReadBounded(*Root, TEXT("ShotNumber"), 0, TNumericLimits<int32>::Max(), DeviceShotNumber) && DeviceShotNumber.IsSet())
            OutMessage.DeviceShotNumber = FMath::RoundToInt32(DeviceShotNumber.GetValue());

        const TSharedPtr<FJsonObject>* Options = nullptr;
        const TSharedPtr<FJsonObject>* Ball = nullptr;
        const TSharedPtr<FJsonObject>* Club = nullptr;
        Root->TryGetObjectField(TEXT("ShotDataOptions"), Options);
        Root->TryGetObjectField(TEXT("BallData"), Ball);
        Root->TryGetObjectField(TEXT("ClubData"), Club);

        // ShotDataOptions is required by the spec, but a message without it is read by what it carries.
        bool bContainsBall = Ball != nullptr;
        bool bContainsClub = Club != nullptr;
        if (Options)
        {
            (*Options)->TryGetBoolField(TEXT("ContainsBallData"), bContainsBall);
            (*Options)->TryGetBoolField(TEXT("ContainsClubData"), bContainsClub);
            (*Options)->TryGetBoolField(TEXT("IsHeartBeat"), OutMessage.bHeartbeat);
            bool bReady = false;
            if ((*Options)->TryGetBoolField(TEXT("LaunchMonitorIsReady"), bReady))
                OutMessage.bReady = bReady;
        }
        if (OutMessage.bHeartbeat || !bContainsBall)
        {
            // A heartbeat or a status update: nothing to fly.
            return true;
        }
        if (!Ball)
        {
            OutError = TEXT("ContainsBallData is set but BallData is missing");
            return false;
        }

        // Open Connect ball speed is in mph; angles in degrees, spin in rpm; positive HLA, spin axis
        // and side spin are to the right, which is already this plugin's convention.
        FLaunchMonitorShot Shot;
        float SpeedMph = 0.0f;
        const FJsonObject& BallObject = **Ball;
        const bool bInRange =
            SetFloat(BallObject, TEXT("Speed"), MaxBallSpeedMps * LaunchMonitorMphPerMps, SpeedMph, Shot.BallSpeedSource) &&
            SetFloat(BallObject, TEXT("VLA"), MaxAbsVerticalAngleDeg, Shot.VerticalLaunchDeg, Shot.VerticalLaunchSource) &&
            SetFloat(BallObject, TEXT("HLA"), MaxAbsHorizontalAngleDeg, Shot.HorizontalLaunchDeg, Shot.HorizontalLaunchSource) &&
            SetRpm(BallObject, TEXT("TotalSpin"), 0, Shot.TotalSpinRpm, Shot.TotalSpinSource) &&
            SetFloat(BallObject, TEXT("SpinAxis"), MaxAbsSpinAxisDeg, Shot.SpinAxisDeg, Shot.SpinAxisSource) &&
            SetRpm(BallObject, TEXT("BackSpin"), -MaxSpinRpm, Shot.BackSpinRpm, Shot.BackSpinSource) &&
            SetRpm(BallObject, TEXT("SideSpin"), -MaxSpinRpm, Shot.SideSpinRpm, Shot.SideSpinSource);
        if (!bInRange)
        {
            OutError = TEXT("Ball data is outside plausible limits");
            return false;
        }
        Shot.BallSpeedMps = SpeedMph / LaunchMonitorMphPerMps;
        if (!Has(Shot.BallSpeedSource) || !Has(Shot.VerticalLaunchSource))
        {
            OutError = TEXT("BallData needs Speed and VLA");
            return false;
        }
        DeriveSpin(Shot);
        if (Shot.BallSpeedMps <= 0.0f || Shot.BallSpeedMps >= MaxBallSpeedMps || Shot.TotalSpinRpm < 0 || Shot.TotalSpinRpm >= MaxSpinRpm ||
            FMath::Abs(Shot.VerticalLaunchDeg) >= MaxAbsVerticalAngleDeg)
        {
            OutError = TEXT("Ball data is outside plausible limits");
            return false;
        }
        Shot.ShotType = ELaunchMonitorShotType::Unknown;
        OutMessage.Shot = Shot;

        if (bContainsClub && Club)
        {
            FLaunchMonitorClubData Data;
            const FJsonObject& ClubObject = **Club;
            SetClub(ClubObject, TEXT("Path"), -MaxAbsClubAngleDeg, MaxAbsClubAngleDeg, Data.bHasPath, Data.PathDeg);
            SetClub(ClubObject, TEXT("FaceToTarget"), -MaxAbsClubAngleDeg, MaxAbsClubAngleDeg, Data.bHasFaceToTarget, Data.FaceToTargetDeg);
            SetClub(ClubObject, TEXT("AngleOfAttack"), -MaxAbsClubAngleDeg, MaxAbsClubAngleDeg, Data.bHasAngleOfAttack, Data.AngleOfAttackDeg);
            SetClub(ClubObject, TEXT("Loft"), MinDynamicLoftDeg, MaxDynamicLoftDeg, Data.bHasDynamicLoft, Data.DynamicLoftDeg);
            SetClub(ClubObject, TEXT("HorizontalFaceImpact"), -MaxImpactHorizontalMm, MaxImpactHorizontalMm, Data.bHasImpactHorizontal, Data.ImpactHorizontalMm);
            SetClub(ClubObject, TEXT("VerticalFaceImpact"), -MaxImpactVerticalMm, MaxImpactVerticalMm, Data.bHasImpactVertical, Data.ImpactVerticalMm);
            OutMessage.Club = Data;
        }
        return true;
    }

    FString Response(int32 Code, const FString& Message)
    {
        const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("Code"), Code);
        Object->SetStringField(TEXT("Message"), Message);
        return Write(Object);
    }

    FString PlayerInformation(bool bLeftHanded, ELaunchMonitorClub Club)
    {
        const TSharedRef<FJsonObject> Player = MakeShared<FJsonObject>();
        Player->SetStringField(TEXT("Handed"), bLeftHanded ? TEXT("LH") : TEXT("RH"));
        Player->SetStringField(TEXT("Club"), ClubCode(Club));
        const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
        Object->SetNumberField(TEXT("Code"), 201);
        Object->SetStringField(TEXT("Message"), TEXT("Player Information"));
        Object->SetObjectField(TEXT("Player"), Player);
        return Write(Object);
    }

    const TCHAR* ClubCode(ELaunchMonitorClub Club)
    {
        switch (Club)
        {
        case ELaunchMonitorClub::Wood3: return TEXT("W3");
        case ELaunchMonitorClub::Wood5: return TEXT("W5");
        case ELaunchMonitorClub::Wood7: return TEXT("W7");
        case ELaunchMonitorClub::Iron4: return TEXT("I4");
        case ELaunchMonitorClub::Iron5: return TEXT("I5");
        case ELaunchMonitorClub::Iron6: return TEXT("I6");
        case ELaunchMonitorClub::Iron7: return TEXT("I7");
        case ELaunchMonitorClub::Iron8: return TEXT("I8");
        case ELaunchMonitorClub::Iron9: return TEXT("I9");
        case ELaunchMonitorClub::PitchingWedge: return TEXT("PW");
        case ELaunchMonitorClub::GapWedge: return TEXT("GW");
        case ELaunchMonitorClub::SandWedge: return TEXT("SW");
        case ELaunchMonitorClub::LobWedge: return TEXT("LW");
        case ELaunchMonitorClub::Putter: return TEXT("PT");
        case ELaunchMonitorClub::Driver:
        default: return TEXT("DR");
        }
    }
} // namespace OpenConnectProtocol
