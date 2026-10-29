// SPDX-License-Identifier: MIT
#include "OpenGolfWindSubsystem.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/PackageName.h"
#include "OpenGolfCourse.h"

namespace OpenGolfWindSubsystemDetail
{
    constexpr float StandardAirDensity = 1.225f;
    /** The range sets the wind every frame; a missing collection is looked for this often, not every time. */
    constexpr double ResolveRetrySeconds = 1.0;
} // namespace OpenGolfWindSubsystemDetail

const FName UOpenGolfWindSubsystem::WindParameterName(TEXT("Wind"));
const FName UOpenGolfWindSubsystem::AirParameterName(TEXT("WindAir"));

void UOpenGolfWindSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    // Nothing to write yet: the collection's own defaults are calm.
    ResolveParameterCollection();
}

bool UOpenGolfWindSubsystem::ResolveParameterCollection()
{
    if (ParameterCollection)
        return false;
    const double Now = FPlatformTime::Seconds();
    if (Now < NextResolveSeconds)
        return false;
    NextResolveSeconds = Now + OpenGolfWindSubsystemDetail::ResolveRetrySeconds;
    // Missing until the wind assets are built (ApplyVegetationWind.py); checking first avoids a load warning.
    const FSoftObjectPath Path(OpenGolfCourse::WindParameterCollectionPath);
    if (!FPackageName::DoesPackageExist(Path.GetLongPackageName()))
        return false;
    ParameterCollection = Cast<UMaterialParameterCollection>(Path.TryLoad());
    return ParameterCollection != nullptr;
}

bool UOpenGolfWindSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

void UOpenGolfWindSubsystem::SetWind(const FVector& InWindMps, float InAirDensityKgM3, float InReferenceHeightM, float InRoughnessM)
{
    const FVector Wind = InWindMps.ContainsNaN() ? FVector::ZeroVector : InWindMps;
    const float Density = FMath::IsFinite(InAirDensityKgM3) ? FMath::Max(0.0f, InAirDensityKgM3) : OpenGolfWindSubsystemDetail::StandardAirDensity;
    const float Reference = FMath::IsFinite(InReferenceHeightM) ? InReferenceHeightM : OpenFairway::WindProfile::StandardReferenceHeightM;
    const float Roughness = FMath::IsFinite(InRoughnessM) ? InRoughnessM : OpenFairway::WindProfile::OpenGrassRoughnessM;
    // A collection built while the world was open gets the current wind straight away.
    const bool bNewCollection = ResolveParameterCollection();
    if (!bNewCollection && Wind.Equals(WindMps, 1e-4) && Density == AirDensityKgM3 && Reference == ReferenceHeightM && Roughness == RoughnessM)
        return;
    // Keep Time * speed + offset where it was, so the gusts do not jump downwind with a new speed.
    if (const UWorld* World = GetWorld())
        GustOffsetM += static_cast<float>(World->GetTimeSeconds() * (WindMps.Size2D() - Wind.Size2D()));
    WindMps = Wind;
    AirDensityKgM3 = Density;
    ReferenceHeightM = Reference;
    RoughnessM = Roughness;
    WriteParameters();
}

FVector UOpenGolfWindSubsystem::GetWindAtHeightMps(float HeightM) const
{
    return WindMps * OpenFairway::WindProfile::Factor(HeightM, ReferenceHeightM, RoughnessM);
}

void UOpenGolfWindSubsystem::WriteParameters()
{
    UWorld* World = GetWorld();
    UMaterialParameterCollectionInstance* Instance = World && ParameterCollection ? World->GetParameterCollectionInstance(ParameterCollection) : nullptr;
    if (!Instance)
        return;
    Instance->SetVectorParameterValue(WindParameterName,
        FLinearColor(static_cast<float>(WindMps.X), static_cast<float>(WindMps.Y), static_cast<float>(WindMps.Size2D()), ReferenceHeightM));
    Instance->SetVectorParameterValue(AirParameterName, FLinearColor(RoughnessM, GustOffsetM, AirDensityKgM3, 0.0f));
}
