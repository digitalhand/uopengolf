// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/DataAsset.h"
#include "GolfCourseDefinition.generated.h"

class UOpenGolfLieRules;
class UTexture2D;
class UWorld;

/**
 * One installed course: what the Courses browser shows and what a course round plays. The Asset Manager finds every
 * definition under /Game/UOpenGolf/Courses (primary asset type GolfCourse, see Config/DefaultGame.ini), so a course
 * is added with an asset, not code. The preview loads with the "Menu" bundle; the map loads only when the round starts.
 */
UCLASS(BlueprintType)
class UOPENGOLF_API UGolfCourseDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    /** The Asset Manager type every course is registered under. */
    static const FPrimaryAssetType AssetType;
    /** The asset bundle that holds the preview, loaded with the course list. */
    static const FName MenuBundle;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override { return FPrimaryAssetId(AssetType, GetFName()); }

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course")
    FText DisplayName;

    /** The level a round travels to. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course")
    TSoftObjectPtr<UWorld> Map;

    /** Card image; a UI texture (TEXTUREGROUP_UI, no mips) at the card's 5:3 aspect. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course", meta = (AssetBundles = "Menu"))
    TSoftObjectPtr<UTexture2D> Preview;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course|Scorecard", meta = (ClampMin = "1"))
    int32 Holes = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course|Scorecard", meta = (ClampMin = "1"))
    int32 Par = 4;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course|Scorecard", meta = (ClampMin = "0", Units = "m"))
    float LengthM = 0.0f;

    /** The round's wind, fixed for the whole round; Training keeps the player's own wind. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course|Wind", meta = (ClampMin = "0", UIMax = "30"))
    float WindMph = 0.0f;

    /** Where the round's wind comes from, in degrees against the target line (90 is from the right). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course|Wind", meta = (ClampMin = "0", ClampMax = "360"))
    float WindFromDeg = 0.0f;

    /**
     * How the round penalises shots from the rough, sand and other lies. Empty plays the standard (the class default of
     * UOpenGolfLieRules); courses can share one tuned asset or each have their own. Training never penalises a lie.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Course|Lies")
    TObjectPtr<const UOpenGolfLieRules> LieRules;

    /** The rules the round plays: LieRules, or the standard. */
    const UOpenGolfLieRules& GetLieRules() const;
};
