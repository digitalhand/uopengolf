// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "HoleMapFrame.h"

#include "UOpenGolfRange.generated.h"

class UPostProcessComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UOpenGolfCupComponent;
class UOpenGolfFlagstickComponent;
class AOpenFairwayBall;
class AOpenGolfTerrainArea;

/**
 * Controls daylight, the flag, wind, green speed and grass visibility.
 * The terrain supplies ground and physical surfaces. The tee is at this actor's origin;
 * the target line follows world +X.
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

    /** Adds daylight, the view settings and the flagstick as separate actors once; existing layout actors are kept. */
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

    /** Visible 3D grass and ground-cover instances on the terrain; zero while the grass switch is off. */
    int32 GetGrassInstanceCount() const;

    /** Where the ball is teed up. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Range")
    FVector GetTeeLocation() const;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "UOpenGolf|Terrain")
    TObjectPtr<AOpenGolfTerrainArea> Terrain;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "UOpenGolf|Terrain")
    void AlignLayoutToTerrain();

    /** Hole map framing: ground shown beyond the tee and behind the cup. The tee-to-cup direction is map up. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Minimap", meta = (ClampMin = "0", Units = "cm"))
    float MapEndMargin = 3200;

    /** Hole map framing: the narrowest ground shown across the hole. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Minimap", meta = (ClampMin = "100", Units = "cm"))
    float MapMinimumWidth = 18000;

    /** Hole map framing: shifts the picture right (positive) or left of the tee-to-cup line. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Minimap", meta = (Units = "cm"))
    float MapSideOffset = 0;

    /**
     * The top-down picture of the hole that the HUD minimap draws; null before the first capture and without a renderer
     * (NullRHI). Captured once the scene has finished loading (and, in editor builds, compiling its textures, meshes and
     * shaders), so a picture taken just after level travel never shows placeholder textures; again after
     * InvalidateHoleMap.
     */
    UTextureRenderTarget2D* GetHoleMapTexture() const { return HoleMapTexture; }

    /** The frame the hole map was captured in: maps world positions to the picture's UV and back. */
    const FHoleMapFrame& GetHoleMapFrame() const { return HoleMapFrame; }

    /** Recaptures the hole map once the scene is ready, after a change to what it shows (grass, layout). */
    void InvalidateHoleMap();

    /**
     * Longest wait (seconds) for the scene to be ready before the hole map is captured anyway, so the minimap is never
     * missing for long; it is captured again once the scene is ready.
     */
    static constexpr double HoleMapReadyTimeoutS = 10.0;

    /** Distance from the tee to the pin (400 yards), where a new layout places the flag. */
    static constexpr float HoleDistanceM = 365.76f;

    /** The hole is a par 4. */
    static constexpr int32 Par = 4;

    /** The flag at the green; its cloth flies in the ball's wind. */
    UOpenGolfFlagstickComponent* GetFlagstick() const { return Flagstick; }

    /** The cup at the flag's foot, placed there when Play starts; it lifts the flag for a nearby ball. */
    UOpenGolfCupComponent* GetCup() const { return Cup; }

    /** The cup on the green's surface; the flag's foot before Play, the tee when there is no flag. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Range")
    FVector GetCupLocation() const;

    /** Saved wind at 10 m for the ball, flag and vegetation. Zero is calm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UOpenGolf|Wind", meta = (DisplayName = "Wind Speed (mph)", ClampMin = "0", UIMax = "40"))
    float WindSpeedMph = 0.0f;

    /** Direction the wind comes from: 0 headwind, 90 from the right, 180 tailwind, 270 from the left. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UOpenGolf|Wind", meta = (DisplayName = "Wind From (degrees)", ClampMin = "0", ClampMax = "360"))
    float WindFromDeg = 0.0f;

    /**
     * Applies wind immediately so the next shot uses it. A moving shot keeps its captured wind.
     */
    void SetWind(float SpeedMph, float FromDeg);

    /** Animate the flag and vegetation while editing. The viewport must have Realtime enabled. */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Wind", meta = (DisplayName = "Preview Wind in Editor"))
    bool bPreviewWindInEditor = true;

protected:
    virtual void BeginPlay() override;

    /** Fixed exposure compensation in EV for the whole range (auto exposure is off project-wide). */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Presentation")
    float ExposureBiasEV = -0.5f;

private:
    template <typename T>
    T* AddLayoutComponent(FName Name, const TCHAR* Folder);
    void UpdateGrass();

    /** Captures a stale hole map when the scene is ready, or an interim one when it has waited too long. */
    void UpdateHoleMap();

    /** Frames the hole and renders it top-down into HoleMapTexture, once; the scene is static during Play. */
    void CaptureHoleMap();

    /** Nothing the capture draws is still loading or, in editor builds, compiling (placeholders would be captured). */
    static bool IsSceneReadyForHoleMap();

    /** Created on the first capture; it never captures on its own (no per-frame cost). */
    UPROPERTY(Transient)
    TObjectPtr<USceneCaptureComponent2D> HoleMapCapture;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> HoleMapTexture;

    FHoleMapFrame HoleMapFrame;
    /** The picture is missing or out of date; captured by UpdateHoleMap. */
    bool bHoleMapDirty = false;
    /** The current picture was taken at the time limit, before the scene was ready. */
    bool bHoleMapInterim = false;
    /** Platform time when the picture went stale. */
    double HoleMapStaleSince = 0.0;

    UPROPERTY(VisibleInstanceOnly, Category = "UOpenGolf|Layout")
    TArray<TObjectPtr<AActor>> LayoutActors;

    /** Applies saved or edited wind. Console changes during Play update the same controls. */
    void ApplyWindSettings();

    /** Writes uopengolf.GreenStimp to the ball when it changed. */
    void ApplyGreenConsoleVariable();

    /** Publishes the ball's wind to the world's wind subsystem, and to the flag when it or the air density changed. */
    void SyncWindToScene();

    /** Unbound range-wide view settings: fixed exposure and no motion blur. */
    UPROPERTY()
    TObjectPtr<UPostProcessComponent> PostProcess;

    bool bGrassEnabled = false;

    /** The grass switch has been applied to the terrain's vegetation since the last refresh. */
    bool bTerrainGrassApplied = false;

    /** Ground under the tee during Play, found once per range position because it traces the terrain. */
    mutable TOptional<FVector> CachedTee;
    mutable FVector CachedTeeOrigin = FVector::ZeroVector;
    mutable TWeakObjectPtr<AOpenGolfTerrainArea> CachedTeeTerrain;

    UPROPERTY()
    TObjectPtr<UOpenGolfFlagstickComponent> Flagstick;

    UPROPERTY(VisibleAnywhere, Category = "UOpenGolf|Range")
    TObjectPtr<UOpenGolfCupComponent> Cup;

    bool bFlagWindApplied = false;
    float AppliedWindVarMph = 0.0f;
    float AppliedWindVarFromDeg = 0.0f;
    float AppliedRangeWindMph = 0.0f;
    float AppliedRangeWindFromDeg = 0.0f;
    TWeakObjectPtr<AOpenFairwayBall> AppliedWindBall;
    float AppliedGreenStimpVar = 10.0f;
    FVector AppliedWindMps = FVector::ZeroVector;
    float AppliedAirDensityKgM3 = 0.0f;
};
