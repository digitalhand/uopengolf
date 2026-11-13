// SPDX-License-Identifier: MIT
#include "Online/OnlineContract.h"

namespace OpenGolfOnline
{
    EFailure ClassifyStatus(int32 Status)
    {
        if (Status >= 200 && Status < 300) return EFailure::None;
        if (Status == 401) return EFailure::SignedOut;
        if (Status == 404) return EFailure::NotFound;
        if (Status == 400 || Status == 409 || Status == 413 || Status == 422) return EFailure::Rejected;
        return EFailure::RetryLater;
    }

    FString ClubKey(ELaunchMonitorClub Club)
    {
        static const TCHAR* Keys[] = {TEXT("DR"), TEXT("3W"), TEXT("5W"), TEXT("7W"), TEXT("4I"), TEXT("5I"),
            TEXT("6I"), TEXT("7I"), TEXT("8I"), TEXT("9I"), TEXT("PW"), TEXT("GW"), TEXT("SW")};
        const int32 Index = static_cast<int32>(Club);
        return Index >= 0 && Index < UE_ARRAY_COUNT(Keys) ? Keys[Index] : FString();
    }

    static double Round(double Value, double Scale)
    {
        return FMath::Sign(Value) * FMath::FloorToDouble(FMath::Abs(Value) * Scale + 0.5) / Scale;
    }

    static void Measurement(const TSharedRef<FJsonObject>& Object, const TCHAR* Name,
        double Value, bool Present, double Minimum, double Maximum, double Scale = 10)
    {
        const double Rounded = FMath::IsFinite(Value) ? Round(Value, Scale) : Value;
        if (Present && FMath::IsFinite(Rounded) && Rounded >= Minimum && Rounded <= Maximum)
        {
            if (Scale == 1) Object->SetNumberField(Name, Rounded);
            else Object->SetField(Name, MakeShared<FJsonValueNumberString>(FString::Printf(TEXT("%.1f"), Rounded == 0 ? 0.0 : Rounded)));
        }
        else
            Object->SetField(Name, MakeShared<FJsonValueNull>());
    }

    TSharedPtr<FJsonObject> EncodeShot(const FRangeShot& Shot)
    {
        const FString Club = ClubKey(Shot.Club);
        if (!Shot.Id.IsValid() || !Shot.SessionId.IsValid() || Club.IsEmpty()
            || !FMath::IsFinite(Shot.CarryYards) || Shot.CarryYards <= 0 || Shot.CarryYards > 450)
            return nullptr;
        const double Carry = Round(Shot.CarryYards, 10);
        if (Carry < 0.1) return nullptr;
        auto Result = MakeShared<FJsonObject>();
        Result->SetStringField(TEXT("id"), Shot.Id.ToString(EGuidFormats::DigitsWithHyphens));
        Result->SetStringField(TEXT("session_id"), Shot.SessionId.ToString(EGuidFormats::DigitsWithHyphens));
        Result->SetStringField(TEXT("shot_at"), Shot.ShotAt.ToIso8601());
        Result->SetStringField(TEXT("course_id"), TEXT("uopengolf-range"));
        Result->SetStringField(TEXT("club"), Club);
        Result->SetField(TEXT("carry_yd"), MakeShared<FJsonValueNumberString>(FString::Printf(TEXT("%.1f"), Carry)));
        Measurement(Result, TEXT("roll_yd"), Shot.RollYards.Get(0), Shot.RollYards.IsSet(), -999999999.9, 999999999.9);
        auto Details = MakeShared<FJsonObject>();
        const auto& L = Shot.Launch;
        Measurement(Details, TEXT("ball_speed_mph"), L.GetBallSpeedMph(), L.BallSpeedSource != ELaunchMonitorValueSource::Missing, 0.1, 250);
        Measurement(Details, TEXT("launch_angle_deg"), L.VerticalLaunchDeg, L.VerticalLaunchSource != ELaunchMonitorValueSource::Missing, -20, 90);
        Measurement(Details, TEXT("launch_direction_deg"), L.HorizontalLaunchDeg, L.HorizontalLaunchSource != ELaunchMonitorValueSource::Missing, -90, 90);
        Measurement(Details, TEXT("total_spin_rpm"), L.TotalSpinRpm, L.TotalSpinSource != ELaunchMonitorValueSource::Missing, 0, 20000, 1);
        Measurement(Details, TEXT("spin_axis_deg"), L.SpinAxisDeg, L.SpinAxisSource != ELaunchMonitorValueSource::Missing, -90, 90);
        Measurement(Details, TEXT("back_spin_rpm"), L.BackSpinRpm, L.BackSpinSource != ELaunchMonitorValueSource::Missing, -20000, 20000, 1);
        Measurement(Details, TEXT("side_spin_rpm"), L.SideSpinRpm, L.SideSpinSource != ELaunchMonitorValueSource::Missing, -20000, 20000, 1);
        const FLaunchMonitorClubData C = Shot.ClubData.Get(FLaunchMonitorClubData{});
        Measurement(Details, TEXT("club_path_deg"), C.PathDeg, C.bHasPath, -45, 45);
        Measurement(Details, TEXT("face_angle_deg"), C.FaceToTargetDeg, C.bHasFaceToTarget, -45, 45);
        Measurement(Details, TEXT("attack_angle_deg"), C.AngleOfAttackDeg, C.bHasAngleOfAttack, -45, 45);
        Measurement(Details, TEXT("dynamic_loft_deg"), C.DynamicLoftDeg, C.bHasDynamicLoft, -10, 90);
        bool HasDetails=false;
        for (const auto& Field : Details->Values) HasDetails |= !Field.Value->IsNull();
        if (HasDetails) Result->SetObjectField(TEXT("details"), Details);
        else Result->SetField(TEXT("details"), MakeShared<FJsonValueNull>());
        return Result;
    }
}
