// SPDX-License-Identifier: MIT
#pragma once

#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairwayTypes.h"
#include "OpenFairwayBall.generated.h"

class UPhysicalMaterial;
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

    /** Stops the ball and places it on the ground at Location. A shot still moving ends as a safety stop where it was. */
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

    /**
     * The surface for a contact, first match wins: a "OpenFairway.Surface.<Name>" tag on the component (an explicit
     * override), an UOpenFairwayPhysicalMaterial (the hit's, else the component's simple-collision material), a tag
     * on the component's actor, else DefaultSurface.
     */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    EOpenFairwaySurface ResolveSurface(const UPrimitiveComponent* Component, const UPhysicalMaterial* PhysicalMaterial = nullptr) const;

    /** The tag that assigns a surface, such as "OpenFairway.Surface.Green". */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    static FName GetSurfaceTag(EOpenFairwaySurface Surface);

    /**
     * The tag ("OpenFairway.Obstacle") that marks an upright obstacle such as a tree trunk, on the component or its
     * actor. The ball deflects off it sideways, never lands or rests on it, and ground probes look through it.
     */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    static FName GetObstacleTag();

    static bool IsObstacle(const UPrimitiveComponent* Component);

    /**
     * Velocity after hitting an obstacle, in the OpenFairway frame (Y up). The ball reflects off the horizontal
     * part of Normal and keeps ObstacleRestitution of its speed. On any upward-facing contact it leaves at
     * ObstacleEscapeMps or more, so it cannot settle there: along the contact's horizontal normal on a rim, or along
     * AwayFromObstacle (its horizontal offset from the obstacle's axis) on a near-flat top.
     */
    static FVector3f ResolveObstacleHit(const FVector3f& Velocity, const FVector3f& Normal, const FVector3f& AwayFromObstacle);

    static constexpr float ObstacleRestitution = 0.30f;
    static constexpr float ObstacleEscapeMps = 1.0f;

    /**
     * The tag ("OpenFairway.Canopy") that marks foliage, such as a tree's branches and leaves, on the component or
     * its actor. Give it an overlap (never blocking) response on TraceChannel: a ball in the air inside it is slowed
     * and turned slightly by ResolveCanopyDrag, and it is never a contact or the ground.
     */
    UFUNCTION(BlueprintPure, Category = "OpenFairway")
    static FName GetCanopyTag();

    static bool IsCanopy(const UPrimitiveComponent* Component);

    /** Fraction of speed and spin kept after PathM metres of canopy: exp(-PathM / CanopyDecayLengthM). */
    static float CanopySpeedFactor(float PathM);

    /**
     * Velocity after PathM metres through canopy, in the OpenFairway frame. Speed scales by CanopySpeedFactor, so
     * many short steps lose as much as one long one. The direction turns by a seeded angle of at most
     * CanopyDeflectionDegPerSqrtM * sqrt(PathM), so a shot repeated with the same seeds takes the same path.
     */
    static FVector3f ResolveCanopyDrag(const FVector3f& Velocity, float PathM, uint32 Seed);

    static constexpr float CanopyDecayLengthM = 4.0f;
    static constexpr float CanopyDeflectionDegPerSqrtM = 6.0f;

    /** The wind in world space (X downrange, Y right, Z up), m/s. */
    UFUNCTION(BlueprintPure, Category = "OpenFairway|Air")
    FVector GetWindVelocityMps() const;

    /** Air density from the ball's air settings, kg/m^3. */
    UFUNCTION(BlueprintPure, Category = "OpenFairway|Air")
    float GetAirDensityKgM3() const;

    /** Headless prediction of a launch on flat ground of one surface, using this ball's air and profile. */
    OpenFairway::FShotSimulationResult PredictShot(const FOpenFairwayLaunch& LaunchData, EOpenFairwaySurface Surface) const;

    static OpenFairway::FLaunchData ToLaunchData(const FOpenFairwayLaunch& LaunchData);

    /** Rotation that rolls a ball of RadiusCm without slipping through DeltaCm along ground facing Up, capped at MaxAngleRad. */
    static FQuat ComputeRollRotation(const FVector& DeltaCm, const FVector& Up, float RadiusCm, float MaxAngleRad);

    /** Ball profile (calibrated defaults); changes apply from the next launch. Its green stimp comes from GreenStimpFeet. */
    OpenFairway::FBallPhysicsProfile Profile;

    UPROPERTY(BlueprintAssignable, Category = "OpenFairway")
    FOnOpenFairwayBallLanded OnBallLanded;

    UPROPERTY(BlueprintAssignable, Category = "OpenFairway")
    FOnOpenFairwayBallAtRest OnBallAtRest;

    /** Surface used where no surface tag is found. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway")
    EOpenFairwaySurface DefaultSurface = EOpenFairwaySurface::Fairway;

    /**
     * Speed of every green as a stimp reading in feet (8-15; values outside play as the nearest end).
     * It sets only the green's constant rolling friction; applies from the next launch.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway", meta = (ClampMin = "8", ClampMax = "15"))
    float GreenStimpFeet = OpenFairway::GreenSpeed::DefaultStimpFeet;

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

    /**
     * A shot still moving after this many seconds ends as a safety stop, as the headless simulation
     * does, so a ball trapped between contacts cannot leave the shot running forever.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway", meta = (ClampMin = "0.1"))
    float MaxShotSeconds = OpenFairway::ShotSimulator::DefaultMaxTimeSeconds;

    /** Channel for contact sweeps and ground traces. */
    UPROPERTY(EditAnywhere, Category = "OpenFairway")
    TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

    /** Visual scale of the ball mesh only; physics always uses the real ball size. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Presentation")
    float VisualScale = 1.0f;

    /** Turns the ball mesh as it rolls along the ground. Presentation only; flight spin is not shown. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenFairway|Presentation")
    bool bVisualRoll = true;

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
        TWeakObjectPtr<const UPhysicalMaterial> PhysMaterial;
    };

    bool StepPhysics(float Dt);
    void ApplyCanopy(float Dt);
    void HandleCollision(const FHitResult* Hit, bool bWasOnGround, const FVector3f& PreviousVelocity);
    bool ProbeGround(FGroundHit& OutHit, float UpMeters, float DownMeters) const;
    void SnapToGround();
    void ApplyVisualRoll(const FVector& FrameDelta);
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
    /** Physics steps since launch; seeds canopy deflection so a repeated shot takes the same path. */
    uint32 ShotStep = 0;

    OpenFairway::FResolvedShotParams ShotParams;
    OpenFairway::FSurfaceSettings SurfaceSettings;
    FVector ShotStart = FVector::ZeroVector;
    /** Horizontal shot direction in the OpenFairway frame. */
    FVector3f ShotDirection = FVector3f(1.0f, 0.0f, 0.0f);
    FOpenFairwayShotResult Result;
    bool bHasShot = false;

    FVector LastTracerPoint = FVector::ZeroVector;
    uint32 TracerBatchId = 0;

    /** Reused by every in-flight substep's canopy query. */
    TArray<FOverlapResult> CanopyOverlaps;
};
