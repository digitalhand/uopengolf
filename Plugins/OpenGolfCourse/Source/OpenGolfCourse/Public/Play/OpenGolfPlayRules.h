// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "OpenGolfPlayRules.generated.h"

/** How shots are played on a course. */
UENUM(BlueprintType)
enum class EOpenGolfPlayMode : uint8
{
    /** Every shot is hit from the tee, and a stopped ball goes back to it. */
    Range,
    /** The hole is played: the ball is hit from where it stops until it drops into the cup. */
    Hole
};

/** Course-independent rules for playing a hole, shared by every course's game mode and HUD. */
namespace OpenGolfPlay
{
    /** Within this of the cup (30 yd), or on the green, the shot is a short-game shot (ogolf CameraProfiles.ForLie). */
    inline constexpr float ShortGameDistanceM = 27.432f;

    /** A ball placed by hand keeps at least this far from the cup's centre. */
    inline constexpr float MinPlaceDistanceFromCupM = 0.15f;

    /** Degrees clockwise from world +X (seen from above) from From toward To; Fallback when they share a spot. */
    OPENGOLFCOURSE_API float BearingDeg(const FVector& From, const FVector& To, float Fallback = 0.0f);

    OPENGOLFCOURSE_API bool IsShortGame(float DistanceToCupM, bool bOnGreen);

    /** Within this of the cup (8 ft) the follow camera starts closing in on the ball. */
    inline constexpr float CupZoomStartM = 2.4384f;

    /** How far the follow camera closes in for a ball DistanceM from the cup: 0 from CupZoomStartM out, easing to 1 at the cup. */
    OPENGOLFCOURSE_API float CupZoomAmount(float DistanceM);

    /** False for a spot too close to the cup to place a ball (world centimetres). */
    OPENGOLFCOURSE_API bool CanPlaceBallAt(const FVector& Location, const FVector& Cup);

    /** Distance to the pin as players read it: feet on the green, otherwise yards. */
    OPENGOLFCOURSE_API FString DistanceToPinLabel(float DistanceM, bool bOnGreen);

    /** One tap of the aim keys turns exactly this; the fine step is with Shift held. */
    inline constexpr float AimTapStepDeg = 0.25f;
    inline constexpr float FineAimTapStepDeg = 0.1f;

    /**
     * Turn rate while an aim key has been held HeldSeconds: nothing during a tap (the tap step covers it), then a
     * smooth ramp from a slow to a fast turn, so a hold starts precise and still crosses the range quickly. Fine aim
     * keeps to the slow rate.
     */
    OPENGOLFCOURSE_API float AimTurnDegPerSecond(float HeldSeconds, bool bFine);

    /**
     * Aim offset from the target line as players read it, e.g. "1.5° R". With a target distance it adds how far right
     * or left of the target the line passes: feet in the short game, otherwise yards.
     */
    OPENGOLFCOURSE_API FString AimOffsetLabel(float OffsetDeg, float TargetDistanceM = 0.0f, bool bShortGame = false);
} // namespace OpenGolfPlay
