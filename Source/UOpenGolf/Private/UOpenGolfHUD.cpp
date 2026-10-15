// SPDX-License-Identifier: MIT
#include "UOpenGolfHUD.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UOpenGolfGameMode.h"

namespace
{
    const TCHAR* Dash = TEXT("—");

    FString FormatValue(ELaunchMonitorValueSource Source, const FString& Value, const TCHAR* Unit)
    {
        if (Source == ELaunchMonitorValueSource::Missing)
        {
            return Dash;
        }
        // Derived values are marked so they are not mistaken for device measurements.
        return FString::Printf(TEXT("%s%s%s"), *Value, Unit, Source == ELaunchMonitorValueSource::Derived ? TEXT("*") : TEXT(""));
    }

    FString FormatOptional(bool bHas, float Value, const TCHAR* Unit)
    {
        return bHas ? FString::Printf(TEXT("%.1f%s"), Value, Unit) : FString(Dash);
    }

    FLinearColor StateColor(ELaunchMonitorConnectionState State)
    {
        switch (State)
        {
        case ELaunchMonitorConnectionState::Ready:
            return FLinearColor(0.3f, 1.0f, 0.4f);
        case ELaunchMonitorConnectionState::Disconnected:
            return FLinearColor(0.65f, 0.65f, 0.65f);
        default:
            return FLinearColor(1.0f, 0.85f, 0.3f);
        }
    }
} // namespace

void AUOpenGolfHUD::DrawHUD()
{
    Super::DrawHUD();

    if (Canvas && GEngine)
    {
        DrawLaunchMonitorPanel();
        DrawShotPanel();
    }
}

void AUOpenGolfHUD::DrawShotPanel()
{
    const UWorld* World = GetWorld();
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Ball)
    {
        return;
    }

    const FLinearColor Label(0.85f, 0.85f, 0.85f);
    const FLinearColor Heading(0.55f, 0.8f, 1.0f);
    const FLinearColor Value = FLinearColor::White;
    TArray<FLine> Lines;
    FOpenFairwayShotResult Shot;
    if (!Ball->GetShotResult(Shot))
    {
        Lines.Add({TEXT("SHOT"), Heading});
        Lines.Add({TEXT("Waiting for a shot: hit one or press [R]"), Label});
        DrawPanel(Lines, -20.0f, 20.0f);
        return;
    }

    const auto SurfaceName = [](EOpenFairwaySurface Surface)
    { return StaticEnum<EOpenFairwaySurface>()->GetDisplayNameTextByValue(static_cast<int64>(Surface)).ToString(); };
    const TCHAR* Status = Shot.bAtRest ? TEXT("at rest") : Shot.bLanded ? TEXT("rolling") : TEXT("in flight");
    Lines.Add({FString::Printf(TEXT("SHOT #%d  (%s)"), Shot.ShotNumber, Status), Heading});
    Lines.Add({FString::Printf(TEXT("Carry:    %s"), Shot.bLanded || Shot.bAtRest ? *FString::Printf(TEXT("%.1f yd"), Shot.CarryM * OpenFairway::YardsPerMeter) : Dash),
        Value});
    Lines.Add({FString::Printf(TEXT("Roll:     %s"), Shot.bLanded ? *FString::Printf(TEXT("%.1f yd"), Shot.RollM * OpenFairway::YardsPerMeter) : Dash), Value});
    Lines.Add({FString::Printf(TEXT("Total:    %.1f yd%s"), Shot.TotalM * OpenFairway::YardsPerMeter, Shot.bAtRest ? TEXT("") : TEXT(" ...")), Value});
    Lines.Add({FString::Printf(TEXT("Offline:  %.1f yd %s"), FMath::Abs(Shot.LateralM) * OpenFairway::YardsPerMeter, Shot.LateralM >= 0.0f ? TEXT("right") : TEXT("left")),
        Value});
    Lines.Add({FString::Printf(TEXT("Apex:     %.1f ft"), Shot.ApexM * OpenFairway::FeetPerMeter), Value});
    Lines.Add({FString::Printf(TEXT("Hang time: %s"), Shot.bLanded ? *FString::Printf(TEXT("%.2f s"), Shot.HangTimeS) : Dash), Value});
    Lines.Add({FString::Printf(TEXT("Landing:  %s"),
                   Shot.bLanded ? *FString::Printf(TEXT("%.1f deg at %.1f mph on %s"), Shot.LandingAngleDeg, Shot.LandingSpeedMps / OpenFairway::MpsPerMph, *SurfaceName(Shot.LandingSurface))
                                : Dash),
        Value});
    Lines.Add({FString::Printf(TEXT("Lie:      %s"), *SurfaceName(Shot.Surface)), Label});
    Lines.Add({FString::Printf(TEXT("Response: %s"), *Shot.LandingBucket), Label});
    Lines.Add({FString(), Label});
    Lines.Add({TEXT("[T] Tee up again"), FLinearColor(0.6f, 0.6f, 0.6f)});
    DrawPanel(Lines, -20.0f, 20.0f);
}

void AUOpenGolfHUD::DrawPanel(const TArray<FLine>& Lines, float Left, float Top)
{
    UFont* Font = GEngine->GetMediumFont();
    const float Scale = 1.0f;
    const float Padding = 12.0f;
    const float LineHeight = Font->GetMaxCharHeight() * Scale + 4.0f;
    float PanelWidth = 0.0f;
    for (const FLine& Line : Lines)
    {
        float Width = 0.0f;
        float Height = 0.0f;
        GetTextSize(Line.Text, Width, Height, Font, Scale);
        PanelWidth = FMath::Max(PanelWidth, Width);
    }

    const float Width = PanelWidth + Padding * 2.0f;
    const float X = Left < 0.0f ? Canvas->SizeX - Width + Left : Left;
    DrawRect(FLinearColor(0.0f, 0.0f, 0.0f, 0.6f), X, Top, Width, Lines.Num() * LineHeight + Padding * 2.0f);
    float Y = Top + Padding;
    for (const FLine& Line : Lines)
    {
        DrawText(Line.Text, Line.Color, X + Padding, Y, Font, Scale);
        Y += LineHeight;
    }
}

void AUOpenGolfHUD::DrawLaunchMonitorPanel()
{
    const UGameInstance* GameInstance = GetGameInstance();
    const UOpenLaunchMonitorSubsystem* LaunchMonitor = GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    if (!LaunchMonitor)
    {
        return;
    }

    TArray<FLine> Lines;
    const FLinearColor Label(0.85f, 0.85f, 0.85f);
    const FLinearColor Heading(0.55f, 0.8f, 1.0f);

    const ELaunchMonitorConnectionState State = LaunchMonitor->GetConnectionState();
    Lines.Add({TEXT("LAUNCH MONITOR  (Square Home)"), Heading});
    Lines.Add({FString::Printf(TEXT("State: %s"), *StaticEnum<ELaunchMonitorConnectionState>()->GetDisplayNameTextByValue(static_cast<int64>(State)).ToString()),
        StateColor(State)});
    if (State != ELaunchMonitorConnectionState::Disconnected || !LaunchMonitor->GetDeviceName().IsEmpty())
    {
        const int32 Battery = LaunchMonitor->GetBatteryPercent();
        Lines.Add({FString::Printf(TEXT("Device: %s   Battery: %s   Firmware: %s"),
                       LaunchMonitor->GetDeviceName().IsEmpty() ? Dash : *LaunchMonitor->GetDeviceName(),
                       Battery < 0 ? Dash : *FString::Printf(TEXT("%d%%"), Battery),
                       LaunchMonitor->GetFirmware().IsEmpty() ? Dash : *LaunchMonitor->GetFirmware()),
            Label});
    }
    if (State == ELaunchMonitorConnectionState::Ready)
    {
        Lines.Add({LaunchMonitor->IsBallReady() ? TEXT("Ball: ready - take your shot") : TEXT("Ball: place a ball"),
            LaunchMonitor->IsBallReady() ? FLinearColor(0.3f, 1.0f, 0.4f) : Label});
    }
    Lines.Add({FString::Printf(TEXT("Club: %s (%s)"), *UOpenLaunchMonitorSubsystem::GetClubDisplayName(LaunchMonitor->GetClub()).ToString(),
                   LaunchMonitor->IsLeftHanded() ? TEXT("left-handed") : TEXT("right-handed")),
        Label});

    FLaunchMonitorShot Shot;
    if (LaunchMonitor->GetLastShot(Shot))
    {
        Lines.Add({FString(), Label});
        const TCHAR* ShotType = Shot.ShotType == ELaunchMonitorShotType::Putt ? TEXT("putt") : Shot.ShotType == ELaunchMonitorShotType::Full ? TEXT("full swing")
                                                                                                                                             : TEXT("shot");
        Lines.Add({FString::Printf(TEXT("Shot #%d  %s  (%s, %s)"), Shot.ShotNumber, ShotType, *Shot.Source,
                       *(Shot.ReceivedUtc + (FDateTime::Now() - FDateTime::UtcNow())).ToString(TEXT("%H:%M:%S"))),
            Heading});
        Lines.Add({FString::Printf(TEXT("Ball speed: %s  (%s)"),
                       *FormatValue(Shot.BallSpeedSource, FString::Printf(TEXT("%.1f"), Shot.GetBallSpeedMph()), TEXT(" mph")),
                       *FormatValue(Shot.BallSpeedSource, FString::Printf(TEXT("%.2f"), Shot.BallSpeedMps), TEXT(" m/s"))),
            FLinearColor::White});
        Lines.Add({FString::Printf(TEXT("Launch: %s up, %s right"),
                       *FormatValue(Shot.VerticalLaunchSource, FString::Printf(TEXT("%.2f"), Shot.VerticalLaunchDeg), TEXT("°")),
                       *FormatValue(Shot.HorizontalLaunchSource, FString::Printf(TEXT("%.2f"), Shot.HorizontalLaunchDeg), TEXT("°"))),
            FLinearColor::White});
        Lines.Add({FString::Printf(TEXT("Spin: %s, axis %s"),
                       *FormatValue(Shot.TotalSpinSource, FString::FromInt(Shot.TotalSpinRpm), TEXT(" rpm")),
                       *FormatValue(Shot.SpinAxisSource, FString::Printf(TEXT("%.2f"), Shot.SpinAxisDeg), TEXT("°"))),
            FLinearColor::White});
        Lines.Add({FString::Printf(TEXT("Back / side spin: %s / %s"),
                       *FormatValue(Shot.BackSpinSource, FString::FromInt(Shot.BackSpinRpm), TEXT(" rpm")),
                       *FormatValue(Shot.SideSpinSource, FString::FromInt(Shot.SideSpinRpm), TEXT(" rpm"))),
            FLinearColor::White});

        FLaunchMonitorClubData Club;
        if (!LaunchMonitor->GetLastClubData(Club))
        {
            Lines.Add({TEXT("Club data: waiting"), Label});
        }
        else if (!Club.HasReading())
        {
            Lines.Add({TEXT("Club data: not tracked"), Label});
        }
        else
        {
            Lines.Add({FString::Printf(TEXT("Club: path %s, face %s, attack %s, loft %s"), *FormatOptional(Club.bHasPath, Club.PathDeg, TEXT("°")),
                           *FormatOptional(Club.bHasFaceToTarget, Club.FaceToTargetDeg, TEXT("°")),
                           *FormatOptional(Club.bHasAngleOfAttack, Club.AngleOfAttackDeg, TEXT("°")),
                           *FormatOptional(Club.bHasDynamicLoft, Club.DynamicLoftDeg, TEXT("°"))),
                FLinearColor::White});
        }
    }

    if (!LaunchMonitor->GetLastError().IsEmpty())
    {
        Lines.Add({FString(), Label});
        Lines.Add({LaunchMonitor->GetLastError(), FLinearColor(1.0f, 0.45f, 0.4f)});
    }
    Lines.Add({FString(), Label});
    Lines.Add({TEXT("[C] Connect   [X] Disconnect   [R] Replay shot   [Tab] Next club"), FLinearColor(0.6f, 0.6f, 0.6f)});
    DrawPanel(Lines, 20.0f, 20.0f);
}
