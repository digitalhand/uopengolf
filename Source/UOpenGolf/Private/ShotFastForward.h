// SPDX-License-Identifier: MIT
#pragma once

#include "Components/ActorComponent.h"
#include "Play/OpenGolfFastForward.h"
#include "ShotFastForward.generated.h"

class AOpenFairwayBall;
struct FInputActionValue;
struct FOpenFairwayShotResult;

/**
 * Space on a rolling ball, for the player controller that owns it: held, the world plays four times faster; tapped, it
 * skips to where the ball stops. It speeds up the whole game clock (global time dilation), so the camera, cup, flag
 * and timers keep in step, and the ball's fixed physics step stops it in the same place. The world is back at normal
 * speed the moment the ball rests or drops into the cup, a new shot starts, the ball is moved, or a menu opens.
 */
UCLASS()
class UShotFastForward : public UActorComponent
{
    GENERATED_BODY()

public:
    UShotFastForward();

    void Press(const FInputActionValue& Value);
    void Release(const FInputActionValue& Value);

    /** The world's speed this component set: 1, OpenGolfPlay::FastForwardHoldRate or FastForwardSkipRate. */
    float GetAppliedRate() const { return AppliedRate; }
    /** The ball rolls on the ground, so Space can speed it up. */
    bool CanFastForward() const;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleShotEnded(const FOpenFairwayShotResult& Result);

    AOpenFairwayBall* GetBall() const;
    /** A menu or dialog has the player's attention. */
    bool IsInterrupted() const;
    double RealSeconds() const;
    /** Normal speed now, without waiting for the next tick: rest timers set in the same frame then run in real time. */
    void Stop();
    void Apply(float Rate);

    OpenGolfPlay::FOpenGolfFastForward Input;
    TWeakObjectPtr<AOpenFairwayBall> BoundBall;
    float AppliedRate = 1.0f;
    int32 LastShotNumber = INDEX_NONE;
    bool bWasRolling = false;
};
