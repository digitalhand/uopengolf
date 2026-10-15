// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/PlayerController.h"
#include "UOpenGolfPlayerController.generated.h"

class ACameraActor;

/**
 * Range controls and the shot camera. Keys: C connect, X disconnect, R replay shot, Tab next
 * club, T tee up the ball again. The camera waits behind the tee, follows the ball in flight
 * and settles behind it at rest.
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

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Launch Monitor")
    void SelectNextClub();

    UFUNCTION(BlueprintCallable, Category = "UOpenGolf|Range")
    void ResetBallToTee();

    virtual void PlayerTick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    void UpdateShotCamera(float DeltaTime, bool bSnap);

    UPROPERTY()
    TObjectPtr<ACameraActor> ShotCamera;
};
