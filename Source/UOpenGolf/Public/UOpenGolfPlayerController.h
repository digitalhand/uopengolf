// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/PlayerController.h"
#include "Play/OpenGolfWindSetting.h"
#include "UOpenGolfPlayerController.generated.h"

class ACameraActor;
class AOpenFairwayBall;
class SWidget;
class SMainMenu;
class UGolfCourseDefinition;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;
struct FKey;

/** A test shot typed into the Test shots panel. The defaults are a basic straight shot: 100 mph, 16°, 5000 rpm, no curve. */
struct UOPENGOLF_API FOpenGolfCustomShot
{
    float BallSpeedMps = 44.704f;
    float VerticalLaunchDeg = 16.0f;
    /** Negative is left, positive right. */
    float HorizontalLaunchDeg = 0.0f;
    int32 TotalSpinRpm = 5000;
    /** Negative curves the ball left, positive right. */
    float SpinAxisDeg = 0.0f;

    /** Each value moved inside the launch-monitor limits that UOpenLaunchMonitorSubsystem::InjectCustomShot checks. */
    FOpenGolfCustomShot Clamped() const;
};

/**
 * Play controls, course travel and the shot camera. Keys: C connect, T disconnect, H hit selected shot, P presets, Tab
 * next club, R tee up the ball again, B place the ball in Training (then click the ground), hold A/D to turn the aim
 * left/right, W the wind controls (Training), F9 record shots on/off, Escape menu (or cancel placing, or close the wind
 * controls). A course round opens the final scorecard when the game mode announces OnRoundComplete. The camera
 * waits behind the ball along the aim line, holds that view for half a second after the hit, then follows low behind the
 * ball in flight and at rest, then glides back to the address view when the ball is ready for the next shot. On or near
 * the green the address view is lower and closer, looking along the putt. Within 8 ft of the cup the follow view slowly
 * closes in on the ball and backs out as it rolls away, by 8 ft past or once it stops; a holed ball keeps the close
 * view until it is teed up again.
 */
UCLASS()
class UOPENGOLF_API AUOpenGolfPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    bool IsMainMenuOpen() const { return bMainMenuOpen; }
    /** The menu is the pause menu (Training was entered) rather than the main menu. */
    bool IsPauseMenu() const;
    void SetMainMenuOpen(bool bOpen);
    void OpenMainMenu();
    /** Opens the menu at Settings › Player: the HUD's settings cog does this when clicked. */
    void OpenGeneralSettings();
    bool CanUseManualShots() const;
    /**
     * Ends the Training session or course round and shows the main menu: opens the game's default map, so the ball,
     * round and session defaults start fresh and any recording is finalized. The launch monitor (game-instance) stays
     * connected.
     */
    void ReturnToMainMenu();
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void ConnectLaunchMonitor();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void DisconnectLaunchMonitor();

    /** Hits the selected test shot (H): the custom shot when it is selected, otherwise the selected preset. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void InjectReplayShot();

    /** Selects a preset, which also leaves the custom shot. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Replay")
    bool SelectReplayShot(int32 Index);

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Replay")
    int32 GetSelectedReplayShot() const { return SelectedReplayIndex; }

    /** Makes H hit the custom shot. Its values last until the map reloads (Main menu, or quitting). */
    void SelectCustomShot() { bCustomShotSelected = true; }
    bool IsCustomShotSelected() const { return bCustomShotSelected; }
    const FOpenGolfCustomShot& GetCustomShot() const { return CustomShot; }
    /** Stores the values limited by FOpenGolfCustomShot::Clamped. */
    void SetCustomShot(const FOpenGolfCustomShot& Shot) { CustomShot = Shot.Clamped(); }
    /** Back to the straight-shot defaults. */
    void ResetCustomShot() { CustomShot = FOpenGolfCustomShot(); }

    void ToggleReplayPanel();
    void SetReplayPanelOpen(bool bOpen);
    bool IsReplayPanelOpen() const { return bReplayPanelOpen; }

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void SelectNextClub();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Range")
    void ResetBallToTee();

    /** A scored course round rather than Training; see AUOpenGolfGameMode::IsCourseRound. */
    bool IsCourseRound() const;
    /** A course round whose ball has dropped; see AUOpenGolfGameMode::IsRoundComplete. */
    bool IsRoundComplete() const;
    /** Travels to Course's map for a fresh round. */
    void StartCourse(const UGolfCourseDefinition& Course);
    /** Ends the round and opens the main menu at the Courses browser. */
    void ReturnToCourses();

    /** In Training, the next click on the ground places the ball there; a right click or Escape cancels. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    void SetPlacingBall(bool bPlacing);

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    void TogglePlacingBall() { SetPlacingBall(!bPlacingBall); }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    bool IsPlacingBall() const { return bPlacingBall; }

    void CancelPlacingBall() { SetPlacingBall(false); }

    /** Places the ball on the ground under a viewport position, in Training; false when no ground is there. */
    bool PlaceBallAtScreen(const FVector2D& ScreenPosition);

    /** Lower, closer address view along the putt, for a short-game shot (OpenGolfPlay::IsShortGame) in Hole mode. */
    bool IsShortGameView() const;

    /** How far the follow view has closed in on a ball near the cup: 0 at the normal distance, 1 fully in. */
    float GetCupZoom() const { return CupZoom; }

    /**
     * Turns the aim line to Degrees clockwise (seen from above) from the world +X target line,
     * limited to +/-MaxAimYawDeg. Only while the ball sits on the tee; returns false otherwise.
     */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Range")
    bool SetAimYaw(float Degrees);

    /** The ball's aim in degrees, positive to the right; 0 without a ball. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Range")
    float GetAimYaw() const;

    static constexpr float MaxAimYawDeg = 45.0f;

    /** One tap of A (Direction -1) or D (+1): turns exactly OpenGolfPlay::AimTapStepDeg, or the fine step with Shift. */
    bool NudgeAim(int32 Direction);

    /**
     * Aims from the ball toward a world XY point, as SetAimYaw, for pointing at the hole map. False while placing a ball,
     * when the ball cannot aim now, or when the point is within MinDistanceCm of the ball (it has no direction).
     */
    bool AimToward(const FVector2D& WorldPoint, double MinDistanceCm = 1.0);

    /**
     * How far along the aim (cm) the hole map draws its target: where the map was last clicked from this lie, else the
     * distance to the cup. Turning the aim keeps it; a new lie (not the re-tee to the same spot) starts at the cup.
     */
    double GetAimTargetDistanceCm() const;

    /** The aim off the target line in degrees, positive to the right: the range's line, or the line to the cup. */
    float GetAimOffsetDeg() const;

    /** Opens or closes the wind controls under the wind indicator (W, or a click on the indicator). */
    void ToggleWindPanel() { SetWindPanelOpen(!bWindPanelOpen); }
    void SetWindPanelOpen(bool bOpen) { bWindPanelOpen = bOpen && !IsCourseRound(); }
    bool IsWindPanelOpen() const { return bWindPanelOpen; }

    /** The wind the HUD sets. It follows wind set on the map, in Details or from the console until the player changes it. */
    const OpenGolfPlay::FOpenGolfWindSetting& GetWind() const { return Wind; }

    /**
     * Wind changes from the HUD. Each reaches the range at once, which passes it to the ball, flag and vegetation; a
     * shot in flight keeps the wind it was hit in. The map keeps its saved wind.
     */
    void SetWindOn(bool bOn);
    void SetWindSpeedMph(float Mph);
    void SetWindFromDeg(float FromDeg);

    /** Switches saving of real launch-monitor shots on or off; on the range only, since the hole never records. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Recording")
    void ToggleRecordShots();

    /** Quits the game (ends Play in the editor). */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Range")
    void QuitGame();

    /** Escape: cancels the current tool, otherwise opens the menu. */
    void HandleEscape();
    bool IsDrillDialogOpen() const { return DrillDialog.IsValid(); }
    /** The Exit/Restart dialog of the active drill, which also reports a finished drill's result. */
    void OpenDrillDialog();
    void CloseDrillDialog();
    void ClearMapAim() { AimTargetLie.Reset(); }
    void HandleMenuBack();

    /**
     * Feeds a key from a HUD panel that holds keyboard focus into this controller's input, as the viewport would,
     * so every play key (and a held A/D) works there too. False when no action uses the key.
     */
    bool ForwardUIKey(const FKey& Key, bool bPressed);

    /** The play keys as Enhanced Input actions, built in code with the input component rather than as assets. */
    const UInputMappingContext* GetKeyMapping() const { return KeyMapping; }

    virtual void PlayerTick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;

private:
    void UpdateShotCamera(float DeltaTime, bool bSnap);
    void UpdateCupZoom(const AOpenFairwayBall& Ball, float DeltaTime, bool bSnap);
    void HandleLeftClick();
    /** A or D going down: one exact step, its sign the turn direction. */
    void StartTurn(const FInputActionValue& Value);
    /** While A or D is held, every frame: the direction PlayerTick turns the aim in. */
    void HoldTurn(const FInputActionValue& Value);
    void StopTurn() { TurnInput = 0.0f; }
    void StartFineAim() { bFineAimHeld = true; }
    void StopFineAim() { bFineAimHeld = false; }
    bool IsFineAim() const { return bFineAimHeld; }
    /** Writes the HUD's wind to the range. */
    void ApplyWind();
    /** Takes wind the range got elsewhere (the map, Details, the console) into the HUD's setting. */
    void AdoptRangeWind();
    /** After a course round's hole-out banner: opens the menu, which shows the final scorecard. */
    void ShowFinishedRound();
    /** Opens the default map with one URL option (the main menu, or the Courses browser). */
    void TravelToMenu(const TCHAR* Option);
    FDelegateHandle RoundCompleteHandle;

    UPROPERTY()
    TObjectPtr<ACameraActor> ShotCamera;

    // Glide back to the tee view after the ball is teed up again.
    FVector ReturnStartLocation = FVector::ZeroVector;
    FQuat ReturnStartRotation = FQuat::Identity;
    float ReturnElapsed = 0.0f;
    bool bReturningToTee = false;
    bool bCameraAtTee = true;
    // Time since the ball left the tee, for the delay before the camera follows it.
    float FollowDelayElapsed = 0.0f;
    // Near the cup: how far the follow view has closed in.
    float CupZoom = 0.0f;

    TSharedPtr<class FHudNavigation> HudNavigation;
    TSharedPtr<SMainMenu> MainMenu;
    TSharedPtr<SWidget> DrillDialog;
    bool bMainMenuOpen = false;
    TSharedPtr<SWidget> ReplayPanel;
    int32 SelectedReplayIndex = 0;
    FOpenGolfCustomShot CustomShot;
    bool bCustomShotSelected = false;
    bool bReplayPanelOpen = false;
    bool bPlacingBall = false;
    // The hole map's target distance (cm), set by AimToward, and the lie it was set from.
    double AimTargetDistanceCm = 0.0;
    TOptional<FVector2D> AimTargetLie;
    // How long A or D has been held, for the turn-rate ramp.
    float AimHeldSeconds = 0.0f;
    // -1 while A is held, +1 while D is held, 0 otherwise.
    float TurnInput = 0.0f;
    bool bFineAimHeld = false;

    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> KeyMapping;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UInputAction>> KeyActions;
    OpenGolfPlay::FOpenGolfWindSetting Wind;
    bool bWindPanelOpen = false;
    // The range's wind as this controller last applied or adopted it; any other value was set elsewhere.
    float AppliedWindMph = -1.0f;
    float AppliedWindFromDeg = -1.0f;
};
