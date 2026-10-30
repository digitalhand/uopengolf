// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "UOpenGolfRange.generated.h"

class UPostProcessComponent;
class UOpenGolfCupComponent;
class UOpenGolfFlagstickComponent;
class AOpenFairwayBall;
class AOpenGolfTerrainArea;

/**
 * Driving range controller on a course terrain area: daylight, the target flagstick, wind for the ball,
 * flag and the world's vegetation (through UOpenGolfWindSubsystem), green speed, and the 3D grass switch. The terrain provides the ground, whose
 * surfaces declare their physics through physical materials. The tee is on the terrain at the actor's
 * origin and the target line is world +X.
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

    /** Number of baked trees on the terrain. The grass switch does not hide them. */
    int32 GetTreeInstanceCount() const;

    /** Where the ball is teed up. */
    UFUNCTION(BlueprintPure, Category = "UOpenGolf|Range")
    FVector GetTeeLocation() const;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "UOpenGolf|Terrain")
    TObjectPtr<AOpenGolfTerrainArea> Terrain;

    UFUNCTION(CallInEditor, BlueprintCallable, Category = "UOpenGolf|Terrain")
    void AlignLayoutToTerrain();

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
