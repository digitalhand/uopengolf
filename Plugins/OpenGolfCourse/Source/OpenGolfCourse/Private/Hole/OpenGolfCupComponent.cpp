// SPDX-License-Identifier: MIT
#include "Hole/OpenGolfCupComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "OpenFairway/CupCapture.h"
#include "OpenGolfFlagstickComponent.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr float CmPerM = 100.0f;
    /** Engine cylinder: 100 cm across and tall, centred on its origin. */
    constexpr float CylinderSizeCm = 100.0f;
    /** Thin discs with top faces just above the surface, measured along its normal. */
    constexpr float DiscCm = 0.01f;
    constexpr float HoleLiftCm = 0.02f;
    /** The pin only lifts for a ball at about the green's height. */
    constexpr float NearGreenCm = 200.0f;
} // namespace

UOpenGolfCupComponent::UOpenGolfCupComponent()
{
    // Placed at the flag during Play, under parents that may be static.
    SetMobility(EComponentMobility::Movable);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    DiscMesh = Cylinder.Object;
    DiscMaterial = ShapeMaterial.Object;
}

void UOpenGolfCupComponent::OnRegister()
{
    Super::OnRegister();
    if (IsTemplate() || !GetWorld())
        return;
    // Transient parts, as the flagstick's cloth: the cup is rebuilt wherever the component is registered.
    auto MakeDisc = [this](UStaticMeshComponent*& Disc, FName Name, float DiameterCm, float LiftCm)
    {
        if (Disc)
            return;
        Disc = NewObject<UStaticMeshComponent>(this, Name, RF_Transient | RF_TextExportTransient);
        Disc->SetupAttachment(this);
        Disc->SetMobility(EComponentMobility::Movable);
        Disc->SetStaticMesh(DiscMesh);
        Disc->SetMaterial(0, DiscMaterial);
        Disc->SetRelativeLocation(FVector(0, 0, LiftCm - DiscCm * 0.5f));
        Disc->SetRelativeScale3D(FVector(DiameterCm / CylinderSizeCm, DiameterCm / CylinderSizeCm, DiscCm / CylinderSizeCm));
        Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Disc->SetGenerateOverlapEvents(false);
        Disc->SetCanEverAffectNavigation(false);
        Disc->SetCastShadow(false);
        Disc->SetVisibility(bPlaced);
        Disc->RegisterComponent();
    };
    const float Diameter = OpenFairway::CupCapture::CupDiameterM * CmPerM;
    UStaticMeshComponent* HolePart = Hole;
    MakeDisc(HolePart, TEXT("CupHole"), Diameter, HoleLiftCm);
    Hole = HolePart;
    SetDiscColor(Hole, FLinearColor(0.004f, 0.005f, 0.004f));
}

void UOpenGolfCupComponent::OnUnregister()
{
    if (Hole)
    {
        Hole->DestroyComponent();
        Hole = nullptr;
    }
    Super::OnUnregister();
}

void UOpenGolfCupComponent::SetDiscColor(UStaticMeshComponent* Disc, const FLinearColor& Color)
{
    if (UMaterialInstanceDynamic* Material = Disc ? Disc->CreateDynamicMaterialInstance(0) : nullptr)
        Material->SetVectorParameterValue(TEXT("Color"), Color);
}

void UOpenGolfCupComponent::PlaceAtFlagstick(UOpenGolfFlagstickComponent* InFlagstick, FVector SurfaceNormal)
{
    if (InFlagstick)
        PlaceOnSurface({InFlagstick->GetComponentLocation(), SurfaceNormal}, InFlagstick);
}

void UOpenGolfCupComponent::PlaceOnSurface(const FOpenFairwayCup& Surface, UOpenGolfFlagstickComponent* InFlagstick)
{
    if (Surface.Position.ContainsNaN())
        return;
    // Restore a previously raised flag before detaching it or moving the cup.
    if (Flagstick.IsValid())
        Flagstick->SetWorldLocation(GetCupLocation());
    Flagstick = InFlagstick;
    PinLift = 0.0f;
    bPinRaised = false;
    SetWorldLocationAndRotation(Surface.Position, FRotationMatrix::MakeFromZ(Surface.GetNormal()).Rotator());
    SetWorldScale3D(FVector::OneVector);
    if (InFlagstick)
        InFlagstick->SetWorldLocation(Surface.Position);
    bPlaced = true;
    if (Hole)
        Hole->SetVisibility(true);
}

FOpenFairwayCup UOpenGolfCupComponent::GetCupDescriptor() const
{
    return {GetCupLocation(), GetUpVector()};
}

float UOpenGolfCupComponent::GetPinLiftCm() const
{
    return FMath::SmoothStep(0.0f, 1.0f, PinLift) * PinLiftCm;
}

void UOpenGolfCupComponent::UpdatePinLift(const FVector& BallLocation, float DeltaSeconds)
{
    UOpenGolfFlagstickComponent* Flag = Flagstick.Get();
    if (!Flag)
        return;
    const FVector Cup = GetCupLocation();
    const double DistanceM = FVector::Dist2D(BallLocation, Cup) / CmPerM;
    const bool bNearGreen = FMath::Abs(BallLocation.Z - Cup.Z) < NearGreenCm;
    if (!bPinRaised && bNearGreen && DistanceM < PinLiftNearM)
        bPinRaised = true;
    else if (bPinRaised && (!bNearGreen || DistanceM > PinLowerFarM))
        bPinRaised = false;
    const float Target = bPinRaised ? 1.0f : 0.0f;
    if (PinLift == Target)
        return;
    PinLift = FMath::FInterpConstantTo(PinLift, Target, DeltaSeconds, 1.0f / PinLiftSeconds);
    Flag->SetWorldLocation(Cup + FVector(0, 0, GetPinLiftCm()));
}
