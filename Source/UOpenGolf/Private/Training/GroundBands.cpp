// SPDX-License-Identifier: MIT
#include "Training/GroundBands.h"
#include "OpenGolfTerrain.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
    struct FBandMesh
    {
        TArray<FVector> Vertices, Normals;
        TArray<FVector2D> UV0, UV1, UV2;
        TArray<int32> Triangles;
    };
} // namespace

bool GroundBands::Draw(UProceduralMeshComponent& Mesh, AOpenGolfTerrainArea& Terrain, FVector Center, UMaterialInterface& Material,
    TConstArrayView<FBand> Bands, int32 Segments, ECollisionChannel Channel, float LiftCm)
{
    TArray<FBandMesh> Sections;
    float Inner = 0.f;
    for (const FBand& Band : Bands)
    {
        FBandMesh& Section = Sections.AddDefaulted_GetRef();
        const float Width = Band.OuterRadiusCm - Inner;
        for (int32 Step = 0; Band.Opacity > 0 && Step <= Band.RadialSteps; ++Step)
        {
            const float T = float(Step) / Band.RadialSteps;
            const float Radius = FMath::Lerp(Inner, Band.OuterRadiusCm, T);
            for (int32 I = 0; I < Segments; ++I)
            {
                const float Angle = 2 * PI * I / Segments;
                const FVector2D Radial(FMath::Cos(Angle), FMath::Sin(Angle));
                FVector Ground, Normal;
                if (!Terrain.GroundAt(Center + FVector(Radial, 0) * Radius, Ground, Normal, Channel))
                    return false;
                Section.Vertices.Add(Ground + Normal * LiftCm);
                Section.Normals.Add(Normal);
                Section.UV0.Emplace(.5f, .5f);
                Section.UV1.Emplace(T, Width);
                Section.UV2.Add(Radial);
                if (Step > 0)
                {
                    const int32 A = (Step - 1) * Segments + I;
                    const int32 B = (Step - 1) * Segments + (I + 1) % Segments;
                    Section.Triangles.Append({A, A + Segments, B, B, A + Segments, B + Segments});
                }
            }
        }
        Inner = Band.OuterRadiusCm;
    }
    Mesh.SetWorldTransform(FTransform::Identity);
    for (int32 Index = 0; Index < Sections.Num(); ++Index)
    {
        const FBandMesh& Section = Sections[Index];
        Mesh.CreateMeshSection(Index, Section.Vertices, Section.Triangles, Section.Normals, Section.UV0, Section.UV1, Section.UV2,
            TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
        UMaterialInterface* BandMaterial = Bands[Index].Material ? Bands[Index].Material : &Material;
        auto* Tint = Cast<UMaterialInstanceDynamic>(Mesh.GetMaterial(Index));
        if (!Tint || Tint->Parent != BandMaterial)
            Tint = Mesh.CreateDynamicMaterialInstance(Index, BandMaterial);
        Tint->SetVectorParameterValue(TEXT("Color"), Bands[Index].Color);
        Tint->SetScalarParameterValue(TEXT("Opacity"), Bands[Index].Opacity);
    }
    return true;
}
