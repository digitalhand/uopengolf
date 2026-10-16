// SPDX-License-Identifier: MIT
#include "UOpenGolfPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"
#include "ReplayShotPanel.h"
#include "Framework/Application/SlateApplication.h"

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
    // Low behind the ball, in flight and at rest.
    const FVector FollowOffset(-700.0f, 0.0f, 135.0f);
    // Degrees the follow view is tilted up from the ball, so the ball sits below the frame centre
    // with the horizon in view.
    constexpr float FollowFramingPitch = 8.0f;
    constexpr float FollowSpeed = 2.5f;
    // The camera holds the tee view this long after the ball leaves the tee.
    constexpr float FollowDelaySeconds = 0.5f;
    constexpr float TeeReturnSeconds = 2.0f;
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
    if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
    {
        ReplayPanel = SNew(SReplayShotPanel).Controller(this);
        Viewport->AddViewportWidgetContent(ReplayPanel.ToSharedRef(), 10);
        bShowMouseCursor = true;
        FInputModeGameAndUI InputMode;
        InputMode.SetHideCursorDuringCapture(false);
        InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(InputMode);
    }
}

void AUOpenGolfPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ReplayPanel.IsValid())
    {
        if (UGameViewportClient* Viewport = GetWorld()->GetGameViewport())
        {
            Viewport->RemoveViewportWidgetContent(ReplayPanel.ToSharedRef());
        }
        ReplayPanel.Reset();
    }
    Super::EndPlay(EndPlayReason);
}

void AUOpenGolfPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &ThisClass::ConnectLaunchMonitor);
    InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ThisClass::DisconnectLaunchMonitor);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::InjectReplayShot);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ThisClass::ToggleReplayPanel);
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::SelectNextClub);
    InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ThisClass::ResetBallToTee);
    InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ThisClass::ToggleRecordShots);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::QuitGame);
}

void AUOpenGolfPlayerController::QuitGame()
{
    // Closes a standalone or packaged game; in the editor it ends Play.
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
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
    const bool bAtTee = !Ball->IsMoving() && !bAwayFromTee;
    if (bAtTee)
    {
        const FVector Desired = BallLocation + TeeCameraOffset;
        // Aim from the final camera spot, never from the camera's path, so the view cannot flip
        // while the camera passes over the aim point on its way back.
        const FQuat TeeRotation = (BallLocation + TeeCameraTarget - Desired).Rotation().Quaternion();
        if (bSnap)
        {
            bReturningToTee = false;
        }
        else if (!bCameraAtTee)
        {
            bReturningToTee = true;
            ReturnElapsed = 0.0f;
            ReturnStartLocation = ShotCamera->GetActorLocation();
            ReturnStartRotation = ShotCamera->GetActorQuat();
        }
        bCameraAtTee = true;

        if (!bReturningToTee)
        {
            ShotCamera->SetActorLocationAndRotation(bSnap ? Desired : FMath::VInterpTo(ShotCamera->GetActorLocation(), Desired, DeltaTime, FollowSpeed), TeeRotation);
            return;
        }
        ReturnElapsed += DeltaTime;
        const float Alpha = FMath::SmoothStep(0.0f, 1.0f, ReturnElapsed / TeeReturnSeconds);
        bReturningToTee = Alpha < 1.0f;
        ShotCamera->SetActorLocationAndRotation(FMath::Lerp(ReturnStartLocation, Desired, Alpha), FQuat::Slerp(ReturnStartRotation, TeeRotation, Alpha));
        return;
    }

    if (bCameraAtTee)
    {
        FollowDelayElapsed = 0.0f;
    }
    bCameraAtTee = false;
    bReturningToTee = false;
    if (!bSnap && FollowDelayElapsed < FollowDelaySeconds)
    {
        FollowDelayElapsed += DeltaTime;
        return;
    }
    const FVector Desired = BallLocation + FollowOffset;
    const FVector Location = bSnap ? Desired : FMath::VInterpTo(ShotCamera->GetActorLocation(), Desired, DeltaTime, FollowSpeed);
    // Aim at the ball so it stays in frame while the camera catches up, then tilt up to frame it low.
    FRotator DesiredRotation = (BallLocation - Location).Rotation();
    DesiredRotation.Pitch += FollowFramingPitch;
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
        LaunchMonitor->InjectReplayShotAt(SelectedReplayIndex);
        SetReplayPanelOpen(false);
    }
}

bool AUOpenGolfPlayerController::SelectReplayShot(int32 Index)
{
    if (!UOpenLaunchMonitorSubsystem::GetReplayShotNames().IsValidIndex(Index))
    {
        return false;
    }
    SelectedReplayIndex = Index;
    return true;
}

void AUOpenGolfPlayerController::ToggleReplayPanel()
{
    SetReplayPanelOpen(!bReplayPanelOpen);
}

void AUOpenGolfPlayerController::SetReplayPanelOpen(bool bOpen)
{
    bReplayPanelOpen = bOpen;
    if (!bOpen && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().SetAllUserFocusToGameViewport();
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
        // Only the player's tee-up clears the history tracers; the automatic re-tee keeps them.
        if (auto* PresentedBall = Cast<AUOpenGolfBall>(Mode->GetBall()))
            PresentedBall->ClearShotTrailHistory();
        Mode->ResetBallToTee();
    }
}

void AUOpenGolfPlayerController::ToggleRecordShots()
{
    if (AUOpenGolfGameMode* Mode = GetRangeMode(*this))
    {
        Mode->SetRecordShots(!Mode->IsRecordingShots());
    }
}
