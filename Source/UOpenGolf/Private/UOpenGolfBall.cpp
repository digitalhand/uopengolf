// SPDX-License-Identifier: MIT
#include "UOpenGolfBall.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "RangePerformance.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

AUOpenGolfBall::AUOpenGolfBall()
{
    bDrawTracer = false;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/UOpenGolf/Art/M_ShotTrail.M_ShotTrail"));
    static const TCHAR* RibbonNames[] = {TEXT("ShotTrail"), TEXT("ShotTrail1"), TEXT("ShotTrail2")};
    static_assert(UE_ARRAY_COUNT(RibbonNames) == UE_ARRAY_COUNT(Ribbons), "One name per tracer ribbon");
    for (int32 I = 0; I < UE_ARRAY_COUNT(Ribbons); ++I)
    {
        UProceduralMeshComponent* Trail = CreateDefaultSubobject<UProceduralMeshComponent>(RibbonNames[I]);
        Trail->SetupAttachment(RootComponent);
        Trail->SetAbsolute(true, true, true);
        Trail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Trail->SetGenerateOverlapEvents(false);
        Trail->SetCanEverAffectNavigation(false);
        Trail->CastShadow = false;
        Trail->SetMaterial(0, Material.Object);
        Ribbons[I].Mesh = Trail;
    }
    // Import normalizes the mesh to 100 cm diameter, matching the base actor's scale.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/UOpenGolf/Art/SM_GolfBall.SM_GolfBall"));
    if (Mesh.Succeeded())
    {
        BallMesh->SetStaticMesh(Mesh.Object);
    }
}

void AUOpenGolfBall::BeginPlay()
{
    Super::BeginPlay();
    // Normalize the actual imported bounds, rather than assuming the FBX import scale.
    const float Diameter = BallMesh->GetStaticMesh()->GetBounds().BoxExtent.GetMax() * 2.0f;
    BallMesh->SetWorldScale3D(FVector(4.267f / FMath::Max(Diameter, 0.001f)));
    for (FUOpenGolfTrailRibbon& Ribbon : Ribbons)
    {
        Ribbon.Mesh->SetWorldTransform(FTransform::Identity);
        Ribbon.Material = Ribbon.Mesh->CreateDynamicMaterialInstance(0);
    }
    ClearShotTrailHistory();
    ResetShotTrail();
}

void AUOpenGolfBall::ClearRibbon(FUOpenGolfTrailRibbon& Ribbon)
{
    Ribbon.Points.Reset();
    Ribbon.Mesh->ClearAllMeshSections();
    Ribbon.MeshPointCount = 0;
    Ribbon.ShotNumber = INDEX_NONE;
}

void AUOpenGolfBall::ApplyRibbonLook(FUOpenGolfTrailRibbon& Ribbon, const FLinearColor& Color, float Opacity)
{
    if (Ribbon.Material && Ribbon.AppliedColor != Color)
    {
        Ribbon.Material->SetVectorParameterValue(TEXT("Color"), Color);
        Ribbon.AppliedColor = Color;
    }
    if (Ribbon.Material && Ribbon.AppliedOpacity != Opacity)
    {
        Ribbon.Material->SetScalarParameterValue(TEXT("Opacity"), Opacity);
        Ribbon.AppliedOpacity = Opacity;
    }
    const bool bVisible = Opacity > 0.001f;
    if (Ribbon.bVisible != bVisible)
    {
        Ribbon.Mesh->SetVisibility(bVisible);
        Ribbon.bVisible = bVisible;
    }
}

void AUOpenGolfBall::ResetShotTrail()
{
    ClearRibbon(Ribbons[CurrentRibbon]);
    CurrentOpacity = 0.0f;
    FadeCeiling = 1.0f;
    bTrailActive = false;
}

void AUOpenGolfBall::ClearShotTrailHistory()
{
    for (const int32 Index : HistoryRibbons)
        ClearRibbon(Ribbons[Index]);
    HistoryRibbons.Reset();
}

int32 AUOpenGolfBall::GetHistoryTrailShotNumber(int32 Index) const
{
    return HistoryRibbons.IsValidIndex(Index) ? Ribbons[HistoryRibbons[Index]].ShotNumber : INDEX_NONE;
}

void AUOpenGolfBall::StartShotTrail()
{
    ResetShotTrail();
    FUOpenGolfTrailRibbon& Ribbon = Ribbons[CurrentRibbon];
    Ribbon.Points.Add(GetActorLocation());
    CurrentOpacity = TrailOpacity;
    bTrailActive = true;
    FOpenFairwayShotResult ShotResult;
    if (GetShotResult(ShotResult))
        TrailShotNumber = ShotResult.ShotNumber;
    Ribbon.ShotNumber = TrailShotNumber;
}

void AUOpenGolfBall::PromoteCurrentToHistory()
{
    if (Ribbons[CurrentRibbon].Points.Num() < 2)
        return;
    // The finished ribbon keeps its geometry; the oldest history ribbon becomes the next current one.
    const int32 Finished = CurrentRibbon;
    int32 Next = INDEX_NONE;
    if (HistoryRibbons.Num() == MaxHistoryTrails)
    {
        Next = HistoryRibbons.Pop();
        ClearRibbon(Ribbons[Next]);
    }
    else
    {
        for (int32 I = 0; I < UE_ARRAY_COUNT(Ribbons) && Next == INDEX_NONE; ++I)
            if (I != Finished && !HistoryRibbons.Contains(I))
                Next = I;
    }
    HistoryRibbons.Insert(Finished, 0);
    CurrentRibbon = Next;
    ApplyRibbonLook(Ribbons[Finished], HistoryTrailColor, HistoryTrailOpacity);
    UpdateTrailGeometry(Ribbons[Finished]);
}

void AUOpenGolfBall::Tick(float DeltaSeconds)
{
    // Also support direct launches used by automation and Blueprint callers.
    FOpenFairwayShotResult ShotResult;
    const bool bNewShot = GetShotResult(ShotResult) && ShotResult.ShotNumber != TrailShotNumber;
    if (IsMoving() && (!bTrailActive || bNewShot))
        StartShotTrail();
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(UOpenGolf_BallPhysics);
        RangePerformance::FScope Timing(RangePerformance::Physics);
        Super::Tick(DeltaSeconds);
    }
    if (!bTrailActive && HistoryRibbons.IsEmpty())
        return;
    TRACE_CPUPROFILER_EVENT_SCOPE(UOpenGolf_ShotTrail);
    RangePerformance::FScope Timing(RangePerformance::Trail);
    // History ribbons only re-orient when the camera moves.
    for (const int32 Index : HistoryRibbons)
        UpdateTrailGeometry(Ribbons[Index]);
    if (!bTrailActive)
        return;
    FUOpenGolfTrailRibbon& Ribbon = Ribbons[CurrentRibbon];
    TArray<FVector>& TrailPoints = Ribbon.Points;
    const FVector Position = GetActorLocation();
    if (TrailPoints.IsEmpty() || FVector::DistSquared(TrailPoints.Last(), Position) >= 625.0 || !IsMoving())
    {
        if (TrailPoints.IsEmpty() || !TrailPoints.Last().Equals(Position, 0.01))
            TrailPoints.Add(Position);
    }
    // Bound memory for extreme shots, preserving the entire arc by decimating samples.
    if (TrailPoints.Num() > 4096)
    {
        TArray<FVector> Reduced;
        for (int32 I = 0; I < TrailPoints.Num() - 1; I += 2)
            Reduced.Add(TrailPoints[I]);
        Reduced.Add(TrailPoints.Last());
        TrailPoints = MoveTemp(Reduced);
    }
    if (GetBallState() != EOpenFairwayBallState::Flight)
    {
        const float T = FMath::Clamp(GetSpeed() / FMath::Max(TrailFadeSpeedMps, 0.1f), 0.0f, 1.0f);
        FadeCeiling = FMath::Min(FadeCeiling, T * T * (3.0f - 2.0f * T));
    }
    CurrentOpacity = IsMoving() ? TrailOpacity * FadeCeiling : 0.0f;
    ApplyRibbonLook(Ribbon, FLinearColor::White, CurrentOpacity);
    if (CurrentOpacity > 0.001f)
        UpdateTrailGeometry(Ribbon);
    if (!IsMoving())
    {
        bTrailActive = false;
        PromoteCurrentToHistory();
    }
}

void AUOpenGolfBall::UpdateTrailGeometry(FUOpenGolfTrailRibbon& Ribbon)
{
#if WITH_DEV_AUTOMATION_TESTS
    // Same rendered workload can measure the previous allocation/recreation path.
    static const bool bLegacy = FParse::Param(FCommandLine::Get(), TEXT("RangeLegacyTrail"));
    if (bLegacy)
    {
        UpdateTrailGeometryLegacy(Ribbon);
        return;
    }
#endif
    const TArray<FVector>& TrailPoints = Ribbon.Points;
    const auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!Camera || TrailPoints.Num() < 2)
        return;
    const FVector Eye = Camera->GetCameraLocation();
    const bool bTopologyChanged = Ribbon.MeshPointCount != TrailPoints.Num();
    if (!bTopologyChanged && Eye == Ribbon.LastEye && Ribbon.LastWidth == TrailWidthCm && Ribbon.LastMinWidthPerDistance == TrailMinWidthPerDistance)
        return;
    Ribbon.LastEye = Eye;
    Ribbon.LastWidth = TrailWidthCm;
    Ribbon.LastMinWidthPerDistance = TrailMinWidthPerDistance;
    Ribbon.Vertices.SetNumUninitialized(TrailPoints.Num() * 2, EAllowShrinking::No);
    if (bTopologyChanged)
    {
        Ribbon.Indices.Reset((TrailPoints.Num() - 1) * 6);
        Ribbon.UVs.SetNumUninitialized(TrailPoints.Num() * 2, EAllowShrinking::No);
        for (int32 I = 0; I < TrailPoints.Num(); ++I)
        {
            Ribbon.UVs[I * 2] = FVector2D(0, 0);
            Ribbon.UVs[I * 2 + 1] = FVector2D(1, 0);
            if (I > 0)
            {
                const int32 V = I * 2;
                Ribbon.Indices.Append({V - 2, V - 1, V, V - 1, V + 1, V});
            }
        }
    }
    FVector PreviousSide = FVector::ZeroVector;
    for (int32 I = 0; I < TrailPoints.Num(); ++I)
    {
        const FVector Tangent = (TrailPoints[FMath::Min(I + 1, TrailPoints.Num() - 1)] - TrailPoints[FMath::Max(0, I - 1)]).GetSafeNormal();
        FVector Side = FVector::CrossProduct(Tangent, Eye - TrailPoints[I]).GetSafeNormal();
        if (Side.IsNearlyZero())
            Side = PreviousSide.IsNearlyZero() ? FVector::RightVector : PreviousSide;
        if (!PreviousSide.IsNearlyZero() && FVector::DotProduct(Side, PreviousSide) < 0)
            Side *= -1;
        PreviousSide = Side;
        // A fixed world width shrinks below a pixel far from the camera; keep a minimum screen width.
        const float HalfWidth = 0.5f * FMath::Max(TrailWidthCm, FVector::Dist(Eye, TrailPoints[I]) * TrailMinWidthPerDistance);
        Ribbon.Vertices[I * 2] = TrailPoints[I] - Side * HalfWidth;
        Ribbon.Vertices[I * 2 + 1] = TrailPoints[I] + Side * HalfWidth;
    }
    if (bTopologyChanged)
        Ribbon.Mesh->CreateMeshSection_LinearColor(0, Ribbon.Vertices, Ribbon.Indices, {}, Ribbon.UVs, {}, {}, false);
    else
        Ribbon.Mesh->UpdateMeshSection_LinearColor(0, Ribbon.Vertices, {}, Ribbon.UVs, {}, {});
    Ribbon.MeshPointCount = TrailPoints.Num();
}

#if WITH_DEV_AUTOMATION_TESTS
void AUOpenGolfBall::UpdateTrailGeometryLegacy(FUOpenGolfTrailRibbon& Ribbon)
{
    const TArray<FVector>& TrailPoints = Ribbon.Points;
    const auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0);
    if (!Camera || TrailPoints.Num() < 2)
        return;
    TArray<FVector> Vertices;
    TArray<int32> Indices;
    TArray<FVector2D> UVs;
    const FVector Eye = Camera->GetCameraLocation();
    FVector PreviousSide = FVector::ZeroVector;
    for (int32 I = 0; I < TrailPoints.Num(); ++I)
    {
        const FVector Tangent = (TrailPoints[FMath::Min(I + 1, TrailPoints.Num() - 1)] - TrailPoints[FMath::Max(0, I - 1)]).GetSafeNormal();
        FVector Side = FVector::CrossProduct(Tangent, Eye - TrailPoints[I]).GetSafeNormal();
        if (Side.IsNearlyZero())
            Side = PreviousSide.IsNearlyZero() ? FVector::RightVector : PreviousSide;
        if (!PreviousSide.IsNearlyZero() && FVector::DotProduct(Side, PreviousSide) < 0)
            Side *= -1;
        PreviousSide = Side;
        Vertices.Add(TrailPoints[I] - Side * TrailWidthCm * 0.5f);
        Vertices.Add(TrailPoints[I] + Side * TrailWidthCm * 0.5f);
        UVs.Add(FVector2D(0, 0));
        UVs.Add(FVector2D(1, 0));
        if (I > 0)
        {
            const int32 V = I * 2;
            Indices.Append({V - 2, V - 1, V, V - 1, V + 1, V});
        }
    }
    Ribbon.Mesh->CreateMeshSection_LinearColor(0, Vertices, Indices, {}, UVs, {}, {}, false);
}

#endif
