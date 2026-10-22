// SPDX-License-Identifier: MIT
#pragma once

#include "OpenFairwayBall.h"
#include "UOpenGolfBall.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

/** One camera-facing tracer ribbon; the ball rotates a fixed pool between current and history. */
USTRUCT()
struct FUOpenGolfTrailRibbon
{
    GENERATED_BODY()
    UPROPERTY()
    TObjectPtr<UProceduralMeshComponent> Mesh;
    UPROPERTY()
    TObjectPtr<UMaterialInstanceDynamic> Material;
    TArray<FVector> Points;
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector2D> UVs;
    FVector LastEye = FVector::ZeroVector;
    FLinearColor AppliedColor = FLinearColor::Transparent;
    int32 MeshPointCount = 0;
    float LastWidth = 0;
    float LastMinWidthPerDistance = 0;
    float AppliedOpacity = -1;
    bool bVisible = true;
    int32 ShotNumber = INDEX_NONE;
};

/** Game-owned presentation; the physics plugin stays independent of project assets. */
UCLASS()
class UOPENGOLF_API AUOpenGolfBall : public AOpenFairwayBall
{
    GENERATED_BODY()
public:
    AUOpenGolfBall();
    virtual void Tick(float DeltaSeconds) override;
    void ResetShotTrail();
    void StartShotTrail();
    /** Removes the finished-shot tracers; the auto re-tee keeps them. */
    void ClearShotTrailHistory();
    static constexpr FColor HistoryTracerOrange{255, 165, 0};
    static constexpr int32 MaxHistoryTrails = 2;

    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Tracer", meta = (ClampMin = "0.1"))
    float TrailWidthCm = 3.0f;
    /** Minimum width as a fraction of camera distance, so far parts stay visible (0.004 is about 2 px at 1080p and 90 degree FOV). */
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Tracer", meta = (ClampMin = "0"))
    float TrailMinWidthPerDistance = 0.004f;
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Tracer", meta = (ClampMin = "0", ClampMax = "1"))
    float TrailOpacity = 0.35f;
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Tracer", meta = (ClampMin = "0.1"))
    float TrailFadeSpeedMps = 5.0f;

    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Tracer")
    FLinearColor HistoryTrailColor = FLinearColor::FromSRGBColor(HistoryTracerOrange);
    UPROPERTY(EditAnywhere, Category = "UOpenGolf|Tracer", meta = (ClampMin = "0", ClampMax = "1"))
    float HistoryTrailOpacity = 0.55f;

    float GetTrailOpacity() const { return CurrentOpacity; }
    int32 GetTrailPointCount() const { return Ribbons[CurrentRibbon].Points.Num(); }
    int32 GetHistoryTrailCount() const { return HistoryRibbons.Num(); }
    /** Shot number of a history tracer, newest first. */
    int32 GetHistoryTrailShotNumber(int32 Index) const;

protected:
    virtual void BeginPlay() override;

private:
    void ClearRibbon(FUOpenGolfTrailRibbon& Ribbon);
    void ApplyRibbonLook(FUOpenGolfTrailRibbon& Ribbon, const FLinearColor& Color, float Opacity);
    void PromoteCurrentToHistory();
    void UpdateTrailGeometry(FUOpenGolfTrailRibbon& Ribbon);
#if WITH_DEV_AUTOMATION_TESTS
    void UpdateTrailGeometryLegacy(FUOpenGolfTrailRibbon& Ribbon);
#endif
    // One current ribbon plus MaxHistoryTrails; UHT needs a literal size.
    UPROPERTY()
    FUOpenGolfTrailRibbon Ribbons[3];
    static_assert(MaxHistoryTrails + 1 == 3, "Ribbon pool must hold the current tracer and the history");
    int32 CurrentRibbon = 0;
    /** Indices into Ribbons, newest first. */
    TArray<int32, TInlineAllocator<MaxHistoryTrails>> HistoryRibbons;
    float CurrentOpacity = 0.0f;
    float FadeCeiling = 1.0f;
    bool bTrailActive = false;
    int32 TrailShotNumber = INDEX_NONE;
};
