// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"

class AOpenGolfTerrainArea;
class UProceduralMeshComponent;
class UMaterialInterface;

namespace GroundBands
{
    struct FBand
    {
        float OuterRadiusCm;
        int32 RadialSteps;
        FLinearColor Color;
        /** A band at zero opacity is a gap: its section has no triangles. */
        float Opacity;
        /** Overrides the material passed to Draw for this band. */
        UMaterialInterface* Material = nullptr;
    };

    /** Just clear of bare ground; turf blades cover anything this low (see the target rings). */
    constexpr float DefaultLiftCm = .6f;

    /**
     * One mesh section per band, each with its own rows. Every vertex carries UV0 (.5, .5) for M_ShotTrail,
     * UV1 (0 on the band's inner row to 1 on its outer row, the band's width in cm) and UV2 (the radial XY direction),
     * which M_TargetRingLine uses to keep thin bands at least a few pixels wide on screen.
     * Samples the whole target before replacing the visible one, so failed traces leave it intact.
     */
    bool Draw(UProceduralMeshComponent& Mesh, AOpenGolfTerrainArea& Terrain, FVector Center,
        UMaterialInterface& Material, TConstArrayView<FBand> Bands, int32 Segments, ECollisionChannel Channel = ECC_Visibility,
        float LiftCm = DefaultLiftCm);
} // namespace GroundBands
