// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenGolfTerrainBuilder.generated.h"

class AOpenGolfTerrainArea;

UCLASS()
class OPENGOLFCOURSEEDITOR_API UOpenGolfTerrainBuilder : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Saves new assets first. Existing generated actors are replaced only after a successful build.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool BuildTerrain(AOpenGolfTerrainArea* Area, const FString& OutputFolder, bool ReplaceGenerated, FString& Error);

    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool StartVegetation(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error);

    // Removes only this area's vegetation, trees and trunk colliders, then starts vegetation again.
    // Terrain, tee and targets are untouched. Bake afterwards as usual.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool RebuildVegetation(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error);

    // Places the area's course grass layer (CourseGrassPresets plus boundary strips) through PCG. Other vegetation is kept.
    // BakeVegetation then replaces only the previous course grass layer; a failed or empty run keeps it.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool RebuildCourseGrass(AOpenGolfTerrainArea* Area, const FString& OutputFolder, FString& Error);

    // Call after PCG finishes. Baked actors load in packaged play without running PCG.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static bool BakeVegetation(AOpenGolfTerrainArea* Area, FString& Error);

    // An empty ground-cover actor owned by the area, for components placed by scripts (for example
    // understory). It is tagged and registered like baked vegetation, so the grass switch hides it
    // and the next vegetation rebuild removes it. Returns null, with a logged error, on failure.
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Terrain")
    static AActor* AddVegetationHost(AOpenGolfTerrainArea* Area, const FString& Label);
};
