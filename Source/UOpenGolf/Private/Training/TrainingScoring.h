// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/** Rules for the target games, as value types: each step returns a new state and leaves its input alone. */
namespace TrainingScoring
{
    constexpr double CmPerFoot = 30.48;
    constexpr double CmPerYard = 91.44;
    /** Scoring radii around the pin, measured to the ball's centre where it stops. */
    constexpr double InnerRadiusCm = 3 * CmPerFoot;
    constexpr double OuterRadiusCm = 6 * CmPerFoot;
    /** The tries a player can choose when picking a game. */
    constexpr int32 TryChoices[] = {3, 5};

    enum class ERing : uint8
    {
        Inside3Ft,
        Inside6Ft,
        Miss
    };

    bool IsValidTries(int32 Tries);

    /** A holed ball is inside 3 ft; a shot ended by the safety stop is a miss wherever it lies. */
    ERing Classify(double DistanceCm, bool bHoled, bool bSafetyStop);

    /** One set of tries at a fixed target. Inside6 includes the balls inside 3 ft. */
    struct FTargetSet
    {
        int32 Tries = 3;
        int32 Shots = 0;
        int32 Inside3 = 0;
        int32 Inside6 = 0;
        bool IsComplete() const { return Shots >= Tries; }
    };

    /** The set after one more scored shot; a complete set is returned unchanged. */
    FTargetSet Record(const FTargetSet& Set, ERing Ring);

    /** The ladder's targets: 15 yd, then every 10 yd to 145 yd, then 150 yd. */
    TConstArrayView<int32> LadderYards();

    enum class ELadderStep : uint8
    {
        /** No shot scored yet at this target. */
        Start,
        /** A miss with tries left at this target. */
        Missed,
        /** Inside 3 ft: on to the next target. */
        Advanced,
        /** Every try missed: back one target. */
        DroppedBack,
        /** Every try missed at the first target: try it again. */
        Repeated,
        /** Inside 3 ft at the last target. */
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

    /** The ladder after one scored shot with Tries per target; only a ball inside 3 ft clears a target. */
    FLadderState Next(const FLadderState& State, int32 Tries, bool bInside3);
} // namespace TrainingScoring
