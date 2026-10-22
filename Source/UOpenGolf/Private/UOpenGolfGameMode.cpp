// SPDX-License-Identifier: MIT
#include "UOpenGolfGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "OpenFairwayBall.h"
#include "UOpenGolfBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "ShotRecording.h"
#include "TimerManager.h"
#include "UOpenGolfHUD.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"

DEFINE_LOG_CATEGORY_STATIC(LogUOpenGolfGameMode, Log, All);

AUOpenGolfGameMode::AUOpenGolfGameMode()
{
    HUDClass = AUOpenGolfHUD::StaticClass();
    PlayerControllerClass = AUOpenGolfPlayerController::StaticClass();
    DefaultPawnClass = nullptr;
    RangeClass = AUOpenGolfRange::StaticClass();
    Recorder = MakeShared<ShotRecording::FShotRecorder>();
}

void AUOpenGolfGameMode::StartPlay()
{
    UWorld* World = GetWorld();
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    UClass* SpawnClass = RangeClass ? RangeClass.Get() : AUOpenGolfRange::StaticClass();
    if (SpawnClass == AUOpenGolfRange::StaticClass() && LocalRangeClass.IsValid() && !GIsAutomationTesting)
    {
        if (UClass* LocalClass = LocalRangeClass.TryLoadClass<AUOpenGolfRange>())
        {
            SpawnClass = LocalClass;
            UE_LOG(LogUOpenGolfGameMode, Log, TEXT("Using local range %s"), *LocalRangeClass.ToString());
        }
        else
        {
            UE_LOG(LogUOpenGolfGameMode, Warning, TEXT("Local range %s did not load; using the public range"), *LocalRangeClass.ToString());
        }
    }
    for (TActorIterator<AUOpenGolfRange> It(World); It; ++It)
    {
        if (Range)
        {
            UE_LOG(LogUOpenGolfGameMode, Error, TEXT("The map contains multiple range controllers. Keep exactly one."));
            return;
        }
        Range = *It;
    }
    if (!Range)
        Range = World->SpawnActor<AUOpenGolfRange>(SpawnClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    else if (SpawnClass != AUOpenGolfRange::StaticClass() && Range->GetClass() == AUOpenGolfRange::StaticClass())
        Range->ApplyPresentationDefaults(*CastChecked<AUOpenGolfRange>(SpawnClass->GetDefaultObject()));
    if (Range)
        Range->BuildDefaultLayout();
    if (!ensure(Range))
        return;
    Ball = World->SpawnActor<AUOpenGolfBall>(Range->GetTeeLocation() + FVector::UpVector * 10.0f, FRotator::ZeroRotator, Params);
    if (Ball)
    {
        Ball->OnBallAtRest.AddDynamic(this, &ThisClass::HandleBallAtRest);
    }

    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>())
    {
        ShotHandle = LaunchMonitor->OnShotReceivedNative.AddUObject(this, &ThisClass::HandleShot);
        ClubDataHandle = LaunchMonitor->OnClubDataReceivedNative.AddUObject(this, &ThisClass::HandleClubData);
    }

    GetWorldTimerManager().SetTimer(RecordingTimer, FTimerDelegate::CreateWeakLambda(this, [this]
                                                        {
        if (Recorder) Recorder->Tick(); }),
        0.1f, true);

    // Spawn the range before players start so the controller can find the ball.
    Super::StartPlay();
}

void AUOpenGolfGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>())
        {
            LaunchMonitor->OnShotReceivedNative.Remove(ShotHandle);
            LaunchMonitor->OnClubDataReceivedNative.Remove(ClubDataHandle);
        }
    }
    if (Recorder)
    {
        Recorder->Shutdown();
    }
    GetWorldTimerManager().ClearTimer(RecordingTimer);
    Super::EndPlay(EndPlayReason);
}

void AUOpenGolfGameMode::ResetBallToTee()
{
    GetWorldTimerManager().ClearTimer(ResetTimer);
    if (Recorder)
        Recorder->CancelSimulation();
    if (Ball && Range)
    {
        if (auto* PresentedBall = Cast<AUOpenGolfBall>(Ball))
            PresentedBall->ResetShotTrail();
        Ball->PlaceAt(Range->GetTeeLocation() + FVector::UpVector * 10.0f);
    }
}

void AUOpenGolfGameMode::SetRecordShots(bool bRecord)
{
    if (Recorder)
    {
        Recorder->SetEnabled(bRecord);
    }
}

bool AUOpenGolfGameMode::IsRecordingShots() const
{
    return Recorder && Recorder->IsEnabled();
}

int32 AUOpenGolfGameMode::GetRecordedShotCount() const
{
    return Recorder ? Recorder->GetShotCount() : 0;
}

int32 AUOpenGolfGameMode::GetFailedRecordingCount() const
{
    return Recorder ? Recorder->GetFailedSaveCount() : 0;
}

FString AUOpenGolfGameMode::GetRecordingError() const
{
    return Recorder ? Recorder->GetLastError() : FString();
}

FString AUOpenGolfGameMode::GetRecordingFolder() const
{
    return Recorder ? Recorder->GetSessionDir() : FString();
}

void AUOpenGolfGameMode::HandleBallAtRest(const FOpenFairwayShotResult& Result)
{
    if (Recorder)
    {
        Recorder->OnBallAtRest(Result);
    }
    // Leave the ball where it stopped so the shot can be seen, then tee up for the next one.
    GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetAfterShot, ResetDelayS, false);
}

void AUOpenGolfGameMode::ResetAfterShot()
{
    // A ball launched directly (not through HandleShot) during the wait finishes its shot.
    if (Ball && !Ball->IsMoving())
    {
        ResetBallToTee();
    }
}

bool AUOpenGolfGameMode::ToLaunch(const FLaunchMonitorShot& Shot, FOpenFairwayLaunch& OutLaunch)
{
    if (Shot.BallSpeedSource == ELaunchMonitorValueSource::Missing || Shot.VerticalLaunchSource == ELaunchMonitorValueSource::Missing)
    {
        return false;
    }

    OutLaunch = FOpenFairwayLaunch();
    OutLaunch.BallSpeedMph = Shot.GetBallSpeedMph();
    OutLaunch.VerticalLaunchDeg = Shot.VerticalLaunchDeg;
    // An unmeasured direction is straight; unmeasured spin is derived or left out, never invented.
    OutLaunch.HorizontalLaunchDeg = Shot.HorizontalLaunchSource == ELaunchMonitorValueSource::Missing ? 0.0f : Shot.HorizontalLaunchDeg;
    OutLaunch.bHasBackSpin = Shot.BackSpinSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.BackSpinRpm = Shot.BackSpinRpm;
    OutLaunch.bHasSideSpin = Shot.SideSpinSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.SideSpinRpm = Shot.SideSpinRpm;
    OutLaunch.bHasTotalSpin = Shot.TotalSpinSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.TotalSpinRpm = Shot.TotalSpinRpm;
    OutLaunch.bHasSpinAxis = Shot.SpinAxisSource != ELaunchMonitorValueSource::Missing;
    OutLaunch.SpinAxisDeg = Shot.SpinAxisDeg;
    return true;
}

void AUOpenGolfGameMode::HandleShot(const FLaunchMonitorShot& Shot)
{
    ResetBallToTee();
    const UOpenLaunchMonitorSubsystem* LaunchMonitor = GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>();
    if (Recorder && LaunchMonitor)
    {
        // The club selected now is the club this shot was hit with.
        Recorder->OnShot(Shot, LaunchMonitor->GetConnectionState(), LaunchMonitor->GetClub(), LaunchMonitor->IsLeftHanded(), LaunchMonitor->GetFirmware());
    }

    FOpenFairwayLaunch Launch;
    if (!Ball || !ToLaunch(Shot, Launch))
    {
        if (Recorder)
            Recorder->CancelSimulation();
        return;
    }
    // Range mode: every shot is hit from the tee.
    if (Ball->Launch(Launch))
    {
        FOpenFairwayShotResult Result;
        if (Recorder && Ball->GetShotResult(Result))
            Recorder->BindSimulation(Shot.ShotNumber, Result.ShotNumber);
        if (auto* PresentedBall = Cast<AUOpenGolfBall>(Ball))
            PresentedBall->StartShotTrail();
    }
    else if (Recorder)
        Recorder->CancelSimulation();
}

void AUOpenGolfGameMode::HandleClubData(const FLaunchMonitorClubData& ClubData)
{
    if (Recorder)
    {
        Recorder->OnClubData(ClubData);
    }
}
