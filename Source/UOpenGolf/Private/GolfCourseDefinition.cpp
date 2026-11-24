// SPDX-License-Identifier: MIT
#include "GolfCourseDefinition.h"
#include "Play/OpenGolfLieRules.h"

const FPrimaryAssetType UGolfCourseDefinition::AssetType(TEXT("GolfCourse"));
const FName UGolfCourseDefinition::MenuBundle(TEXT("Menu"));

const UOpenGolfLieRules& UGolfCourseDefinition::GetLieRules() const
{
    return LieRules ? *LieRules : *GetDefault<UOpenGolfLieRules>();
}
