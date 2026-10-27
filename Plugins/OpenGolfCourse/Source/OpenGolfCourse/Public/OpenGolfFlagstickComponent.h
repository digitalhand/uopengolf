// SPDX-License-Identifier: MIT
#pragma once

#include "Components/StaticMeshComponent.h"
#include "OpenGolfFlagCloth.h"
#include "ProceduralMeshComponent.h"
#include "OpenGolfFlagstickComponent.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

/**
 * A seven-foot flagstick with a cloth flag. The component draws the pole; its FlagCloth child draws
 * the flag, simulated by FOpenGolfFlagCloth every tick: gravity drops it, and the wind passed to
 * SetWind flies and flaps it. Place it at the hole with its origin on the ground; with no wind the
 * flag hangs from the pole on the component's +Y side. Neither part has collision.
 */
UCLASS(ClassGroup = (OpenGolf), meta = (BlueprintSpawnableComponent))
class OPENGOLFCOURSE_API UOpenGolfFlagstickComponent : public UStaticMeshComponent
{
    GENERATED_BODY()

public:
    UOpenGolfFlagstickComponent(const FObjectInitializer& ObjectInitializer);

    /** Pole top above the ground: seven feet. */
    static constexpr float PoleHeightCm = 213.36f;
    /** Half-inch pole. */
    static constexpr float PoleDiameterCm = 1.27f;
    /** A 20 by 14 inch flag, measured along the flag and down from the pole top. */
    static constexpr float FlagLengthCm = 50.8f;
    static constexpr float FlagHeightCm = 35.56f;
    /** Seconds of calm air simulated when the flag is created, so it starts hanging. */
    static constexpr float SettleSeconds = 2.0f;

    /** Two-sided cloth material; recolor with an instance such as MI_Flag_Red. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenGolf|Flag")
    TObjectPtr<UMaterialInterface> FlagMaterial;

    /** Cloth weight per area, kg/m^2; a nylon flag is about 0.11. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenGolf|Flag", meta = (ClampMin = "0.01"))
    float ClothKgPerM2 = 0.11f;

    /** Flat-plate pressure coefficient across the cloth. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenGolf|Flag", meta = (ClampMin = "0"))
    float NormalForceCoefficient = 1.2f;

    /** Skin-friction coefficient along the cloth. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenGolf|Flag", meta = (ClampMin = "0"))
    float FrictionCoefficient = 0.015f;

    /**
     * Keep simulating the cloth in Play when it has not been drawn for a while. Off by default: an
     * unseen flag skips its solve and mesh upload and resumes from the same pose when seen again.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenGolf|Flag")
    bool bSimulateWhenHidden = false;

    /** World wind in m/s; gusts are added on top. The flag's sleeve turns downwind. */
    UFUNCTION(BlueprintCallable, Category = "OpenGolf|Flag")
    void SetWind(const FVector& WindVelocityMps, float AirDensityKgM3 = 1.225f);

    UProceduralMeshComponent* GetFlagCloth() const { return FlagCloth; }
    const FOpenGolfFlagCloth& GetCloth() const { return Cloth; }

    virtual void OnRegister() override;
    virtual void OnUnregister() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    FVector GetPoleTop() const;
    FOpenGolfFlagCloth::FSettings GetClothSettings() const;
    void UpdateFlagMesh(bool bCreate);

    FOpenGolfFlagCloth Cloth;
    TArray<FVector> MeshVertices;
    TArray<FVector> MeshNormals;
    TArray<FVector2D> MeshUVs;
    TArray<FProcMeshTangent> MeshTangents;

    UPROPERTY(Transient)
    TObjectPtr<UProceduralMeshComponent> FlagCloth;
};
