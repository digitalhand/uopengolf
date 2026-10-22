// SPDX-License-Identifier: MIT
#include "OpenGolfFlagstickComponent.h"

#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "OpenGolfCourse.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

UOpenGolfFlagstickComponent::UOpenGolfFlagstickComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(OpenGolfCourse::FlagstickMeshPath);
    if (Mesh.Succeeded())
    {
        SetStaticMesh(Mesh.Object);
    }
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Flag(OpenGolfCourse::FlagMaterialPath);
    FlagMaterial = Flag.Object;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SetGenerateOverlapEvents(false);
    SetCanEverAffectNavigation(false);
    // The range can also enable editor ticking for wind preview.
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UOpenGolfFlagstickComponent::OnRegister()
{
    Super::OnRegister();
    if (IsTemplate() || !GetWorld())
        return;
    // The flag starts laid out along +Y and falls for a moment, so it is already hanging when seen.
    Cloth.Reset(GetClothSettings(), GetPoleTop(), static_cast<float>(GetComponentRotation().Yaw) + 90.0f);
    Cloth.Settle(SettleSeconds, GetPoleTop());
    if (!FlagCloth)
    {
        FlagCloth = NewObject<UProceduralMeshComponent>(this, TEXT("FlagCloth"), RF_Transient | RF_TextExportTransient);
        FlagCloth->SetupAttachment(this);
        FlagCloth->SetMobility(EComponentMobility::Movable);
        FlagCloth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        FlagCloth->SetGenerateOverlapEvents(false);
        FlagCloth->SetCanEverAffectNavigation(false);
        FlagCloth->RegisterComponent();
    }
    UpdateFlagMesh(true);
}

void UOpenGolfFlagstickComponent::OnUnregister()
{
    if (FlagCloth)
    {
        FlagCloth->DestroyComponent();
        FlagCloth = nullptr;
    }
    Super::OnUnregister();
}

void UOpenGolfFlagstickComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Cloth.Advance(DeltaTime, GetPoleTop());
    UpdateFlagMesh(false);
}

void UOpenGolfFlagstickComponent::SetWind(const FVector& WindVelocityMps, float AirDensityKgM3)
{
    Cloth.SetWind(WindVelocityMps, AirDensityKgM3);
}

FVector UOpenGolfFlagstickComponent::GetPoleTop() const
{
    return GetComponentTransform().TransformPosition(FVector(0.0, 0.0, PoleHeightCm));
}

FOpenGolfFlagCloth::FSettings UOpenGolfFlagstickComponent::GetClothSettings() const
{
    FOpenGolfFlagCloth::FSettings Settings;
    Settings.LengthCm = FlagLengthCm;
    Settings.HeightCm = FlagHeightCm;
    Settings.PoleRadiusCm = PoleDiameterCm * 0.5f;
    Settings.ClothKgPerM2 = ClothKgPerM2;
    Settings.NormalForceCoefficient = NormalForceCoefficient;
    Settings.FrictionCoefficient = FrictionCoefficient;
    return Settings;
}

void UOpenGolfFlagstickComponent::UpdateFlagMesh(bool bCreate)
{
    if (!FlagCloth)
        return;
    const FTransform& ToWorld = GetComponentTransform();
    const TArray<FVector>& World = Cloth.GetPositions();
    const TArray<int32>& Triangles = Cloth.GetTriangles();

    auto& Vertices = MeshVertices;
    Vertices.SetNumUninitialized(World.Num());
    for (int32 Index = 0; Index < World.Num(); ++Index)
        Vertices[Index] = ToWorld.InverseTransformPosition(World[Index]);

    // Area-weighted vertex normals, wound as the engine expects for front faces.
    auto& Normals = MeshNormals;
    Normals.Init(FVector::ZeroVector, Vertices.Num());
    for (int32 Corner = 0; Corner < Triangles.Num(); Corner += 3)
    {
        const FVector& P0 = Vertices[Triangles[Corner]];
        const FVector& P1 = Vertices[Triangles[Corner + 1]];
        const FVector& P2 = Vertices[Triangles[Corner + 2]];
        const FVector Face = FVector::CrossProduct(P1 - P2, P0 - P2);
        for (int32 Offset = 0; Offset < 3; ++Offset)
            Normals[Triangles[Corner + Offset]] += Face;
    }

    auto& UVs = MeshUVs;
    auto& Tangents = MeshTangents;
    UVs.SetNumUninitialized(Vertices.Num());
    Tangents.SetNumUninitialized(Vertices.Num());
    for (int32 Row = 0; Row < FOpenGolfFlagCloth::Rows; ++Row)
    {
        for (int32 Column = 0; Column < FOpenGolfFlagCloth::Columns; ++Column)
        {
            const int32 Index = FOpenGolfFlagCloth::Index(Column, Row);
            Normals[Index] = Normals[Index].GetSafeNormal(UE_SMALL_NUMBER, FVector::XAxisVector);
            // U runs from the pole to the free edge, V from the top down.
            if (bCreate)
                UVs[Index] = FVector2D(static_cast<double>(Column) / (FOpenGolfFlagCloth::Columns - 1), static_cast<double>(Row) / (FOpenGolfFlagCloth::Rows - 1));
            const int32 Before = FOpenGolfFlagCloth::Index(FMath::Max(Column - 1, 0), Row);
            const int32 After = FOpenGolfFlagCloth::Index(FMath::Min(Column + 1, FOpenGolfFlagCloth::Columns - 1), Row);
            Tangents[Index] = FProcMeshTangent((Vertices[After] - Vertices[Before]).GetSafeNormal(), false);
        }
    }

    const TArray<FColor> NoColors;
    if (bCreate)
    {
        FlagCloth->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, NoColors, Tangents, false);
        FlagCloth->SetMaterial(0, FlagMaterial);
    }
    else
    {
        FlagCloth->UpdateMeshSection(0, Vertices, Normals, UVs, NoColors, Tangents);
    }
}
