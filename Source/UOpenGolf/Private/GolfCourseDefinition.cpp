// SPDX-License-Identifier: MIT
#include "GolfCourseDefinition.h"
#include "OpenGolfTerrain.h"
#include "Play/OpenGolfLieRules.h"

const FPrimaryAssetType UGolfCourseDefinition::AssetType(TEXT("GolfCourse"));
const FName UGolfCourseDefinition::MenuBundle(TEXT("Menu"));

const UOpenGolfLieRules& UGolfCourseDefinition::GetLieRules() const
{
    return LieRules ? *LieRules : *GetDefault<UOpenGolfLieRules>();
}

EGolfPenalty UGolfCourseDefinition::PenaltyAt(FVector BallLocation) const
{
    const FVector2D Point(BallLocation);
    if (InBoundsCm.Num() >= 3 && !UOpenGolfTerrainDefinition::Contains(InBoundsCm, Point))
        return EGolfPenalty::OutOfBounds;
    for (const auto& Area : Water)
        if (Area.OutlineCm.Num() >= 3 && UOpenGolfTerrainDefinition::Contains(Area.OutlineCm, Point))
            return EGolfPenalty::Water;
    return EGolfPenalty::None;
}

const FGolfCourseTee* FGolfCourseHole::FindTee(FName SetName) const
{
    const FGolfCourseTee* Tee = Tees.FindByPredicate([SetName](const FGolfCourseTee& Candidate) { return Candidate.SetName == SetName; });
    return Tee ? Tee : (Tees.IsEmpty() ? nullptr : &Tees[0]);
}

int32 UGolfCourseDefinition::TeeSetYards(FName SetName) const
{
    int32 Yards = 0;
    for (const FGolfCourseHole& Hole : Layout)
        if (const FGolfCourseTee* Tee = Hole.FindTee(SetName))
            Yards += Tee->PublishedYards;
    return Yards;
}
