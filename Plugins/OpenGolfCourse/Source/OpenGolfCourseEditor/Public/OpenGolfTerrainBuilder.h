// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenGolfTerrainBuilder.generated.h"

class AOpenGolfTerrainArea;
class UStaticMesh;

UCLASS()
class OPENGOLFCOURSEEDITOR_API UOpenGolfTerrainBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Saves new assets first. Existing generated actors are replaced only after a successful build.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool BuildTerrain(AOpenGolfTerrainArea* Area, const FString& OutputFolder, bool ReplaceGenerated, FString& Error);

    // Rebuilds only the detailed ground meshes from the current definition, for example after its outlines change.
    // Each generated ground actor keeps its components, material overrides and collision; vegetation, trees,
    // Landscapes and course grass are untouched. Nothing changes unless every surface mesh builds and saves.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool RebuildSurfaceMeshes(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error);

    // The surface index BuildTerrain gave a generated ground actor's mesh (SM_Surface_<index>), or INDEX_NONE.
    static int32 SurfaceMeshIndex(const AActor& Actor);

    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool StartVegetation(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error);

    // Removes only this area's vegetation, trees and trunk colliders, then starts vegetation again.
    // Terrain, tee, targets and the course grass layer are untouched. Bake afterwards as usual.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool RebuildVegetation(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error);

    // True for a generated actor that RebuildVegetation removes and places again. Course grass is
    // excluded: only RebuildCourseGrass places it.
    static bool IsReplacedByVegetationRebuild(const AActor& Actor);

    // Places the area's course grass layer (CourseGrassPresets plus boundary strips) through PCG. Other vegetation is kept.
    // BakeVegetation then replaces only the previous course grass layer; a failed or empty run keeps it.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool RebuildCourseGrass(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error);

    // Call after PCG finishes. Baked actors load in packaged play without running PCG.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool BakeVegetation(AOpenGolfTerrainArea* Area, FString& Error);

    // Replaces the hidden canopy volumes of every baked tree that uses TreeMesh with CanopyMesh (one convex hull in
    // simple collision) at each tree's transform. Trees, colliders and other vegetation are untouched. Count is the
    // number of volumes placed; on failure the existing canopy is kept and the error is logged.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool RebuildTreeCanopy(AOpenGolfTerrainArea* Area, UStaticMesh* TreeMesh, UStaticMesh* CanopyMesh, int32& Count, FString& Error);

    // An empty ground-cover actor owned by the area, for components placed by scripts (for example
    // understory). It is tagged and registered like baked vegetation, so the grass switch hides it
    // and the next vegetation rebuild removes it. Returns null, with a logged error, on failure.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static AActor* AddVegetationHost(AOpenGolfTerrainArea* Area, const FString& Label);
};
