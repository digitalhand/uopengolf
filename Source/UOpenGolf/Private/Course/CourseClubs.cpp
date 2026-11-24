// SPDX-License-Identifier: MIT
#include "Course/CourseClubs.h"
#include "Play/OpenGolfPlayRules.h"

namespace CourseClubsDetail
{
    using EClub = ELaunchMonitorClub;

    constexpr float MetresPerYard = 0.9144f;

    /**
     * Carry in yards, Driver to Lob Wedge in ELaunchMonitorClub order: TrackMan's PGA Tour averages to the pitching wedge.
     * The 7 wood takes the hybrid's carry; the gap, sand and lob wedges are estimates beyond TrackMan's table.
     * Docs/Playing.md ("Auto club") gives the source.
     */
    constexpr float CarryYards[] = {275, 243, 230, 225, 203, 194, 183, 172, 160, 148, 136, 124, 110, 95};
    static_assert(static_cast<int32>(UE_ARRAY_COUNT(CarryYards)) == static_cast<int32>(EClub::Putter), "One carry for every club but the putter");

    /** Two clubs whose carries miss by amounts this close count as equally near. */
    constexpr float EquallyNearM = 0.01f;
} // namespace CourseClubsDetail

EOpenGolfClubGroup CourseClubs::Group(ELaunchMonitorClub Club)
{
    using namespace CourseClubsDetail;
    switch (Club)
    {
    case EClub::Driver:
    case EClub::Wood3:
    case EClub::Wood5:
    case EClub::Wood7:
        return EOpenGolfClubGroup::Wood;
    case EClub::PitchingWedge:
    case EClub::GapWedge:
    case EClub::SandWedge:
    case EClub::LobWedge:
        return EOpenGolfClubGroup::Wedge;
    case EClub::Putter:
        return EOpenGolfClubGroup::Putter;
    default:
        return EOpenGolfClubGroup::Iron;
    }
}

float CourseClubs::PgaCarryM(ELaunchMonitorClub Club)
{
    using namespace CourseClubsDetail;
    const int32 Index = static_cast<int32>(Club);
    return Index >= 0 && Index < static_cast<int32>(UE_ARRAY_COUNT(CarryYards)) ? CarryYards[Index] * MetresPerYard : 0.0f;
}

ELaunchMonitorClub CourseClubs::Recommend(const FAdvice& Advice)
{
    using namespace CourseClubsDetail;
    const EOpenFairwaySurface Surface = Advice.Surface;
    if (!Advice.bOnTee && (Surface == EOpenFairwaySurface::Green || Surface == EOpenFairwaySurface::Fringe))
        return EClub::Putter;
    if (!Advice.bOnTee && Surface == EOpenFairwaySurface::Sand && OpenGolfPlay::IsShortGame(Advice.DistanceToCupM, false))
        return EClub::SandWedge;

    // Longest first, so of two equally near clubs the longer is kept.
    EClub Best = EClub::LobWedge;
    float BestMissM = TNumericLimits<float>::Max();
    for (int32 Index = 0; Index < static_cast<int32>(EClub::Putter); ++Index)
    {
        const EClub Club = static_cast<EClub>(Index);
        if (Club == EClub::Driver && !Advice.bOnTee)
            continue;
        const FOpenGolfLieCut Cut = Advice.bOnTee || !Advice.Rules ? FOpenGolfLieCut() : Advice.Rules->CutFor(Surface, Advice.DistanceToCupM, Group(Club));
        const float MissM = FMath::Abs(PgaCarryM(Club) * Cut.BallSpeedScale() - Advice.DistanceToCupM);
        if (MissM < BestMissM - EquallyNearM)
        {
            Best = Club;
            BestMissM = MissM;
        }
    }
    return Best;
}
