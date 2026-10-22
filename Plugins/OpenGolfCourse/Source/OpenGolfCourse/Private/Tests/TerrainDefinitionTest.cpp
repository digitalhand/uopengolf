// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"
#include "Engine/StaticMesh.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FTerrainDefinitionTest, "UOpenGolf.Course.Terrain.Definition",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FTerrainDefinitionTest::RunTest(const FString& Parameters)
{
    auto* Definition = NewObject<UOpenGolfTerrainDefinition>();
    auto* Rough = NewObject<UOpenGolfSurfacePreset>();
    Rough->Surface = EOpenFairwaySurface::Rough;
    Definition->DefaultSurface = Rough;
    auto* Green = NewObject<UOpenGolfSurfacePreset>();
    Green->Surface = EOpenFairwaySurface::Green;
    FOpenGolfTerrainRegion Region;
    Region.Name = TEXT("Green");
    Region.Surface = Green;
    Region.OutlineM = {{130, -10}, {145, -10}, {145, 10}, {130, 10}};
    Region.SpacingM = 0.5f;
    Definition->Regions.Add(Region);
    FOpenGolfTerrainMound Mound;
    Mound.CenterM = FVector2D(140, 0);
    Mound.RadiusM = FVector2D(20, 15);
    Mound.HeightM = 0.5f;
    Definition->Mounds.Add(Mound);
    FString Error;
    TestTrue(TEXT("A bounded terrain definition is valid"), Definition->Validate(Error));
    TestEqual(TEXT("Tee stays level"), Definition->HeightAt(FVector2D::ZeroVector), 0.0);
    TestTrue(TEXT("The green has authored elevation"), Definition->HeightAt(FVector2D(140, 0)) > 0.49);
    TestEqual(TEXT("Green surface inside its outline"), Definition->SurfaceAt(FVector2D(137, 0)), Green);
    TestEqual(TEXT("Rough outside the outline"), Definition->SurfaceAt(FVector2D(160, 0)), Rough);
    TestTrue(TEXT("Boundary is owned by the region"), Definition->Contains(Region.OutlineM, FVector2D(130, 0)));
    const FTransform Placement(FRotator(0, 90, 0), FVector(10000, -5000, 200));
    const FVector Local(13700, 400, Definition->HeightAt(FVector2D(137, 4)) * 100);
    const FVector RoundTrip = Placement.InverseTransformPosition(Placement.TransformPosition(Local));
    TestTrue(TEXT("A moved and rotated definition retains local coordinates"), Local.Equals(RoundTrip, 0.001));
    Definition->Regions[0].OutlineM = {{130, -10}, {145, 10}, {130, 10}, {145, -10}};
    TestFalse(TEXT("Crossing outlines are rejected before generation"), Definition->Validate(Error));
    Definition->Regions[0].OutlineM = Region.OutlineM;
    Definition->Mounds[0].RadiusM.X = 0;
    TestFalse(TEXT("Zero mound radius is rejected"), Definition->Validate(Error));
    Definition->Mounds[0].RadiusM.X = 20;
    Definition->DetailSpacingM = 0;
    TestFalse(TEXT("Zero sample spacing is rejected"), Definition->Validate(Error));
    Definition->DetailSpacingM = 3;

    auto* Pines = NewObject<UOpenGolfVegetationPreset>();
    Pines->Kind = EOpenGolfVegetationKind::Tree;
    FOpenGolfVegetationArea Belt;
    Belt.Name = TEXT("RightBelt");
    Belt.Preset = Pines;
    // Areas may extend past the detailed area onto the Landscape.
    Belt.OutlineM = {{-40, 90}, {420, 90}, {420, 200}, {-40, 200}};
    Definition->VegetationAreas.Add(Belt);
    TestTrue(TEXT("A tree area reaching onto the Landscape is valid"), Definition->Validate(Error));
    Definition->VegetationAreas[0].OutlineM[2] = FVector2D(420, 2000);
    TestFalse(TEXT("A vegetation area outside the Landscape is rejected"), Definition->Validate(Error));
    Definition->VegetationAreas[0].OutlineM = Belt.OutlineM;
    Definition->VegetationAreas[0].Name = NAME_None;
    TestFalse(TEXT("An unnamed vegetation area is rejected"), Definition->Validate(Error));
    Definition->VegetationAreas[0].Name = Belt.Name;
    Definition->VegetationAreas[0].Preset = nullptr;
    TestFalse(TEXT("A vegetation area without a preset is rejected"), Definition->Validate(Error));
    Definition->VegetationAreas[0].Preset = Pines;
    Pines->TrunkColliderRadiusM = 0;
    TestFalse(TEXT("A tree without a trunk collider is rejected"), Definition->Validate(Error));
    Pines->TreeCollisionMesh = NewObject<UStaticMesh>();
    TestTrue(TEXT("A wood mesh replaces the cylinder dimensions"), Definition->Validate(Error));
    Pines->Meshes.Add(NewObject<UStaticMesh>());
    Pines->Meshes.Add(NewObject<UStaticMesh>());
    TestFalse(TEXT("One wood proxy cannot stand in for different tree shapes"), Definition->Validate(Error));
    Pines->Meshes.Reset();
    Pines->TreeCollisionMesh = nullptr;
    Pines->TrunkColliderRadiusM = 0.25f;
    Pines->TreeCanopyMesh = NewObject<UStaticMesh>();
    TestTrue(TEXT("A canopy volume works with the cylinder collider"), Definition->Validate(Error));
    Pines->Meshes.Add(NewObject<UStaticMesh>());
    Pines->Meshes.Add(NewObject<UStaticMesh>());
    TestFalse(TEXT("One canopy volume cannot stand in for different tree shapes"), Definition->Validate(Error));
    Pines->Meshes.Reset();
    Pines->TreeCanopyMesh = nullptr;
    Pines->MinimumDistanceFromPlayM = 20;
    Pines->MaximumDistanceFromPlayM = 10;
    TestFalse(TEXT("An empty distance band is rejected"), Definition->Validate(Error));
    Pines->MaximumDistanceFromPlayM = 0;
    TestTrue(TEXT("A minimum distance alone is valid"), Definition->Validate(Error));
    TestEqual(TEXT("Distance from play inside the green"), Definition->DistanceFromPlay(FVector2D(137, 0)), 0.0);
    TestEqual(TEXT("Distance from play beside the green"), Definition->DistanceFromPlay(FVector2D(137, 30)), 20.0, 0.001);
    return true;
}
#endif
