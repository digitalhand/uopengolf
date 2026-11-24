// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OpenFairwayTypes.h"
#include "OpenGolfLieRules.generated.h"

/** The kinds of club a lie penalty tells apart; a game maps its own clubs onto them. */
UENUM(BlueprintType)
enum class EOpenGolfClubGroup : uint8
{
    /** Driver and fairway woods. */
    Wood,
    Iron,
    /** The pitching wedge and every wedge below it. */
    Wedge,
    /** Never penalised: the grass already slows a ball rolled through it. */
    Putter
};

/**
 * What a lie takes off a shot hit from it, in percent of the launch monitor's measurement. Launch angles and direction
 * stay as measured: the real club's loft is already in them.
 */
USTRUCT(BlueprintType)
struct OPENGOLFCOURSE_API FOpenGolfLieCut
{
    GENERATED_BODY()

    /** No cut takes more than this; an authored value beyond it is held at it. */
    static constexpr float MaxPct = 90.0f;

    FOpenGolfLieCut() = default;
    FOpenGolfLieCut(float InBallSpeedPct, float InSpinPct) : BallSpeedPct(InBallSpeedPct), SpinPct(InSpinPct) {}

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lie", meta = (ClampMin = "0", ClampMax = "90", Units = "Percent"))
    float BallSpeedPct = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lie", meta = (ClampMin = "0", ClampMax = "90", Units = "Percent"))
    float SpinPct = 0.0f;

    bool IsNone() const { return BallSpeedPct <= 0.0f && SpinPct <= 0.0f; }
    /** What the measured ball speed is multiplied by. */
    float BallSpeedScale() const { return 1.0f - FMath::Clamp(BallSpeedPct, 0.0f, MaxPct) / 100.0f; }
    /** What each measured spin is multiplied by. */
    float SpinScale() const { return 1.0f - FMath::Clamp(SpinPct, 0.0f, MaxPct) / 100.0f; }
};

/** One lie's penalty for each kind of club. */
USTRUCT(BlueprintType)
struct OPENGOLFCOURSE_API FOpenGolfLiePenalty
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lie")
    FOpenGolfLieCut Woods;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lie")
    FOpenGolfLieCut Irons;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lie")
    FOpenGolfLieCut Wedges;

    /** The cut for Group; none for the putter. */
    FOpenGolfLieCut ForGroup(EOpenGolfClubGroup Group) const;
};

/**
 * How each lie penalises a shot on a course, shared by every course that uses it. The class default holds the standard
 * values (Docs/Reference.md, "Lie rules"); a course plays its own by pointing its definition at an asset of this class,
 * and a new asset starts from the standard. A surface with no row has no penalty, so a new surface needs only its row.
 * Course play applies these; Training never does.
 */
UCLASS(BlueprintType)
class OPENGOLFCOURSE_API UOpenGolfLieRules : public UDataAsset
{
    GENERATED_BODY()

public:
    UOpenGolfLieRules();

    /** Each surface's penalty anywhere on the hole. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lies")
    TMap<EOpenFairwaySurface, FOpenGolfLiePenalty> Lies;

    /**
     * Replaces a surface's row in the short game, within OpenGolfPlay::ShortGameDistanceM (30 yd) of the cup: a greenside
     * bunker's explosion shot.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Lies")
    TMap<EOpenFairwaySurface, FOpenGolfLiePenalty> NearGreenLies;

    /** The cut for a ball lying on Surface, DistanceToCupM from the cup, hit with a club of Group. */
    FOpenGolfLieCut CutFor(EOpenFairwaySurface Surface, float DistanceToCupM, EOpenGolfClubGroup Group) const;
};

/** Course-independent lie rules, shared by every course's game mode and HUD. */
namespace OpenGolfPlay
{
    /**
     * The launch a shot plays after its lie: Launch with ball speed and every spin it has cut by Cut. Launch angles, spin
     * axis and which values were measured stay as they were. Launch itself is not changed.
     */
    OPENGOLFCOURSE_API FOpenFairwayLaunch ApplyLieCut(const FOpenFairwayLaunch& Launch, const FOpenGolfLieCut& Cut);

    /** The speed cut beside the lie in the hole map's header, in whole percent: "−8%". Empty without one. */
    OPENGOLFCOURSE_API FString LieCutLabel(const FOpenGolfLieCut& Cut);

    /** The spin cut on the header's right: "Spin −25%". Empty without one. */
    OPENGOLFCOURSE_API FString SpinCutLabel(const FOpenGolfLieCut& Cut);
} // namespace OpenGolfPlay
