// SPDX-License-Identifier: MIT
#pragma once

#include "Components/SceneComponent.h"
#include "OpenFairwayTypes.h"
#include "OpenGolfCupComponent.generated.h"

class UOpenGolfFlagstickComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * A regulation dark opening (4.25 in, OpenFairway::CupCapture), aligned with the green's surface. No liner is drawn
 * above the grass. The ball's capture is logic (AOpenFairwayBall cups), not
 * geometry. Like an attendant, the cup lifts its flagstick while a ball is near and lowers it again.
 *
 * Call PlaceAtFlagstick once the flag stands on the green; the cup then stays at the flag's foot.
 */
UCLASS(ClassGroup = (OpenGolf), meta = (BlueprintSpawnableComponent))
class OPENGOLFCOURSE_API UOpenGolfCupComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UOpenGolfCupComponent();

    /** Terrain-independent placement; the optional flag is positioned at the surface and lifted vertically. */
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Cup")
    void PlaceOnSurface(const FOpenFairwayCup& Surface, UOpenGolfFlagstickComponent* InFlagstick = nullptr);

    UFUNCTION(BlueprintPure, Category = "OpenGolf|Cup")
    FOpenFairwayCup GetCupDescriptor() const;

    /** Moves the cup to the flagstick's foot, aligns it to the surface normal and shows it; the flag stays upright. */
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Cup")
    void PlaceAtFlagstick(UOpenGolfFlagstickComponent* InFlagstick, FVector SurfaceNormal = FVector(0, 0, 1));

    /** The cup's centre on the green's surface. */
    UFUNCTION(BlueprintPure, Category = "OpenGolf|Cup")
    FVector GetCupLocation() const { return GetComponentLocation(); }

    UOpenGolfFlagstickComponent* GetFlagstick() const { return Flagstick.Get(); }

    /** Raises the flag while a ball is within PinLiftNearM of the cup, lowers it beyond PinLowerFarM, easing over PinLiftSeconds. */
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Cup")
    void UpdatePinLift(const FVector& BallLocation, float DeltaSeconds);

    /** The flagstick's current height above the cup, 0 to PinLiftCm. */
    UFUNCTION(BlueprintPure, Category = "OpenGolf|Cup")
    float GetPinLiftCm() const;

    /** Two feet up, as ogolf PinLift: high enough to see the cup, the flag still in view. */
    static constexpr float PinLiftCm = 60.96f;
    /** Within 5 ft the pin comes out; beyond 6 ft it goes back. */
    static constexpr float PinLiftNearM = 1.524f;
    static constexpr float PinLowerFarM = 1.8288f;
    static constexpr float PinLiftSeconds = 0.2f;

    UE_DEPRECATED(5.8, "The surface liner was removed. Use GetHole for the opening.")
    UStaticMeshComponent* GetLiner() const { return nullptr; }
    UStaticMeshComponent* GetHole() const { return Hole; }

    virtual void OnRegister() override;
    virtual void OnUnregister() override;

private:
    static void SetDiscColor(UStaticMeshComponent* Disc, const FLinearColor& Color);

    /** Engine cylinder and basic shape material, tinted per disc. */
    UPROPERTY()
    TObjectPtr<UStaticMesh> DiscMesh;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> DiscMaterial;

    UPROPERTY(Transient)
    TObjectPtr<UStaticMeshComponent> Hole;

    /** Shown once placed at a flag. */
    bool bPlaced = false;

    TWeakObjectPtr<UOpenGolfFlagstickComponent> Flagstick;
    float PinLift = 0.0f;
    bool bPinRaised = false;
};
