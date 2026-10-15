// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/PlayerController.h"
#include "UOpenGolfPlayerController.generated.h"

class ACameraActor;
class SWidget;

/**
 * Range controls and the shot camera. Keys: C connect, D disconnect, R repeat selected shot, P presets, Tab next
 * club, T tee up the ball again, F9 record shots on/off. The camera waits behind the tee, follows the ball in flight
 * and settles behind it at rest, then glides back to the tee view when the ball is teed up again.
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

    /** Switches saving of real launch-monitor shots on or off. */
    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Recording")
    void ToggleRecordShots();

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

    TSharedPtr<SWidget> ReplayPanel;
    int32 SelectedReplayIndex = 0;
    bool bReplayPanelOpen = false;
};
