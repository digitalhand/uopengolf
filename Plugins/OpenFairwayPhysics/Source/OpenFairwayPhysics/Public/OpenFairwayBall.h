// SPDX-License-Identifier: MIT
#pragma once

#include "GameFramework/Actor.h"
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairwayTypes.h"
#include "OpenFairwayBall.generated.h"

class UStaticMeshComponent;
struct FHitResult;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOpenFairwayBallLanded, const FOpenFairwayShotResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOpenFairwayBallAtRest, const FOpenFairwayShotResult&, Result);

/**
 * A golf ball driven by the OpenFairway core. Unreal (Chaos) is used the way ogolf-physics
 * uses Jolt: sphere sweeps and line traces find contacts, normals and surfaces, while flight,
 * bounce, roll and rest come from the calibrated OpenFairway equations at 120 Hz. The ball
 * does not simulate as a Chaos rigid body, so the two never compete for the same motion.
 */
UCLASS(Blueprintable)
class OPENFAIRWAYPHYSICS_API AOpenFairwayBall : public AActor
{
    GENERATED_BODY()

public:
    AOpenFairwayBall();

    /** Launches from the current position, snapped to the ground beneath it. Returns false for invalid launch data. */
    UFUNCTION(BlueprintCallable, Category = "OpenFairway")
    bool Launch(const FOpenFairwayLaunch& LaunchData);

    /** Stops the ball and places it on the ground at Location. */
    UFUNCTION(BlueprintCallable, Category = "OpenFairway")
    void PlaceAt(const FVector& Location);

    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    EOpenFairwayBallState GetBallState() const { return BallState; }

    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    bool IsMoving() const { return BallState != EOpenFairwayBallState::Rest; }

    /** The current or last shot; false before the first launch. */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    bool GetShotResult(FOpenFairwayShotResult& OutResult) const;

    /** Ball speed in m/s. */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    float GetSpeed() const { return Velocity.Size(); }

    /** The surface for a hit: a "OpenFairway.Surface.<Name>" tag on the component, then its actor, else DefaultSurface. */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    EOpenFairwaySurface ResolveSurface(const UPrimitiveComponent* Component) const;

    /** The tag that assigns a surface, such as "OpenFairway.Surface.Green". */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    static FName GetSurfaceTag(EOpenFairwaySurface Surface);

    /** Headless prediction of a launch on flat ground of one surface, using this ball's air and profile. */
    OpenFairway::FShotSimulationResult PredictShot(const FOpenFairwayLaunch& LaunchData, EOpenFairwaySurface Surface) const;

    static OpenFairway::FLaunchData ToLaunchData(const FOpenFairwayLaunch& LaunchData);

    /** Ball profile (calibrated defaults); changes apply from the next launch. */
    OpenFairway::FBallPhysicsProfile Profile;

    UPROPERTY(BlueprintAssignable, Category = "OpenFairway")
    FOnOpenFairwayBallLanded OnBallLanded;

    UPROPERTY(BlueprintAssignable, Category = "OpenFairway")
    FOnOpenFairwayBallAtRest OnBallAtRest;

    /** Surface used where no surface tag is found. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    EOpenFairwaySurface DefaultSurface = EOpenFairwaySurface::Fairway;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Air")
    float TemperatureF = 75.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Air")
    float AltitudeFt = 0.0f;

    /** 0-1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Air", meta = (ClampMin = "0", ClampMax = "1"))
    float RelativeHumidity = 0.0f;

    /** Measured station pressure in pascals (about 101325 at sea level, not hPa or psi); 0 derives it from AltitudeFt. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Air", meta = (ClampMin = "0"))
    float PressurePa = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Air", meta = (ClampMin = "0"))
    float WindSpeedMph = 0.0f;

    /** Direction the wind blows from, clockwise from the world +X target line: 0 is a headwind, 90 from the right. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Air")
    float WindFromDeg = 0.0f;

    /** Degrees to turn launches clockwise (seen from above) from the world +X target line. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    float AimYawDeg = 0.0f;

    /** Channel for contact sweeps and ground traces. */
    UPROPERTY(EditAnywhere, Category = "OpenFairway")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** Visual scale of the ball mesh only; physics always uses the real ball size. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Presentation")
    float VisualScale = 1.0f;

    /** Draws the ball's path until the next launch. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Presentation")
    bool bDrawTracer = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Presentation")
    FLinearColor TracerColor = FLinearColor(1.0f, 0.85f, 0.2f);

    virtual void Tick(float DeltaSeconds) override;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, Category = "OpenFairway")
    TObjectPtr<UStaticMeshComponent> BallMesh;

private:
    struct FGroundHit
    {
        FVector Location = FVector::ZeroVector;
        FVector3f Normal = FVector3f(0.0f, 1.0f, 0.0f);
        TWeakObjectPtr<const UPrimitiveComponent> Component;
    };

    bool StepPhysics(float Dt);
    void HandleCollision(const FHitResult* Hit, bool bWasOnGround, const FVector3f& PreviousVelocity);
    bool ProbeGround(FGroundHit& OutHit, float UpMeters, float DownMeters) const;
    void SnapToGround();
    void SetSurface(EOpenFairwaySurface Surface);
    void EnterRest();
    void UpdateResult();
    void AddTracerPoint(bool bForce);
    void ClearTracer();
    OpenFairway::FAirConditions GetAirConditions() const;

    EOpenFairwayBallState BallState = EOpenFairwayBallState::Rest;
    /** OpenFairway frame (+X downrange, +Y up, +Z right), m/s and rad/s. */
    FVector3f Velocity = FVector3f::ZeroVector;
    FVector3f Omega = FVector3f::ZeroVector;
    FVector3f FloorNormal = FVector3f(0.0f, 1.0f, 0.0f);
    bool bOnGround = false;
    EOpenFairwaySurface CurrentSurface = EOpenFairwaySurface::Fairway;
    float RolloutImpactSpinRpm = 0.0f;
    float SubstepAccumulator = 0.0f;

    OpenFairway::FResolvedShotParams ShotParams;
    OpenFairway::FSurfaceSettings SurfaceSettings;
    FVector ShotStart = FVector::ZeroVector;
    /** Horizontal shot direction in the OpenFairway frame. */
    FVector3f ShotDirection = FVector3f(1.0f, 0.0f, 0.0f);
    FOpenFairwayShotResult Result;
    bool bHasShot = false;

    FVector LastTracerPoint = FVector::ZeroVector;
    uint32 TracerBatchId = 0;
};
