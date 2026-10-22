// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "OpenFairwayTypes.h"
#include "OpenGolfTerrain.generated.h"

class UMaterialInterface;
class UStaticMesh;

UENUM(BlueprintType)
enum class EOpenGolfVegetationKind : uint8
{
    /** Grass and low plants: tilted to the slope, no shadows, no collision. */
    GroundCover,
    /** Upright trees that cast shadows. Each gets a hidden trunk collider that the ball deflects off. */
    Tree
};

UENUM(BlueprintType)
enum class EOpenGolfPlacementPattern : uint8
{
    JitteredGrid,
    RandomSeparated,
    /** Seamless square tiles on a grid aligned to the terrain, halved near play edges and on curved ground. */
    Tiled
};

/** One footprint size of a tiled preset, with the meshes that fill exactly that square. */
USTRUCT(BlueprintType)
struct OPENGOLFCOURSE_API FOpenGolfGrassTile
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation", meta = (ClampMin = "0.05", Units = "m"))
    float SizeM = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    TArray<TObjectPtr<UStaticMesh>> Meshes;
};

UCLASS(BlueprintType)
class OPENGOLFCOURSE_API UOpenGolfSurfacePreset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
    EOpenFairwaySurface Surface = EOpenFairwaySurface::Rough;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Surface")
    TObjectPtr<UMaterialInterface> Material;
};

UCLASS(BlueprintType)
class OPENGOLFCOURSE_API UOpenGolfVegetationPreset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation")
    TArray<TObjectPtr<UStaticMesh>> Meshes;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation")
    EOpenGolfVegetationKind Kind = EOpenGolfVegetationKind::GroundCover;

    /** Optional wood-only mesh, placed at the visible tree's exact transform. Otherwise uses a cylinder. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation|Tree", meta = (EditCondition = "Kind == EOpenGolfVegetationKind::Tree"))
    TObjectPtr<UStaticMesh> TreeCollisionMesh;

    /** Trunk collider radius at instance scale 1. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation|Tree", meta = (ClampMin = "0.05", Units = "m", EditCondition = "Kind == EOpenGolfVegetationKind::Tree"))
    float TrunkColliderRadiusM = 0.25f;

    /** Trunk collider height above the ground at instance scale 1. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation|Tree", meta = (ClampMin = "0.5", Units = "m", EditCondition = "Kind == EOpenGolfVegetationKind::Tree"))
    float TrunkColliderHeightM = 7.0f;

    /** Closest placement to fairway, fringe, green or sand outlines. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation", meta = (ClampMin = "0", Units = "m"))
    float MinimumDistanceFromPlayM = 0;

    /** Farthest placement from fairway, fringe, green or sand outlines; 0 has no limit. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation", meta = (ClampMin = "0", Units = "m"))
    float MaximumDistanceFromPlayM = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation")
    EOpenFairwaySurface Surface = EOpenFairwaySurface::Rough;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation", meta = (ClampMin = "0.1", Units = "m"))
    float SpacingM = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation")
    EOpenGolfPlacementPattern PlacementPattern = EOpenGolfPlacementPattern::JitteredGrid;

    /** Random placement keeps this distance between plants within the same preset or area. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation", meta = (ClampMin = "0.1", Units = "m", EditCondition = "PlacementPattern == EOpenGolfPlacementPattern::RandomSeparated"))
    float MinimumSeparationM = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation")
    FVector2D ScaleRange = FVector2D(0.8, 1.3);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation", meta = (ClampMin = "0", ClampMax = "60", Units = "deg"))
    float MaximumSlopeDeg = 30;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation", meta = (ClampMin = "0", Units = "m"))
    float BoundaryClearanceM = 0.5f;

    /** Ground cover fades out from 75% of this distance. Trees are never culled, so they always match their colliders. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation", meta = (ClampMin = "1", Units = "m"))
    float CullDistanceM = 100;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation")
    int32 Seed = 1731;

    /** Tiled pattern: largest first, each half the size of the one before (for example 1, 0.5, 0.25 m). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation|Tiles", meta = (EditCondition = "PlacementPattern == EOpenGolfPlacementPattern::Tiled"))
    TArray<FOpenGolfGrassTile> Tiles;

    /** A tile is halved when the ground normal across it turns more than this, so tiles hug curved ground. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation|Tiles", meta = (ClampMin = "0", ClampMax = "45", Units = "deg", EditCondition = "PlacementPattern == EOpenGolfPlacementPattern::Tiled"))
    float MaximumTileTiltDeg = 6;

    /** Tiles sit this far into the ground so their edges never float on uneven ground. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation|Tiles", meta = (ClampMin = "0", ClampMax = "0.2", Units = "m", EditCondition = "PlacementPattern == EOpenGolfPlacementPattern::Tiled"))
    float TileSinkM = 0.02f;

    /** Boundary grass: strip footprint, X along the play edge and Y into the rough. Its inner edge sits at BoundaryClearanceM. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vegetation|Boundary", meta = (Units = "m"))
    FVector2D EdgeStripSizeM = FVector2D(1, 0.25);
};

USTRUCT(BlueprintType)
struct OPENGOLFCOURSE_API FOpenGolfTerrainMound
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contour", meta = (Units = "m"))
    FVector2D CenterM = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contour", meta = (Units = "m"))
    FVector2D RadiusM = FVector2D(20, 20);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Contour", meta = (Units = "m"))
    float HeightM = 1;
};

USTRUCT(BlueprintType)
struct OPENGOLFCOURSE_API FOpenGolfTerrainRegion
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    FName Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    TObjectPtr<UOpenGolfSurfacePreset> Surface;
    // Closed outline in terrain-local meters. Later regions take precedence.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region")
    TArray<FVector2D> OutlineM;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Region", meta = (ClampMin = "0.1", Units = "m"))
    float SpacingM = 1;
};

USTRUCT(BlueprintType)
struct OPENGOLFCOURSE_API FOpenGolfVegetationArea
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    FName Name;
    // Closed outline in terrain-local meters, anywhere inside the Landscape bounds.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    TArray<FVector2D> OutlineM;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vegetation")
    TObjectPtr<UOpenGolfVegetationPreset> Preset;
};

/** One scattered vegetation instance: world transform and a seed for mesh selection. */
struct FOpenGolfScatterPoint
{
    FTransform Transform;
    int32 Seed = 0;
    /** Index into the preset's Tiles for tiled placement; otherwise none, and the preset's Meshes apply. */
    int32 Tile = INDEX_NONE;
};

UCLASS(BlueprintType)
class OPENGOLFCOURSE_API UOpenGolfTerrainDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (Units = "m"))
    FVector2D MinimumM = FVector2D(-1260, -1134);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (Units = "m"))
    FVector2D MaximumM = FVector2D(1260, 1134);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (Units = "m"))
    FVector2D DetailMinimumM = FVector2D(-252, -126);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (Units = "m"))
    FVector2D DetailMaximumM = FVector2D(504, 126);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.25", Units = "m"))
    float LandscapeSpacingM = 4;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0.25", Units = "m"))
    float DetailSpacingM = 3;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    TObjectPtr<UOpenGolfSurfacePreset> DefaultSurface;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    TArray<FOpenGolfTerrainRegion> Regions;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    TArray<FOpenGolfTerrainMound> Mounds;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (Units = "m"))
    FVector2D LevelTeeCenterM = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain", meta = (ClampMin = "0", Units = "m"))
    float LevelTeeRadiusM = 3;
    // Scattered across the detailed area.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    TArray<TObjectPtr<UOpenGolfVegetationPreset>> Vegetation;
    // Scattered inside their own outlines, which may extend onto the Landscape.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    TArray<FOpenGolfVegetationArea> VegetationAreas;

    /** Highest number of instances one preset or area may place. */
    static constexpr int32 MaxScatterPoints = 100000;

    /** Ground lookup for scattering: terrain-local meters in, world ground position and normal out. */
    using FGroundQuery = TFunctionRef<bool(FVector2D PointM, FVector& GroundPosition, FVector& GroundNormal)>;

    /**
     * Seeded grid, separated random or tiled placement over AreaOutlineM (or the detailed area when null),
     * filtered by surface, tee, boundary clearance, distance from play and slope. Trees stand upright; ground
     * cover follows the ground normal.
     */
    bool Scatter(const UOpenGolfVegetationPreset& Preset, const TArray<FVector2D>* AreaOutlineM, FGroundQuery Ground,
        TArray<FOpenGolfScatterPoint>& OutPoints, FString& Error, double TerrainYawDeg = 0) const;

    /**
     * A continuous row of edge strips along the rough side of every play outline (fairway, fringe, green, tee, sand),
     * turned to follow the edge. Strips overlap slightly so bends leave no gap.
     */
    bool ScatterBoundary(const UOpenGolfVegetationPreset& Preset, FGroundQuery Ground, TArray<FOpenGolfScatterPoint>& OutPoints,
        FString& Error, double TerrainYawDeg = 0) const;

    bool Validate(FString& Error) const;
    double HeightAt(FVector2D PointM) const;
    FVector NormalAt(FVector2D PointM) const;
    UOpenGolfSurfacePreset* SurfaceAt(FVector2D PointM) const;
    /** Distance to the nearest region that is not rough; infinite when there is none. */
    double DistanceFromPlay(FVector2D PointM) const;
    static bool Contains(const TArray<FVector2D>& Outline, FVector2D Point);
    static double DistanceToOutline(const TArray<FVector2D>& Outline, FVector2D Point);

private:
    /**
     * Tiled placement, after Scatter has validated the preset: every grid cell whose whole footprint lies on the
     * preset's surface and BoundaryClearanceM from every region edge and the tee gets one tile. Other cells are halved
     * down to the smallest tile, which needs only its centre to pass. Cells on curved ground are halved too. The
     * distance band is judged at cell centres, so its outer edge stays soft. Yaw turns in 90 degree steps.
     */
    bool ScatterTiles(const UOpenGolfVegetationPreset& Preset, const TArray<FVector2D>* AreaOutlineM, FGroundQuery Ground,
        TArray<FOpenGolfScatterPoint>& OutPoints, FString& Error, double TerrainYawDeg) const;
};

UCLASS(BlueprintType)
class OPENGOLFCOURSE_API AOpenGolfTerrainArea : public AActor
{
    GENERATED_BODY()
public:
    AOpenGolfTerrainArea();

    /** Every actor a build creates. */
    static const FName GeneratedTag;
    /** Baked ground cover, hidden by the range's 3D grass switch. */
    static const FName VegetationTag;
    /** Baked tree meshes. */
    static const FName TreesTag;
    /** Hidden trunk colliders, one per tree, also tagged as ball obstacles. */
    static const FName TreeCollidersTag;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain")
    TObjectPtr<UOpenGolfTerrainDefinition> Definition;
    /** Course grass layer: ground-cover presets (for example dense tiles near play, sparse beyond), rebuilt on their own. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Course grass")
    TArray<TObjectPtr<UOpenGolfVegetationPreset>> CourseGrassPresets;
    /** Course grass layer: edge strips along every play outline. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Terrain|Course grass")
    TObjectPtr<UOpenGolfVegetationPreset> BoundaryGrassPreset;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
    TArray<TObjectPtr<AActor>> GeneratedActors;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Terrain")
    FGuid GenerationId;

    /** Line trace for the ground beneath WorldPosition. Ball obstacles such as trunk colliders are ignored. */
    UFUNCTION(BlueprintCallable, Category = "Terrain")
    bool GroundAt(FVector WorldPosition, FVector& GroundPosition, FVector& GroundNormal) const;
};
