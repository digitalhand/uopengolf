// SPDX-License-Identifier: MIT
#pragma once

#include "OpenFairwayTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "OpenFairwayPhysicalMaterial.generated.h"

/**
 * A physical material that declares an OpenFairway surface. Set it on a material (or material
 * instance, or Landscape layer) and every mesh drawn with that material plays as this surface,
 * with no component tags needed. Engine properties such as friction do not affect the ball:
 * OpenFairway computes contact from the surface preset.
 */
UCLASS(BlueprintType)
class OPENFAIRWAYPHYSICS_API UOpenFairwayPhysicalMaterial : public UPhysicalMaterial
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenFairway")
    EOpenFairwaySurface Surface = EOpenFairwaySurface::Fairway;
};
