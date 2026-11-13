// SPDX-License-Identifier: MIT
#include "UOpenGolfPlayerController.h"
#include "OpenGolfSettings.h"
#include "GameFramework/PlayerInput.h"
#include "UI/Menu/MainMenu.h"
#include "UI/Hud/HudNavigation.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "OpenFairwayBall.h"
#include "Hole/OpenGolfCupComponent.h"
#include "OpenGolfTerrain.h"
#include "Play/OpenGolfPlayRules.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UOpenGolfBall.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"
#include "UI/Common/HudLayers.h"
#include "UI/Hud/ReplayShotPanel.h"
#include "Framework/Application/SlateApplication.h"

namespace
{
    /** Level URL option: the map was reloaded by "Main menu", so open the main menu. */
    const TCHAR* ReturnToMainMenuOption = TEXT("MainMenu");

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
    // Fast enough that a ball rolling on past the cup is back at the follow distance by about 8 ft past.
    constexpr float CupZoomOutSpeed = 3.0f;
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

    if (auto* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()); Input && KeyMapping)
    {
        Input->AddMappingContext(KeyMapping, 0);
    }

    // The camera follows the ball, so it reads the ball's position only after the ball has moved this frame.
    if (const AUOpenGolfGameMode* Mode = GetRangeMode(*this); Mode && Mode->GetBall())
    {
        AddTickPrerequisiteActor(Mode->GetBall());
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
        HudNavigation=MakeShared<FHudNavigation>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(HudNavigation);
        MainMenu = SNew(SMainMenu).Controller(this);
        Viewport->AddViewportWidgetContent(MainMenu.ToSharedRef(), 100);
        const AGameModeBase* Mode = GetWorld()->GetAuthGameMode();
        const bool bReturnedToMenu = Mode && UGameplayStatics::HasOption(Mode->OptionsString, ReturnToMainMenuOption);
        if (!GolfSettings::BypassStartupMenu() || bReturnedToMenu) MainMenu->Open();
    }
}

void AUOpenGolfPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if(HudNavigation && FSlateApplication::IsInitialized())FSlateApplication::Get().UnregisterInputPreProcessor(HudNavigation);
    HudNavigation.Reset();
    if (MainMenu.IsValid())
    {
        if (auto* Viewport = GetWorld()->GetGameViewport()) Viewport->RemoveViewportWidgetContent(MainMenu.ToSharedRef());
        MainMenu.Reset();
    }
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
    auto* Input = Cast<UEnhancedInputComponent>(InputComponent);
    if (!ensureMsgf(Input, TEXT("Config/DefaultInput.ini must select the Enhanced Input component.")))
        return;
    KeyMapping = NewObject<UInputMappingContext>(this, TEXT("RangeKeys"));
    KeyActions.Reset();
    auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
    {
        UInputAction* Action = NewObject<UInputAction>(this, Name);
        Action->ValueType = Type;
        KeyActions.Add(Action);
        return Action;
    };

    struct FPress
    {
        const TCHAR* Name;
        FKey Key;
        void (ThisClass::*Handler)();
    };
    const FPress Presses[] = {
        {TEXT("IA_Connect"), EKeys::C, &ThisClass::ConnectLaunchMonitor},
        {TEXT("IA_Disconnect"), EKeys::T, &ThisClass::DisconnectLaunchMonitor},
        {TEXT("IA_HitTestShot"), EKeys::H, &ThisClass::InjectReplayShot},
        {TEXT("IA_TestShots"), EKeys::P, &ThisClass::ToggleReplayPanel},
        {TEXT("IA_NextClub"), EKeys::Tab, &ThisClass::SelectNextClub},
        {TEXT("IA_TeeUp"), EKeys::R, &ThisClass::ResetBallToTee},
        {TEXT("IA_PlayMode"), EKeys::G, &ThisClass::TogglePlayMode},
        {TEXT("IA_PlaceBall"), EKeys::B, &ThisClass::TogglePlacingBall},
        {TEXT("IA_Wind"), EKeys::W, &ThisClass::ToggleWindPanel},
        {TEXT("IA_Click"), EKeys::LeftMouseButton, &ThisClass::HandleLeftClick},
        {TEXT("IA_CancelPlacing"), EKeys::RightMouseButton, &ThisClass::CancelPlacingBall},
        {TEXT("IA_RecordShots"), EKeys::F9, &ThisClass::ToggleRecordShots},
        {TEXT("IA_Escape"), EKeys::Escape, &ThisClass::HandleEscape},
    };
    for (const FPress& Press : Presses)
    {
        UInputAction* Action = MakeAction(Press.Name, EInputActionValueType::Boolean);
        KeyMapping->MapKey(Action, Press.Key);
        Input->BindAction(Action, ETriggerEvent::Started, this, Press.Handler);
    }

    // A tap of A or D turns one step; holding them turns faster the longer they are down (PlayerTick).
    UInputAction* Turn = MakeAction(TEXT("IA_TurnAim"), EInputActionValueType::Axis1D);
    KeyMapping->MapKey(Turn, EKeys::D);
    KeyMapping->MapKey(Turn, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(KeyMapping));
    Input->BindAction(Turn, ETriggerEvent::Started, this, &ThisClass::StartTurn);
    Input->BindAction(Turn, ETriggerEvent::Triggered, this, &ThisClass::HoldTurn);
    Input->BindAction(Turn, ETriggerEvent::Completed, this, &ThisClass::StopTurn);
    // Shift makes both the tap and the held turn fine.
    UInputAction* FineAim = MakeAction(TEXT("IA_FineAim"), EInputActionValueType::Boolean);
    KeyMapping->MapKey(FineAim, EKeys::LeftShift);
    KeyMapping->MapKey(FineAim, EKeys::RightShift);
    Input->BindAction(FineAim, ETriggerEvent::Started, this, &ThisClass::StartFineAim);
    Input->BindAction(FineAim, ETriggerEvent::Completed, this, &ThisClass::StopFineAim);
}

void AUOpenGolfPlayerController::StartTurn(const FInputActionValue& Value)
{
    if (const float Direction = Value.Get<float>(); Direction != 0.0f)
        NudgeAim(Direction > 0.0f ? 1 : -1);
}

void AUOpenGolfPlayerController::HoldTurn(const FInputActionValue& Value)
{
    TurnInput = FMath::Sign(Value.Get<float>());
}

bool AUOpenGolfPlayerController::ForwardUIKey(const FKey& Key, bool bPressed)
{
    if (bMainMenuOpen) return false;
    if (!KeyMapping || !KeyMapping->GetMappings().ContainsByPredicate([&Key](const FEnhancedActionKeyMapping& Mapping) { return Mapping.Key == Key; }))
        return false;
    // The same path a key takes from the viewport, so taps, holds and Shift all behave as they do there.
    InputKey(FInputKeyEventArgs::CreateSimulated(Key, bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
    return true;
}

void AUOpenGolfPlayerController::HandleEscape()
{
    if (bPlacingBall)
        SetPlacingBall(false);
    else if (bWindPanelOpen)
        SetWindPanelOpen(false);
    else if (bReplayPanelOpen)
        SetReplayPanelOpen(false);
    else
        OpenMainMenu();
}

void AUOpenGolfPlayerController::TogglePlayMode()
{
    if (bMainMenuOpen) return;
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
    // The click picks what is visible; the terrain's own query then finds the ground the ball rests on, through trees.
    FVector Ground, Normal;
    return Range->Terrain->GroundAt(Hit.ImpactPoint, Ground, Normal, AUOpenGolfBall::ContactChannel) && Normal.Z > 0.7 && Mode->PlaceBallAt(Ground);
}

bool AUOpenGolfPlayerController::IsShortGameView() const
{
    const AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Ball || Mode->GetPlayMode() != EOpenGolfPlayMode::Hole)
        return false;
    return OpenGolfPlay::IsShortGame(Mode->GetDistanceToCupM(), Ball->GetResult().Surface == EOpenFairwaySurface::Green);
}

void AUOpenGolfPlayerController::QuitGame()
{
    // Closes a standalone or packaged game; in the editor it ends Play.
    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AUOpenGolfPlayerController::PlayerTick(float DeltaTime)
{
    // Input runs first (Super), so TurnInput is this frame's A/D.
    Super::PlayerTick(DeltaTime);
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

    bSnap = bSnap || GolfSettings::Get().ReducedMotion;
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

    if (GolfSettings::Get().FixedCamera || GolfSettings::Get().ReducedMotion) return;
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
    if (!GolfSettings::Get().CupZoom) { CupZoom = 0; return; }
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
    if (bMainMenuOpen) return;
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
    if (bMainMenuOpen) return;
    if (UOpenLaunchMonitorSubsystem* LaunchMonitor = GetLaunchMonitor(*this))
    {
        const int32 ClubCount = static_cast<int32>(ELaunchMonitorClub::Putter) + 1;
        LaunchMonitor->SetClub(static_cast<ELaunchMonitorClub>((static_cast<int32>(LaunchMonitor->GetClub()) + 1) % ClubCount));
    }
}

void AUOpenGolfPlayerController::ResetBallToTee()
{
    if (bMainMenuOpen) return;
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
    if (bMainMenuOpen) return false;
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
    // The same controls as Details; the range passes them to the ball, flag and vegetation straight away.
    Range->SetWind(Wind.AppliedMph(), Wind.FromDeg);
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
    if (bMainMenuOpen) return;
    // Recording is for the range; the hole never records, so F9 does nothing there.
    AUOpenGolfGameMode* Mode = GetRangeMode(*this);
    if (Mode && Mode->GetPlayMode() == EOpenGolfPlayMode::Range)
    {
        Mode->SetRecordShots(!Mode->IsRecordingShots());
    }
}

void AUOpenGolfPlayerController::OpenMainMenu()
{
    if (MainMenu.IsValid()) MainMenu->Open();
}

void AUOpenGolfPlayerController::OpenLaunchMonitorSettings()
{
    if (MainMenu.IsValid()) MainMenu->OpenLaunchMonitorSettings();
}

bool AUOpenGolfPlayerController::IsPauseMenu() const
{
    return MainMenu.IsValid() && MainMenu->IsPauseMenu();
}

void AUOpenGolfPlayerController::ReturnToMainMenu()
{
    // The MainMenu option makes the reloaded controller open the menu, even under -NoMainMenu (automation).
    UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, ReturnToMainMenuOption);
}

void AUOpenGolfPlayerController::SetMainMenuOpen(bool bOpen)
{
    bMainMenuOpen = bOpen;
    TurnInput = 0; AimHeldSeconds = 0; bFineAimHeld = false;
    if (PlayerInput) PlayerInput->FlushPressedKeys();
    if (bOpen)
    {
        SetPlacingBall(false); SetWindPanelOpen(false); SetReplayPanelOpen(false);
        FInputModeUIOnly Mode;
        if (MainMenu.IsValid()) Mode.SetWidgetToFocus(MainMenu);
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        SetInputMode(Mode);
    }
    else
    {
        FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock); SetInputMode(Mode);
        if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
    bShowMouseCursor = true;
}

void AUOpenGolfPlayerController::HandleMenuBack() { if(MainMenu.IsValid()) MainMenu->NavigateBack(); }
