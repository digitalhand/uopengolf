// SPDX-License-Identifier: MIT
#include "UOpenGolfPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"

namespace
{
    UOpenLaunchMonitorSubsystem* GetLaunchMonitor(const APlayerController& Controller)
    {
        const UGameInstance* GameInstance = Controller.GetGameInstance();
        return GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    }

    AUOpenGolfGameMode* GetRangeMode(const APlayerController& Controller)
    {
        const UWorld* World = Controller.GetWorld();
        return World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    }

    // Camera placement in centimeters.
    const FVector TeeCameraOffset(-600.0f, 0.0f, 220.0f);
    const FVector TeeCameraTarget(6000.0f, 0.0f, 0.0f);
    const FVector FollowOffset(-900.0f, 0.0f, 300.0f);
    const FVector RestOffset(-700.0f, 0.0f, 250.0f);
    constexpr float FollowSpeed = 2.5f;
} // namespace

void AUOpenGolfPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!IsLocalController())
    {
        return;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    ShotCamera = GetWorld()->SpawnActor<ACameraActor>(Params);
    // There is no pawn; the shot camera stays the view target.
    bAutoManageActiveCameraTarget = false;
    if (ShotCamera)
    {
        ShotCamera->GetCameraComponent()->SetFieldOfView(70.0f);
        ShotCamera->GetCameraComponent()->bConstrainAspectRatio = false;
        UpdateShotCamera(0.0f, true);
        SetViewTarget(ShotCamera);
    }
}

void AUOpenGolfPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ThisClass::ConnectLaunchMonitor);
    InputComponent->BindKey(EKeys::X, IE_Pressed, this, &ThisClass::DisconnectLaunchMonitor);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::InjectReplayShot);
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::SelectNextClub);
    InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ThisClass::ResetBallToTee);
}

void AUOpenGolfPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    UpdateShotCamera(DeltaTime, false);
}

void AUOpenGolfPlayerController::UpdateShotCamera(float DeltaTime, bool bSnap)
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!ShotCamera || !Ball)
    {
        return;
    }

    const FVector BallLocation = Ball->GetActorLocation();
    const bool bAwayFromTee = Mode->GetRange() && FVector::Dist2D(BallLocation, Mode->GetRange()->GetTeeLocation()) > 200.0f;
    FVector Desired;
    FVector Target;
    if (Ball->IsMoving())
    {
        Desired = BallLocation + FollowOffset;
        Target = BallLocation;
    }
    else if (bAwayFromTee)
    {
        Desired = BallLocation + RestOffset;
        Target = BallLocation;
    }
    else
    {
        Desired = BallLocation + TeeCameraOffset;
        Target = BallLocation + TeeCameraTarget;
    }

    const FVector Location = bSnap ? Desired : FMath::VInterpTo(ShotCamera->GetActorLocation(), Desired, DeltaTime, FollowSpeed);
    const FRotator DesiredRotation = (Target - Location).Rotation();
    const FRotator Rotation = bSnap ? DesiredRotation : FMath::RInterpTo(ShotCamera->GetActorRotation(), DesiredRotation, DeltaTime, FollowSpeed * 2.0f);
    ShotCamera->SetActorLocationAndRotation(Location, Rotation);
}

void AUOpenGolfPlayerController::ConnectLaunchMonitor()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        LaunchMonitor->Connect();
    }
}

void AUOpenGolfPlayerController::DisconnectLaunchMonitor()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        LaunchMonitor->Disconnect();
    }
}

void AUOpenGolfPlayerController::InjectReplayShot()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        LaunchMonitor->InjectReplayShot();
    }
}

void AUOpenGolfPlayerController::SelectNextClub()
{
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        const int32 ClubCount = static_cast<int32>(ELaunchMonitorClub::Putter) + 1;
        LaunchMonitor->SetClub(static_cast<ELaunchMonitorClub>((static_cast<int32>(LaunchMonitor->GetClub()) + 1) % ClubCount));
    }
}

void AUOpenGolfPlayerController::ResetBallToTee()
{
    if (AUOpenGolfGameMode* Mode = GetRangeMode(*this))
    {
        Mode->ResetBallToTee();
    }
}
