// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/PlayerController.h"
#include "UOpenGolfPlayerController.generated.h"

class ACameraActor;
class SWidget;

/**
 * Range controls and the shot camera. Keys: C connect, T disconnect, H hit selected shot, P presets, Tab next
 * club, R tee up the ball again, hold A/D to turn the aim left/right, F9 record shots on/off, Escape quit. The camera
 * waits behind the tee along the aim line, holds that view for half a second after the hit, then follows low behind the
 * ball in flight and at rest, then glides back to the tee view when the ball is teed up again.
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

    /** Switches saving of real launch-monitor shots on or off. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Recording")
    void ToggleRecordShots();

    /** Quits the game (ends Play in the editor). */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Range")
    void QuitGame();

    virtual void PlayerTick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void SetupInputComponent() override;

private:
    void UpdateShotCamera(float DeltaTime, bool bSnap);

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

    TSharedPtr<SWidget> ReplayPanel;
    int32 SelectedReplayIndex = 0;
    bool bReplayPanelOpen = false;
};
