// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "UOpenGolfRange.generated.h"

class UStaticMesh;
class UMaterialInterface;
class UStaticMeshComponent;
class UHierarchicalInstancedStaticMeshComponent;
class UPostProcessComponent;
class UOpenGolfFlagstickComponent;
class AOpenFairwayBall;
class AOpenGolfTerrainArea;

/**
 * Driving range built from the OpenGolfCourse kit: a tee, a fairway strip with rough on both
 * sides, a target flagstick and simple daylight.
 * Ground pieces declare their physics surface through their surface material's physical
 * material: the fairway and fringe play as Fairway, surrounding ground as Rough, and the putting disc as Green.
 * Optional turf and grass are limited to the fairway strip. The tee is on the ground at the
 * actor's origin and the target line is world +X; the turf's mowing stripes assume the range
 * sits at the world origin with no yaw.
 */
UCLASS()
class UOPENGOLF_API AUOpenGolfRange : public AActor
{
    GENERATED_BODY()

public:
    AUOpenGolfRange();
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return bPreviewWindInEditor; }

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "UOpenGolf|Layout")
    void BuildDefaultLayout();

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "UOpenGolf|Presentation")
    void RefreshPresentation();

    const TArray<TObjectPtr<AActor>>& GetLayoutActors() const { return LayoutActors; }

    template <typename T>
    void GetLayoutComponents(TArray<T*>& Out) const
    {
        GetComponents(Out);
        for (const auto& Actor : LayoutActors)
        {
            if (!IsValid(Actor))
                continue;
            TArray<T*> Parts;
            Actor->GetComponents(Parts);
            Out.Append(Parts);
        }
    }

    void ApplyPresentationDefaults(const AUOpenGolfRange& Defaults);

    /** Number of decorative grass clumps, zero when disabled. */
    int32 GetGrassInstanceCount() const;

    /** Number of baked trees on the terrain. The grass switch does not hide them. */
    int32 GetTreeInstanceCount() const;

    /** Where the ball is teed up. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Range")
    FVector GetTeeLocation() const;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "UOpenGolf|Terrain")
    TObjectPtr<AOpenGolfTerrainArea> Terrain;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "UOpenGolf|Terrain")
    void AlignLayoutToTerrain();

    /** Distance from the tee to the green's center (150 yards). */
    static constexpr float GreenDistanceM = 137.16f;

    /** Half width of the fairway strip; turf and grass stay inside it. Rough starts beyond it. */
    static constexpr float FairwayHalfWidthM = 17.0f;

    /** Tag on the fairway, fringe and green pieces that receive GroundMaterialOverride. */
    static const FName TurfTag;

    /** Tag on the rough strips and the backdrop that receive RoughMaterialOverride. */
    static const FName RoughTag;

    /** Tag on the putting green around the flag; no material override replaces it. */
    static const FName GreenTag;

    /** The flag at the green; its cloth flies in the ball's wind. */
    UOpenGolfFlagstickComponent* GetFlagstick() const { return Flagstick; }

    /** Saved wind for the ball and flag. Zero is calm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UOpenGolf|Wind", meta = (DisplayName = "Wind Speed (mph)", ClampMin = "0", UIMax = "40"))
    float WindSpeedMph = 0.0f;

    /** Direction the wind comes from: 0 headwind, 90 from the right, 180 tailwind, 270 from the left. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UOpenGolf|Wind", meta = (DisplayName = "Wind From (degrees)", ClampMin = "0", ClampMax = "360"))
    float WindFromDeg = 0.0f;

    /** Animate the flag while editing. The viewport must have Realtime enabled. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Wind", meta = (DisplayName = "Preview Wind in Editor"))
    bool bPreviewWindInEditor = true;

    /** Show the painted tee box. Turn off where the course ground draws its own tee, as the cut turf does. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation")
    bool bShowPaintedTeeBox = true;

protected:
    virtual void BeginPlay() override;

    /** Empty uses the public clump. Grass is clipped to the fairway strip. Private subclasses own optional content references. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation")
    TArray<TObjectPtr<UStaticMesh>> GrassMeshes;

    /** Applied only to TurfTag pieces (fairway, fringe and green). */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation")
    TObjectPtr<UMaterialInterface> GroundMaterialOverride;

    /** Applied only to RoughTag pieces (rough strips and backdrop). Null keeps the public material. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation")
    TObjectPtr<UMaterialInterface> RoughMaterialOverride;

    /** Fixed exposure compensation in EV for the whole range (auto exposure is off project-wide). */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation")
    float ExposureBiasEV = -0.5f;

    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation", meta = (ClampMin = "5", ClampMax = "200"))
    float GrassSpacingCm = 20.0f;

    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation")
    FVector GrassScale = FVector::OneVector;

    /** Grass covers this radius around the tee, intersected with the fairway strip. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation", meta = (ClampMin = "100", ClampMax = "5000"))
    float GrassRadiusCm = 2000.0f;

    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation", meta = (ClampMin = "0"))
    int32 GrassCullStartCm = 1200;

    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation", meta = (ClampMin = "100"))
    int32 GrassCullEndCm = 2200;

private:
    template <typename T>
    T* AddLayoutComponent(FName Name, const TCHAR* Folder);
    void UpdateGrass();

    UPROPERTY(VisibleInstanceOnly, Category = "UOpenGolf|Layout")
    TArray<TObjectPtr<AActor>> LayoutActors;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> ShapeMaterial;

    UPROPERTY()
    TObjectPtr<UStaticMesh> DefaultGrassMesh;

    /** Applies saved or edited wind. Console changes during Play update the same controls. */
    void ApplyWindSettings();

    /** Writes uopengolf.GreenStimp to the ball when it changed. */
    void ApplyGreenConsoleVariable();

    /** Passes the ball's wind to the flag, when that wind or the air density changed. */
    void SyncFlagToWind();

    UPROPERTY()
    TObjectPtr<UHierarchicalInstancedStaticMeshComponent> Grass;

    /** Unbound range-wide view settings: fixed exposure and no motion blur. */
    UPROPERTY()
    TObjectPtr<UPostProcessComponent> PostProcess;

    UPROPERTY()
    TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassComponents;

    bool bGrassEnabled = false;

    /** The grass switch has been applied to the terrain's vegetation since the last refresh. */
    bool bTerrainGrassApplied = false;

    /** Ground under the tee during Play, found once per range position because it traces the terrain. */
    mutable TOptional<FVector> CachedTee;
    mutable FVector CachedTeeOrigin = FVector::ZeroVector;
    mutable TWeakObjectPtr<AOpenGolfTerrainArea> CachedTeeTerrain;

    /** A ground piece when SurfaceMaterial is set (MI_Fairway or MI_Rough), else a colored decoration. */
    UStaticMeshComponent* AddBox(FName Name, const FVector& MinM, const FVector& MaxM, bool bCollides,
        UMaterialInterface* SurfaceMaterial = nullptr);

    UPROPERTY()
    TObjectPtr<UStaticMesh> CubeMesh;

    /** Course-kit surface materials; each carries the physical material that sets its OpenFairway surface. */
    UPROPERTY()
    TObjectPtr<UMaterialInterface> FairwayMaterial;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> RoughMaterial;

    UPROPERTY()
    TObjectPtr<UOpenGolfFlagstickComponent> Flagstick;

    /** The 20-foot putting green around the flag (SM_GreenDisc with MI_Green). */
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> PuttingGreen;

    bool bFlagWindApplied = false;
    float AppliedWindVarMph = 0.0f;
    float AppliedWindVarFromDeg = 0.0f;
    float AppliedRangeWindMph = 0.0f;
    float AppliedRangeWindFromDeg = 0.0f;
    TWeakObjectPtr<AOpenFairwayBall> AppliedWindBall;
    float AppliedGreenStimpVar = 10.0f;
    FVector AppliedWindMps = FVector::ZeroVector;
    float AppliedAirDensityKgM3 = 0.0f;

    /** Tee paint: fairway turf tinted the tee color at BeginPlay. */
    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> TeeBox;
};
