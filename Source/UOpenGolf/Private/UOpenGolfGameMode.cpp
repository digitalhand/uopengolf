// SPDX-License-Identifier: MIT
#include "UOpenGolfGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "OpenFairwayBall.h"
#include "Hole/OpenGolfCupComponent.h"
#include "UOpenGolfBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "ShotRecording.h"
#include "TimerManager.h"
#include "UOpenGolfHUD.h"
#include "UOpenGolfPlayerController.h"
#include "OpenGolfFlagstickComponent.h"
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
    for (TActorIterator<AUOpenGolfRange> It(World); It; ++It)
    {
        if (Range)
        {
            // Play must still start (every actor's BeginPlay depends on it), so use the first one.
            UE_LOG(LogUOpenGolfGameMode, Error, TEXT("The map contains multiple range controllers; using %s and ignoring %s. Keep exactly one."),
                *Range->GetName(), *It->GetName());
            continue;
        }
        Range = *It;
    }
    if (!Range)
        Range = World->SpawnActor<AUOpenGolfRange>(SpawnClass, FVector::ZeroVector, FRotator::ZeroRotator, Params);
    if (ensure(Range))
    {
        Range->BuildDefaultLayout();
        Ball = World->SpawnActor<AUOpenGolfBall>(Range->GetTeeLocation() + FVector::UpVector * 10.0f, FRotator::ZeroRotator, Params);
    }
    if (Ball)
    {
        Ball->OnBallAtRest.AddDynamic(this, &ThisClass::HandleBallAtRest);
        Ball->PoleRadiusCm = UOpenGolfFlagstickComponent::PoleDiameterCm * 0.5f;
    }
    Round.Start(AUOpenGolfRange::Par);

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
    if (Ball && Range && Range->GetCup())
        Ball->SetCups({Range->GetCup()->GetCupDescriptor()});
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
    Round.Start(AUOpenGolfRange::Par);
    if (Recorder)
        Recorder->CancelSimulation();
    PlaceOnTee();
    if (PlayMode == EOpenGolfPlayMode::Hole)
        Round.Start(AUOpenGolfRange::Par);
}

void AUOpenGolfGameMode::ResetBall()
{
    if (!Round.bPractice)
    {
        ResetBallToTee();
        return;
    }
    if (Recorder) Recorder->CancelSimulation();
    GetWorldTimerManager().ClearTimer(ResetTimer);
    GetWorldTimerManager().ClearTimer(AddressTimer);
    HoledTime = -1.0;
    if (Ball)
    {
        if (auto* Presented = Cast<AUOpenGolfBall>(Ball)) Presented->ResetShotTrail();
        // Keep the player's aim (A/D): placing aimed the ball at the cup once, and PlaceAt faces it down the aim line.
        Ball->PlaceAt(PracticeOrigin);
    }
    bAddressReady = true;
}

void AUOpenGolfGameMode::PlaceOnTee()
{
    GetWorldTimerManager().ClearTimer(ResetTimer);
    GetWorldTimerManager().ClearTimer(AddressTimer);
    HoledTime = -1.0;
    bAddressReady = true;
    if (Ball && Range)
    {
        if (auto* PresentedBall = Cast<AUOpenGolfBall>(Ball))
            PresentedBall->ResetShotTrail();
        // A hole starts down the target line; on the range the aim stays set across shots.
        if (PlayMode == EOpenGolfPlayMode::Hole)
            Ball->AimYawDeg = 0.0f;
        Ball->PlaceAt(Range->GetTeeLocation() + FVector::UpVector * 10.0f);
    }
}

bool AUOpenGolfGameMode::SetPlayMode(EOpenGolfPlayMode Mode)
{
    if (Ball && Ball->IsMoving())
        return false;
    PlayMode = Mode;
    UE_LOG(LogUOpenGolfGameMode, Log, TEXT("Play mode: %s."), Mode == EOpenGolfPlayMode::Hole ? TEXT("hole") : TEXT("range"));
    ResetBallToTee();
    return true;
}

bool AUOpenGolfGameMode::TogglePlayMode()
{
    return SetPlayMode(PlayMode == EOpenGolfPlayMode::Range ? EOpenGolfPlayMode::Hole : EOpenGolfPlayMode::Range);
}

bool AUOpenGolfGameMode::PlaceBallAt(const FVector& Location)
{
    if (PlayMode != EOpenGolfPlayMode::Hole || !Ball || Ball->IsMoving() || !OpenGolfPlay::CanPlaceBallAt(Location, GetCupLocation()))
        return false;
    GetWorldTimerManager().ClearTimer(ResetTimer);
    GetWorldTimerManager().ClearTimer(AddressTimer);
    if (Recorder)
        Recorder->CancelSimulation();
    // A finished hole starts over; either way the hole is now practice.
    Round.Start(AUOpenGolfRange::Par);
    Round.bPractice = true;
    HoledTime = -1.0;
    if (auto* PresentedBall = Cast<AUOpenGolfBall>(Ball))
        PresentedBall->ResetShotTrail();
    Ball->PlaceAt(Location + FVector::UpVector * 10.0f);
    PracticeOrigin = Ball->GetActorLocation();
    AimAtCup();
    bAddressReady = true;
    return true;
}

FVector AUOpenGolfGameMode::GetCupLocation() const
{
    return Range ? Range->GetCupLocation() : FVector::ZeroVector;
}

float AUOpenGolfGameMode::GetDistanceToCupM() const
{
    return Ball ? static_cast<float>(FVector::Dist2D(Ball->GetActorLocation(), GetCupLocation())) / 100.0f : 0.0f;
}

bool AUOpenGolfGameMode::IsBallAtAddress() const
{
    if (!Ball || Ball->IsMoving())
        return false;
    if (PlayMode == EOpenGolfPlayMode::Hole)
        return bAddressReady && !Ball->IsHoled();
    return !Range || FVector::Dist2D(Ball->GetActorLocation(), Range->GetTeeLocation()) <= 200.0f;
}

float AUOpenGolfGameMode::GetHoledSeconds() const
{
    return HoledTime >= 0.0 && GetWorld() ? static_cast<float>(GetWorld()->GetTimeSeconds() - HoledTime) : -1.0f;
}

void AUOpenGolfGameMode::AimAtCup()
{
    if (!Ball)
        return;
    Ball->AimYawDeg = OpenGolfPlay::BearingDeg(Ball->GetActorLocation(), GetCupLocation(), Ball->AimYawDeg);
    Ball->SetActorRotation(FRotator(0.0f, Ball->AimYawDeg, 0.0f));
}

void AUOpenGolfGameMode::ReadyForNextShot()
{
    if (!Ball || Ball->IsMoving() || PlayMode != EOpenGolfPlayMode::Hole)
        return;
    AimAtCup();
    bAddressReady = true;
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
    if (Round.bPractice)
    {
        if (Result.bHoled) HoledTime = GetWorld()->GetTimeSeconds();
        GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetAfterShot, ResetDelayS, false);
        return;
    }
    if (PlayMode == EOpenGolfPlayMode::Hole)
    {
        Round.RecordStroke(bShotFromGreen, Result.bHoled);
        if (Round.bHoled)
        {
            HoledTime = GetWorld()->GetTimeSeconds();
            UE_LOG(LogUOpenGolfGameMode, Log, TEXT("Holed in %d (%d putts): %s."), Round.Strokes, Round.Putts, *Round.ResultName());
            GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetAfterShot, HoleOutDelayS, false);
        }
        else
        {
            // Let the shot be seen where it stopped, then turn to the cup for the next one.
            GetWorldTimerManager().SetTimer(AddressTimer, this, &ThisClass::ReadyForNextShot, AddressDelayS, false);
        }
        return;
    }
    // Leave the ball where it stopped so the shot can be seen, then tee up for the next one.
    GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetAfterShot, ResetDelayS, false);
}

void AUOpenGolfGameMode::ResetAfterShot()
{
    // A ball launched directly (not through HandleShot) during the wait finishes its shot.
    if (Ball && !Ball->IsMoving())
    {
        ResetBall();
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
    FOpenFairwayLaunch CheckedLaunch;
    if (!Ball || !ToLaunch(Shot, CheckedLaunch)) return;
    if (Round.bPractice) ResetBall();
    else if (PlayMode == EOpenGolfPlayMode::Range || Round.bHoled)
        ResetBallToTee();
    else if (Ball && Ball->IsMoving())
    {
        // A shot arriving mid-roll is hit from where the ball is now; the interrupted shot still counts.
        Round.RecordStroke(bShotFromGreen, false);
        Ball->PlaceAt(Ball->GetActorLocation());
    }
    else if (!bAddressReady)
        ReadyForNextShot();
    GetWorldTimerManager().ClearTimer(AddressTimer);
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
    // Range mode hits from the tee; Hole mode from wherever the ball lies.
    if (Ball->Launch(Launch))
    {
        bAddressReady = false;
        FOpenFairwayShotResult Launched;
        bShotFromGreen = Ball->GetShotResult(Launched) && Launched.Surface == EOpenFairwaySurface::Green;
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
