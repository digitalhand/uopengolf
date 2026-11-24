// SPDX-License-Identifier: MIT
#include "Play/OpenGolfLieRules.h"
#include "Play/OpenGolfPlayRules.h"

namespace OpenGolfLieRulesDetail
{
    FOpenGolfLiePenalty Penalty(const FOpenGolfLieCut& Woods, const FOpenGolfLieCut& Irons, const FOpenGolfLieCut& Wedges)
    {
        FOpenGolfLiePenalty Row;
        Row.Woods = Woods;
        Row.Irons = Irons;
        Row.Wedges = Wedges;
        return Row;
    }

    /** "−N%" in whole percent, or empty when it rounds to nothing. The minus is U+2212, as the HUD's other signs. */
    FString CutText(float Pct)
    {
        const int32 Whole = FMath::RoundToInt(FMath::Clamp(Pct, 0.0f, FOpenGolfLieCut::MaxPct));
        return Whole > 0 ? FString::Printf(TEXT("−%d%%"), Whole) : FString();
    }
} // namespace OpenGolfLieRulesDetail

FOpenGolfLieCut FOpenGolfLiePenalty::ForGroup(EOpenGolfClubGroup Group) const
{
    switch (Group)
    {
    case EOpenGolfClubGroup::Wood: return Woods;
    case EOpenGolfClubGroup::Iron: return Irons;
    case EOpenGolfClubGroup::Wedge: return Wedges;
    default: return FOpenGolfLieCut();
    }
}

UOpenGolfLieRules::UOpenGolfLieRules()
{
    // The standard, provisional until tuned in play: ball speed % / spin % for woods, irons and wedges. No PGA Tour table
    // of lie penalties is published; Docs/Reference.md ("Lie rules") gives the sources these follow.
    Lies.Add(EOpenFairwaySurface::Rough, OpenGolfLieRulesDetail::Penalty({10, 20}, {8, 25}, {6, 35}));
    Lies.Add(EOpenFairwaySurface::Sand, OpenGolfLieRulesDetail::Penalty({15, 25}, {10, 25}, {8, 25}));
    NearGreenLies.Add(EOpenFairwaySurface::Sand, OpenGolfLieRulesDetail::Penalty({40, 30}, {40, 30}, {40, 30}));
}

FOpenGolfLieCut UOpenGolfLieRules::CutFor(EOpenFairwaySurface Surface, float DistanceToCupM, EOpenGolfClubGroup Group) const
{
    const FOpenGolfLiePenalty* NearGreen = OpenGolfPlay::IsShortGame(DistanceToCupM, false) ? NearGreenLies.Find(Surface) : nullptr;
    const FOpenGolfLiePenalty* Row = NearGreen ? NearGreen : Lies.Find(Surface);
    return Row ? Row->ForGroup(Group) : FOpenGolfLieCut();
}

FOpenFairwayLaunch OpenGolfPlay::ApplyLieCut(const FOpenFairwayLaunch& Launch, const FOpenGolfLieCut& Cut)
{
    FOpenFairwayLaunch Played = Launch;
    Played.BallSpeedMph *= Cut.BallSpeedScale();
    // A spin that was not measured stays missing; the physics derives it from the cut values.
    const float Spin = Cut.SpinScale();
    if (Played.bHasBackSpin)
        Played.BackSpinRpm *= Spin;
    if (Played.bHasSideSpin)
        Played.SideSpinRpm *= Spin;
    if (Played.bHasTotalSpin)
        Played.TotalSpinRpm *= Spin;
    return Played;
}

FString OpenGolfPlay::LieCutLabel(const FOpenGolfLieCut& Cut)
{
    return OpenGolfLieRulesDetail::CutText(Cut.BallSpeedPct);
}

FString OpenGolfPlay::SpinCutLabel(const FOpenGolfLieCut& Cut)
{
    const FString Text = OpenGolfLieRulesDetail::CutText(Cut.SpinPct);
    return Text.IsEmpty() ? Text : TEXT("Spin ") + Text;
}
