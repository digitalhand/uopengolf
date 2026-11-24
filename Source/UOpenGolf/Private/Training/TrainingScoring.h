// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/** Rules for the target games, as value types: each step returns a new state and leaves its input alone. */
namespace TrainingScoring
{
    constexpr double CmPerFoot = 30.48;
    constexpr double CmPerYard = 91.44;
    constexpr int32 DefaultRadiusFeet = 3;
    constexpr int32 RadiusChoices[] = {3, 6, 10};
    /** The tries a player can choose for a game that uses them (UTrainingDrill::UsesTries). */
    constexpr int32 TryChoices[] = {3, 5};
    constexpr int32 DefaultTries = TryChoices[0];

    bool IsValidTries(int32 Tries);
    bool IsValidRadius(int32 Feet);
    FText RadiusText(int32 Feet);

    /** A holed ball or one touching the stripe is a hit. A safety stop is a miss. */
    bool IsHit(double DistanceCm, bool bHoled, bool bSafetyStop, double RadiusCm, double StripeWidthCm);

    /** Shots and hits at one target, with no limit on the number of shots. */
    struct FHitTally
    {
        int32 Shots = 0;
        int32 Hits = 0;
    };

    /** The tally after one more scored shot. */
    FHitTally Record(const FHitTally& Tally, bool bHit);

    /** The ladder's targets: 15 yd, then every 10 yd to 145 yd, then 150 yd. */
    TConstArrayView<int32> LadderYards();

    enum class ELadderStep : uint8
    {
        /** No shot scored yet at this target. */
        Start,
        /** A miss with tries left at this target. */
        Missed,
        /** A hit moves on to the next target. */
        Advanced,
        /** Every try missed: back one target. */
        DroppedBack,
        /** Every try missed at the first target: try it again. */
        Repeated,
        /** A hit at the last target. */
        Completed
    };

    struct FLadderState
    {
        /** Index into LadderYards(). */
        int32 Stage = 0;
        /** Tries used at the current target. */
        int32 TriesUsed = 0;
        int32 TotalShots = 0;
        /** The furthest target cleared, or INDEX_NONE. */
        int32 BestStage = INDEX_NONE;
        ELadderStep LastStep = ELadderStep::Start;
        bool IsComplete() const { return LastStep == ELadderStep::Completed; }
    };

    /** The ladder after one scored shot with Tries per target; one hit clears a target. */
    FLadderState Next(const FLadderState& State, int32 Tries, bool bHit);
} // namespace TrainingScoring
