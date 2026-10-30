// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/PlayerController.h"
#include "Play/OpenGolfWindSetting.h"
#include "UOpenGolfPlayerController.generated.h"

class ACameraActor;
class AOpenFairwayBall;
class SWidget;

/**
 * Range controls and the shot camera. Keys: C connect, T disconnect, H hit selected shot, P presets, Tab next
 * club, R tee up the ball again, G play the hole / back to the range, B place the ball (Hole mode; then click the
 * ground), hold A/D to turn the aim left/right, W the wind controls, F9 record shots on/off, Escape quit (or cancel
 * placing, or close the wind controls). The camera
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
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void ConnectLaunchMonitor();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void DisconnectLaunchMonitor();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void InjectReplayShot();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Replay")
    bool SelectReplayShot(int32 Index);

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Replay")
    int32 GetSelectedReplayShot() const { return SelectedReplayIndex; }

    void ToggleReplayPanel();
    void SetReplayPanelOpen(bool bOpen);
    bool IsReplayPanelOpen() const { return bReplayPanelOpen; }

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void SelectNextClub();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Range")
    void ResetBallToTee();

    /** Switches between the range and playing the hole. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    void TogglePlayMode();

    /** In Hole mode, the next click on the ground places the ball there; a right click or Escape cancels. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    void SetPlacingBall(bool bPlacing);

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Play")
    void TogglePlacingBall() { SetPlacingBall(!bPlacingBall); }

    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Play")
    bool IsPlacingBall() const { return bPlacingBall; }

    void CancelPlacingBall() { SetPlacingBall(false); }

    /** Places the ball on the ground under a viewport position, in Hole mode; false when no ground is there. */
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

    /** The aim off the target line in degrees, positive to the right: the range's line, or the line to the cup. */
    float GetAimOffsetDeg() const;

    /** Opens or closes the wind controls under the wind indicator (W, or a click on the indicator). */
    void ToggleWindPanel() { SetWindPanelOpen(!bWindPanelOpen); }
    void SetWindPanelOpen(bool bOpen) { bWindPanelOpen = bOpen; }
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

    /** Switches saving of real launch-monitor shots on or off. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Recording")
    void ToggleRecordShots();

    /** Quits the game (ends Play in the editor). */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Range")
    void QuitGame();

    /** Escape: cancels placing the ball, else quits. */
    void HandleEscape();

    virtual void PlayerTick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;

private:
    void UpdateShotCamera(float DeltaTime, bool bSnap);
    void UpdateCupZoom(const AOpenFairwayBall& Ball, float DeltaTime, bool bSnap);
    void HandleLeftClick();
    void NudgeAimLeft() { NudgeAim(-1); }
    void NudgeAimRight() { NudgeAim(1); }
    bool IsFineAim() const;
    /** Writes the HUD's wind to the range. */
    void ApplyWind();
    /** Takes wind the range got elsewhere (the map, Details, the console) into the HUD's setting. */
    void AdoptRangeWind();

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

    TSharedPtr<SWidget> ReplayPanel;
    int32 SelectedReplayIndex = 0;
    bool bReplayPanelOpen = false;
    bool bPlacingBall = false;
    // How long A or D has been held, for the turn-rate ramp.
    float AimHeldSeconds = 0.0f;
    OpenGolfPlay::FOpenGolfWindSetting Wind;
    bool bWindPanelOpen = false;
    // The range's wind as this controller last applied or adopted it; any other value was set elsewhere.
    float AppliedWindMph = -1.0f;
    float AppliedWindFromDeg = -1.0f;
};
