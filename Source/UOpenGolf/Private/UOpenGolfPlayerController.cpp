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
#include "Hole/OpenGolfCupComponent.h"
#include "OpenGolfTerrain.h"
#include "Play/OpenGolfPlayRules.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"
#include "UI/HudLayers.h"
#include "UI/ReplayShotPanel.h"
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
    // Respond promptly near the cup, where a firm putt can cross the zoom region in under a second.
    const FVector CupZoomOffset(-250.0f, 0.0f, 80.0f);
    constexpr float CupZoomInSpeed = 6.0f;
    constexpr float CupZoomOutSpeed = 2.0f;
    constexpr float CupCameraFollowSpeed = 10.0f;
    // The camera holds the tee view this long after the ball leaves the tee.
    constexpr float FollowDelaySeconds = 0.5f;
    constexpr float TeeReturnSeconds = 2.0f;
    // Short game: low and close behind the ball, looking along the putt at the cup's height.
    const FVector ShortGameCameraOffset(-280.0f, 0.0f, 95.0f);
    constexpr float ShortGameMinLookCm = 300.0f;
    // Clicks reach this far to place the ball.
    constexpr float PlaceReachCm = 100000.0f;

    // At rest and waiting for the next shot.
    bool IsBallOnTee(const AUOpenGolfGameMode& Mode, const AOpenFairwayBall&)
    {
        return Mode.IsBallAtAddress();
    }

    // The aim turns around the target line on the range, and around the line to the cup on the hole.
    float AimCenterDeg(const AUOpenGolfGameMode& Mode, const AOpenFairwayBall& Ball)
    {
        return Mode.GetPlayMode() == EOpenGolfPlayMode::Hole ? OpenGolfPlay::BearingDeg(Ball.GetActorLocation(), Mode.GetCupLocation(), Ball.AimYawDeg) : 0.0f;
    }
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
        Viewport->AddViewportWidgetContent(ReplayPanel.ToSharedRef(), HudLayers::TestShotPanel);
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
    InputComponent->BindKey(EKeys::T, IE_Pressed, this, &ThisClass::DisconnectLaunchMonitor);
    InputComponent->BindKey(EKeys::H, IE_Pressed, this, &ThisClass::InjectReplayShot);
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &ThisClass::ToggleReplayPanel);
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &ThisClass::SelectNextClub);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ThisClass::ResetBallToTee);
    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ThisClass::TogglePlayMode);
    InputComponent->BindKey(EKeys::B, IE_Pressed, this, &ThisClass::TogglePlacingBall);
    InputComponent->BindKey(EKeys::W, IE_Pressed, this, &ThisClass::ToggleWindPanel);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &ThisClass::HandleLeftClick);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &ThisClass::CancelPlacingBall);
    // A tap of A or D turns one step; holding them turns faster the longer they are down (PlayerTick).
    InputComponent->BindKey(EKeys::A, IE_Pressed, this, &ThisClass::NudgeAimLeft);
    InputComponent->BindKey(EKeys::D, IE_Pressed, this, &ThisClass::NudgeAimRight);
    InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &ThisClass::ToggleRecordShots);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ThisClass::HandleEscape);
}

void AUOpenGolfPlayerController::HandleEscape()
{
    if (bPlacingBall)
        SetPlacingBall(false);
    else if (bWindPanelOpen)
        SetWindPanelOpen(false);
    else
        QuitGame();
}

void AUOpenGolfPlayerController::TogglePlayMode()
{
    if (AUOpenGolfGameMode* Mode = GetRangeMode(*this); Mode && Mode->TogglePlayMode())
    {
        SetPlacingBall(false);
        // A new hole or range session starts with a clean view of the tee.
        if (auto* PresentedBall = Cast<AUOpenGolfBall>(Mode->GetBall()))
            PresentedBall->ClearShotTrailHistory();
    }
}

void AUOpenGolfPlayerController::SetPlacingBall(bool bPlacing)
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    bPlacingBall = bPlacing && Mode && Mode->GetPlayMode() == EOpenGolfPlayMode::Hole;
    CurrentMouseCursor = bPlacingBall ? EMouseCursor::Crosshairs : EMouseCursor::Default;
}

void AUOpenGolfPlayerController::HandleLeftClick()
{
    FVector2D ScreenPosition;
    if (bPlacingBall && GetMousePosition(ScreenPosition.X, ScreenPosition.Y) && PlaceBallAtScreen(ScreenPosition))
        SetPlacingBall(false);
}

bool AUOpenGolfPlayerController::PlaceBallAtScreen(const FVector2D& ScreenPosition)
{
    AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    const AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    FVector Origin, Direction;
    if (!Range || !IsValid(Range->Terrain) || !DeprojectScreenPositionToWorld(ScreenPosition.X, ScreenPosition.Y, Origin, Direction))
        return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(UOpenGolfPlaceBall), false, Mode->GetBall());
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * PlaceReachCm, ECC_Visibility, Params))
        return false;
    // The terrain's own query finds the ground under the point, through trees.
    FVector Ground, Normal;
    return Range->Terrain->GroundAt(Hit.ImpactPoint, Ground, Normal) && Normal.Z > 0.7 && Mode->PlaceBallAt(Ground);
}

bool AUOpenGolfPlayerController::IsShortGameView() const
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Ball || Mode->GetPlayMode() != EOpenGolfPlayMode::Hole)
        return false;
    FOpenFairwayShotResult Result;
    Ball->GetShotResult(Result);
    return OpenGolfPlay::IsShortGame(Mode->GetDistanceToCupM(), Result.Surface == EOpenFairwaySurface::Green);
}

void AUOpenGolfPlayerController::QuitGame()
{
    // Closes a standalone or packaged game; in the editor it ends Play.
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AUOpenGolfPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    const float TurnInput = (IsInputKeyDown(EKeys::D) ? 1.0f : 0.0f) - (IsInputKeyDown(EKeys::A) ? 1.0f : 0.0f);
    AimHeldSeconds = TurnInput != 0.0f ? AimHeldSeconds + DeltaTime : 0.0f;
    if (TurnInput != 0.0f)
    {
        SetAimYaw(GetAimYaw() + TurnInput * OpenGolfPlay::AimTurnDegPerSecond(AimHeldSeconds, IsFineAim()) * DeltaTime);
    }
    UpdateShotCamera(DeltaTime, false);
    AdoptRangeWind();
    if (const AUOpenGolfGameMode* Mode = GetRangeMode(*this); Mode && Mode->GetRange() && Mode->GetRange()->GetCup() && Mode->GetBall())
        Mode->GetRange()->GetCup()->UpdatePinLift(Mode->GetBall()->GetActorLocation(), DeltaTime);
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
    // Both views sit behind the ball along the aim line.
    const FRotator Aim(0.0f, Ball->AimYawDeg, 0.0f);
    if (IsBallOnTee(*Mode, *Ball))
    {
        const bool bShortGame = IsShortGameView();
        const FVector Desired = BallLocation + Aim.RotateVector(bShortGame ? ShortGameCameraOffset : TeeCameraOffset);
        // Along the putt, at the cup's height, so the ball sits low in the frame with the cup ahead.
        const FVector LookAt = bShortGame ? BallLocation + Aim.Vector() * FMath::Max(Mode->GetDistanceToCupM() * 100.0f, ShortGameMinLookCm) +
                                                FVector(0, 0, Mode->GetCupLocation().Z - BallLocation.Z)
                                          : BallLocation + Aim.RotateVector(TeeCameraTarget);
        // Aim from the final camera spot, never from the camera's path, so the view cannot flip
        // while the camera passes over the aim point on its way back.
        const FQuat TeeRotation = (LookAt - Desired).Rotation().Quaternion();
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
        // The glide starts from wherever the cup zoom left the camera.
        CupZoom = 0.0f;

        if (!bReturningToTee)
        {
            // The teed ball is still, so the camera holds its spot exactly and A/D turns orbit without lag.
            ShotCamera->SetActorLocationAndRotation(Desired, TeeRotation);
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
    UpdateCupZoom(*Ball, DeltaTime, bSnap);
    const FVector Desired = BallLocation + Aim.RotateVector(FMath::Lerp(FollowOffset, CupZoomOffset, CupZoom));
    const float CameraSpeed = FMath::Lerp(FollowSpeed, CupCameraFollowSpeed, CupZoom);
    const FVector Location = bSnap ? Desired : FMath::VInterpTo(ShotCamera->GetActorLocation(), Desired, DeltaTime, CameraSpeed);
    // Aim at the ball so it stays in frame while the camera catches up, then tilt up to frame it low.
    FRotator DesiredRotation = (BallLocation - Location).Rotation();
    DesiredRotation.Pitch += FollowFramingPitch;
    const FRotator Rotation = bSnap ? DesiredRotation : FMath::RInterpTo(ShotCamera->GetActorRotation(), DesiredRotation, DeltaTime, CameraSpeed * 2.0f);
    ShotCamera->SetActorLocationAndRotation(Location, Rotation);
}

void AUOpenGolfPlayerController::UpdateCupZoom(const AOpenFairwayBall& Ball, float DeltaTime, bool bSnap)
{
    // Close on a holed ball until it is teed up again. A moving ball only within CupZoomStartM of a cup, so the view
    // backs out as it rolls away, to the normal distance by 8 ft past, and once a miss stops.
    float Target = Ball.IsHoled() ? 1.0f : 0.0f;
    if (!Ball.IsHoled() && Ball.IsMoving())
    {
        // In 3D, so a shot flying over the flag does not close in.
        double NearestCm = TNumericLimits<double>::Max();
        for (const FVector& Cup : Ball.GetCupLocations())
            NearestCm = FMath::Min(NearestCm, FVector::Dist(Ball.GetActorLocation(), Cup));
        Target = OpenGolfPlay::CupZoomAmount(static_cast<float>(NearestCm / 100.0));
    }
    CupZoom = bSnap ? Target : FMath::FInterpTo(CupZoom, Target, DeltaTime, Target > CupZoom ? CupZoomInSpeed : CupZoomOutSpeed);
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
        Mode->ResetBall();
    }
}

bool AUOpenGolfPlayerController::SetAimYaw(float Degrees)
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    // Turning mid-shot or under a stopped ball's result would change how its offline is measured.
    if (!Ball || !IsBallOnTee(*Mode, *Ball))
    {
        return false;
    }
    const float Center = AimCenterDeg(*Mode, *Ball);
    Ball->AimYawDeg = Center + FMath::Clamp(FRotator::NormalizeAxis(Degrees - Center), -MaxAimYawDeg, MaxAimYawDeg);
    // Face the teed ball along the new line, as a re-tee does.
    Ball->SetActorRotation(FRotator(0.0f, Ball->AimYawDeg, 0.0f));
    return true;
}

bool AUOpenGolfPlayerController::NudgeAim(int32 Direction)
{
    return SetAimYaw(GetAimYaw() + FMath::Sign(Direction) * (IsFineAim() ? OpenGolfPlay::FineAimTapStepDeg : OpenGolfPlay::AimTapStepDeg));
}

bool AUOpenGolfPlayerController::IsFineAim() const
{
    return IsInputKeyDown(EKeys::LeftShift) || IsInputKeyDown(EKeys::RightShift);
}

float AUOpenGolfPlayerController::GetAimOffsetDeg() const
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    return Ball ? FRotator::NormalizeAxis(Ball->AimYawDeg - AimCenterDeg(*Mode, *Ball)) : 0.0f;
}

float AUOpenGolfPlayerController::GetAimYaw() const
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    return Ball ? Ball->AimYawDeg : 0.0f;
}

void AUOpenGolfPlayerController::SetWindOn(bool bOn)
{
    AdoptRangeWind();
    Wind.bOn = bOn;
    ApplyWind();
}

void AUOpenGolfPlayerController::SetWindSpeedMph(float Mph)
{
    AdoptRangeWind();
    Wind.SetSpeedMph(Mph);
    ApplyWind();
}

void AUOpenGolfPlayerController::SetWindFromDeg(float FromDeg)
{
    AdoptRangeWind();
    Wind.SetFromDeg(FromDeg);
    ApplyWind();
}

void AUOpenGolfPlayerController::ApplyWind()
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    if (!Range)
        return;
    // The same controls as Details: the range passes them to the ball, flag and vegetation on its next tick.
    Range->WindSpeedMph = Wind.AppliedMph();
    Range->WindFromDeg = Wind.FromDeg;
    AppliedWindMph = Range->WindSpeedMph;
    AppliedWindFromDeg = Range->WindFromDeg;
}

void AUOpenGolfPlayerController::AdoptRangeWind()
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    const AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    if (!Range || (Range->WindSpeedMph == AppliedWindMph && Range->WindFromDeg == AppliedWindFromDeg))
        return;
    Wind.Adopt(Range->WindSpeedMph, Range->WindFromDeg);
    AppliedWindMph = Range->WindSpeedMph;
    AppliedWindFromDeg = Range->WindFromDeg;
}

void AUOpenGolfPlayerController::ToggleRecordShots()
{
    if (AUOpenGolfGameMode* Mode = GetRangeMode(*this))
    {
        Mode->SetRecordShots(!Mode->IsRecordingShots());
    }
}
