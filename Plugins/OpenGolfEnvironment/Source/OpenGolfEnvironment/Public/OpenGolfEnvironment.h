// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/DataAsset.h"
#include "Curves/CurveFloat.h"
#include "OpenGolfEnvironment.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UVolumetricCloudComponent;
class UExponentialHeightFogComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EOpenGolfDaylightPreset : uint8
{
    Morning,
    Day,
    Sunset
};

USTRUCT(BlueprintType)
struct OPENGOLFENVIRONMENT_API FOpenGolfEnvironmentSettings
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0", ClampMax = "1"))
    float Daylight = .5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0", ClampMax = "1"))
    float CloudCoverage = .35f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "0", ClampMax = "5"))
    float CloudSpeed = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    bool bAutomaticTime = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment", meta = (ClampMin = "1", ClampMax = "1440"))
    float DurationMinutes = 60;
    void Sanitize();
    void Advance(float Seconds);
};

UCLASS(BlueprintType)
class OPENGOLFENVIRONMENT_API UOpenGolfEnvironmentProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UOpenGolfEnvironmentProfile();
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    FOpenGolfEnvironmentSettings StartingSettings;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    float SunOrientationDegrees = 35;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    FRuntimeFloatCurve SunElevation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    FRuntimeFloatCurve SunIntensity;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    FRuntimeFloatCurve SunTemperature;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    FRuntimeFloatCurve FogDensity;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    TSoftObjectPtr<UMaterialInterface> CloudMaterial;
};

DECLARE_MULTICAST_DELEGATE(FOpenGolfEnvironmentChanged);

UCLASS(BlueprintType)
class OPENGOLFENVIRONMENT_API AOpenGolfEnvironment : public AActor
{
    GENERATED_BODY()
public:
    AOpenGolfEnvironment();
    static AOpenGolfEnvironment* Find(const UWorld* World);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    TObjectPtr<UOpenGolfEnvironmentProfile> Profile;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment")
    bool bOverrideStartingSettings = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Environment", meta = (EditCondition = "bOverrideStartingSettings"))
    FOpenGolfEnvironmentSettings StartingSettings;
    UPROPERTY(EditAnywhere, Category = "Environment")
    bool bPreviewAnimationInEditor = false;
    UFUNCTION(BlueprintPure, Category = "Environment")
    FOpenGolfEnvironmentSettings GetSettings() const { return Current; }
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void ApplySettings(FOpenGolfEnvironmentSettings Settings, bool bNotify = true);
    UFUNCTION(BlueprintCallable, Category = "Environment")
    void SelectPreset(EOpenGolfDaylightPreset Preset);
    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Environment")
    void ResetToCourseDefaults();
    FOpenGolfEnvironmentChanged OnSettingsChanged;
    float GetCloudOffset() const { return CloudOffset; }
    virtual void PostRegisterAllComponents() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual bool ShouldTickIfViewportsOnly() const override { return bPreviewAnimationInEditor; }
#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& Event) override;
#endif
protected:
    virtual void BeginPlay() override;

private:
    void UpdateAtmosphere();
    const UOpenGolfEnvironmentProfile& ActiveProfile() const;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UDirectionalLightComponent> Sun;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<USkyAtmosphereComponent> Sky;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<USkyLightComponent> SkyLight;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UVolumetricCloudComponent> Clouds;
    UPROPERTY(VisibleAnywhere, Category = "Environment")
    TObjectPtr<UExponentialHeightFogComponent> Fog;
    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> CloudInstance;
    FSoftObjectPath CloudMaterialPath;
    UPROPERTY(Transient)
    FOpenGolfEnvironmentSettings Current;
    float CloudOffset = 0;
};
