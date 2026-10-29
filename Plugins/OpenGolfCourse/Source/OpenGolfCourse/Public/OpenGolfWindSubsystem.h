// SPDX-License-Identifier: MIT
#pragma once

#include "OpenFairway/WindProfile.h"
#include "Subsystems/WorldSubsystem.h"
#include "OpenGolfWindSubsystem.generated.h"

class UMaterialParameterCollection;

/**
 * One world's wind, for everything that shows it: the reference (10 m) wind in world space, the air
 * density, and the log profile that slows the wind near the ground. Vegetation materials read it from
 * MPC_Wind; code asks GetWindAtHeightMps. A source such as the range sets it; a world nobody sets stays
 * calm, and so do its materials. Works in editor worlds too, for wind preview.
 */
UCLASS()
class OPENGOLFCOURSE_API UOpenGolfWindSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    /** MPC_Wind vector: world X and Y of the reference wind (m/s), its speed (m/s) and the reference height (m). */
    static const FName WindParameterName;
    /**
     * MPC_Wind vector: roughness length (m), gust travel offset (m), air density (kg/m^3), unused. Materials
     * move their gusts downwind by Time * speed + the offset; the offset keeps them in place when the speed
     * changes, against the material Time of a game world.
     */
    static const FName AirParameterName;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

    /** World wind at ReferenceHeightM in m/s. Non-finite values read as calm, standard air and the standard profile. */
    void SetWind(const FVector& WindMps, float AirDensityKgM3,
        float ReferenceHeightM = OpenFairway::WindProfile::StandardReferenceHeightM,
        float RoughnessM = OpenFairway::WindProfile::OpenGrassRoughnessM);

    /** World wind at the reference height, m/s. */
    const FVector& GetWindMps() const { return WindMps; }
    /** World wind at HeightM above the ground, m/s. */
    FVector GetWindAtHeightMps(float HeightM) const;
    float GetAirDensityKgM3() const { return AirDensityKgM3; }
    float GetReferenceHeightM() const { return ReferenceHeightM; }
    float GetRoughnessM() const { return RoughnessM; }
    float GetGustOffsetM() const { return GustOffsetM; }

    /** MPC_Wind, or null before the wind assets are built; the wind state works without it. */
    UMaterialParameterCollection* GetParameterCollection() const { return ParameterCollection; }

private:
    /** Loads MPC_Wind once it exists, looking at most once a second; true when it was found just now. */
    bool ResolveParameterCollection();
    void WriteParameters();

    UPROPERTY(Transient)
    TObjectPtr<UMaterialParameterCollection> ParameterCollection;

    FVector WindMps = FVector::ZeroVector;
    float AirDensityKgM3 = 1.225f;
    float ReferenceHeightM = OpenFairway::WindProfile::StandardReferenceHeightM;
    float RoughnessM = OpenFairway::WindProfile::OpenGrassRoughnessM;
    float GustOffsetM = 0.0f;
    /** FPlatformTime seconds before which a missing MPC_Wind is not looked for again. */
    double NextResolveSeconds = 0.0;
};
