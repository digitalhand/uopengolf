// SPDX-License-Identifier: MIT
#include "OpenGolfTerrain.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "OpenFairwayBall.h"

namespace
{
    double Cross(FVector2D A, FVector2D B)
    {
        return A.X * B.Y - A.Y * B.X;
    }
    bool SegmentsCross(FVector2D A, FVector2D B, FVector2D C, FVector2D D)
    {
        const double AB = Cross(B - A, C - A) * Cross(B - A, D - A);
        const double CD = Cross(D - C, A - C) * Cross(D - C, B - C);
        return AB < 0 && CD < 0;
    }
    bool Finite(FVector2D V)
    {
        return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y);
    }

    bool ValidPreset(const UOpenGolfVegetationPreset* Preset)
    {
        if (!Preset || !FMath::IsFinite(Preset->FairwayClearanceM) || Preset->FairwayClearanceM < 0 || !FMath::IsFinite(Preset->SpacingM) || Preset->SpacingM < 0.1f || Preset->ScaleRange.X <= 0 ||
            Preset->ScaleRange.Y < Preset->ScaleRange.X || Preset->MaximumSlopeDeg < 0 || Preset->MaximumSlopeDeg > 60)
            return false;
        if (!FMath::IsFinite(Preset->MinimumDistanceFromPlayM) || !FMath::IsFinite(Preset->MaximumDistanceFromPlayM) ||
            Preset->MinimumDistanceFromPlayM < 0 || Preset->MaximumDistanceFromPlayM < 0 ||
            (Preset->MaximumDistanceFromPlayM > 0 && Preset->MaximumDistanceFromPlayM <= Preset->MinimumDistanceFromPlayM))
            return false;
        if (Preset->PlacementPattern != EOpenGolfPlacementPattern::JitteredGrid &&
            Preset->PlacementPattern != EOpenGolfPlacementPattern::RandomSeparated &&
            Preset->PlacementPattern != EOpenGolfPlacementPattern::Tiled)
            return false;
        if (Preset->PlacementPattern == EOpenGolfPlacementPattern::Tiled)
        {
            if (Preset->Kind != EOpenGolfVegetationKind::GroundCover || Preset->Tiles.IsEmpty() ||
                !FMath::IsFinite(Preset->MaximumTileTiltDeg) || Preset->MaximumTileTiltDeg < 0 || Preset->MaximumTileTiltDeg > 45 ||
                !FMath::IsFinite(Preset->TileSinkM) || Preset->TileSinkM < 0 || Preset->TileSinkM > 0.2f ||
                !FMath::IsFinite(Preset->BoundaryClearanceM) || Preset->BoundaryClearanceM < 0)
                return false;
            for (int32 I = 0; I < Preset->Tiles.Num(); ++I)
            {
                const auto& Tile = Preset->Tiles[I];
                // Each size halves the one before, so four smaller tiles fill a larger cell exactly.
                if (!FMath::IsFinite(Tile.SizeM) || Tile.SizeM < 0.05f || Tile.Meshes.IsEmpty() || Tile.Meshes.Contains(nullptr) ||
                    (I > 0 && !FMath::IsNearlyEqual(Tile.SizeM * 2, Preset->Tiles[I - 1].SizeM, 0.001f)))
                    return false;
            }
        }
        if (Preset->PlacementPattern == EOpenGolfPlacementPattern::RandomSeparated &&
            (!FMath::IsFinite(Preset->MinimumSeparationM) || Preset->MinimumSeparationM < 0.1f))
            return false;
        if (Preset->Kind != EOpenGolfVegetationKind::Tree)
            return true;
        // A wood proxy or canopy volume follows one tree shape, so it cannot serve several meshes.
        if ((Preset->TreeCollisionMesh || Preset->TreeCanopyMesh) && Preset->Meshes.Num() > 1)
            return false;
        if (Preset->TreeCollisionMesh)
            return true;
        return (FMath::IsFinite(Preset->TrunkColliderRadiusM) && FMath::IsFinite(Preset->TrunkColliderHeightM) &&
                   Preset->TrunkColliderRadiusM >= 0.05f && Preset->TrunkColliderHeightM >= 0.5f);
    }
} // namespace

const FName AOpenGolfTerrainArea::GeneratedTag(TEXT("OpenGolf.Terrain.Generated"));
const FName AOpenGolfTerrainArea::VegetationTag(TEXT("OpenGolf.Terrain.Vegetation"));
const FName AOpenGolfTerrainArea::TreesTag(TEXT("OpenGolf.Terrain.Trees"));
const FName AOpenGolfTerrainArea::TreeCollidersTag(TEXT("OpenGolf.Terrain.TreeColliders"));
const FName AOpenGolfTerrainArea::TreeCanopyTag(TEXT("OpenGolf.Terrain.TreeCanopy"));

double UOpenGolfTerrainDefinition::DistanceToOutline(const TArray<FVector2D>& Outline, FVector2D Point)
{
    double Distance = TNumericLimits<double>::Max();
    for (int32 I = 0; I < Outline.Num(); ++I)
    {
        const FVector2D A = Outline[I], Edge = Outline[(I + 1) % Outline.Num()] - A;
        const double T = Edge.SizeSquared() > UE_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / Edge.SizeSquared(), 0.0, 1.0) : 0;
        Distance = FMath::Min(Distance, (Point - A - Edge * T).Size());
    }
    return Distance;
}

bool UOpenGolfTerrainDefinition::Contains(const TArray<FVector2D>& Outline, FVector2D Point)
{
    if (Outline.Num() < 3)
        return false;
    if (DistanceToOutline(Outline, Point) < 0.00001)
        return true;
    bool Inside = false;
    for (int32 I = 0, J = Outline.Num() - 1; I < Outline.Num(); J = I++)
    {
        const FVector2D A = Outline[I], B = Outline[J];
        if ((A.Y > Point.Y) != (B.Y > Point.Y) && Point.X < (B.X - A.X) * (Point.Y - A.Y) / (B.Y - A.Y) + A.X)
            Inside = !Inside;
    }
    return Inside;
}

bool UOpenGolfTerrainDefinition::Validate(FString& Error) const
{
    auto Fail = [&Error](const TCHAR* Message)
    { Error = Message; return false; };
    Error.Reset();
    if (!DefaultSurface || !Finite(MinimumM) || !Finite(MaximumM) || !Finite(DetailMinimumM) || !Finite(DetailMaximumM))
        return Fail(TEXT("Terrain needs a default surface and finite bounds."));
    if (!FMath::IsFinite(LandscapeSpacingM) || !FMath::IsFinite(DetailSpacingM) || LandscapeSpacingM < 0.25f || DetailSpacingM < 0.25f)
        return Fail(TEXT("Terrain sample spacing must be at least 0.25 meters."));
    const double Tile = LandscapeSpacingM * 63;
    for (int32 Axis = 0; Axis < 2; ++Axis)
    {
        if (!(MinimumM[Axis] < DetailMinimumM[Axis] && DetailMinimumM[Axis] < DetailMaximumM[Axis] && DetailMaximumM[Axis] < MaximumM[Axis]))
            return Fail(TEXT("The detailed area must sit strictly inside the Landscape bounds."));
        for (double Size : {DetailMinimumM[Axis] - MinimumM[Axis], DetailMaximumM[Axis] - DetailMinimumM[Axis], MaximumM[Axis] - DetailMaximumM[Axis]})
            if (!FMath::IsNearlyEqual(Size / Tile, FMath::RoundToDouble(Size / Tile), 0.00001))
                return Fail(TEXT("Terrain boundaries must align to 63-quad Landscape components."));
    }
    const FVector2D Size = MaximumM - MinimumM;
    if (Size.X / LandscapeSpacingM * Size.Y / LandscapeSpacingM > 2000000 ||
        (DetailMaximumM - DetailMinimumM).X * (DetailMaximumM - DetailMinimumM).Y / FMath::Square(DetailSpacingM) > 200000)
        return Fail(TEXT("Terrain exceeds the two-million Landscape or 200,000 detail sample budget."));
    if (!Finite(LevelTeeCenterM) || !Finite(LevelTeeHalfSizeM) || LevelTeeHalfSizeM.X < 0 || LevelTeeHalfSizeM.Y < 0)
        return Fail(TEXT("The tee box needs a finite center and non-negative half size."));
    for (const auto& Mound : Mounds)
        if (!Finite(Mound.CenterM) || !Finite(Mound.RadiusM) || Mound.RadiusM.X <= 0 || Mound.RadiusM.Y <= 0 || !FMath::IsFinite(Mound.HeightM))
            return Fail(TEXT("Contours need finite centers, heights, and positive radii."));
    for (int32 R = 0; R < Regions.Num(); ++R)
    {
        const auto& Region = Regions[R];
        if (!Region.Surface || Region.Name.IsNone() || Region.OutlineM.Num() < 3 || !FMath::IsFinite(Region.SpacingM) || Region.SpacingM < 0.25f)
            return Fail(TEXT("Every region needs a name, surface, closed outline, and spacing of at least 0.25 meters."));
        double Area = 0;
        for (int32 I = 0; I < Region.OutlineM.Num(); ++I)
        {
            const FVector2D A = Region.OutlineM[I], B = Region.OutlineM[(I + 1) % Region.OutlineM.Num()];
            if (!Finite(A) || A.X <= DetailMinimumM.X || A.Y <= DetailMinimumM.Y || A.X >= DetailMaximumM.X || A.Y >= DetailMaximumM.Y || (B - A).Size() < 0.001)
                return Fail(TEXT("Region outlines must have distinct vertices strictly inside the detailed area."));
            Area += Cross(A, B);
            for (int32 S = 0; S <= R; ++S)
            {
                const auto& Other = Regions[S].OutlineM;
                for (int32 J = 0; J < Other.Num(); ++J)
                    if (SegmentsCross(A, B, Other[J], Other[(J + 1) % Other.Num()]))
                        return Fail(TEXT("Region edges must not cross. Nested outlines are supported."));
            }
        }
        if (FMath::Abs(Area) < 0.001)
            return Fail(TEXT("Region outlines must enclose an area."));
    }
    const TCHAR* PresetError = TEXT("Vegetation presets need valid spacing, positive scales, slope limits, distance bands, and tree collider sizes.");
    for (const UOpenGolfVegetationPreset* Preset : Vegetation)
        if (!ValidPreset(Preset))
            return Fail(PresetError);
    for (const auto& Area : VegetationAreas)
    {
        if (Area.Name.IsNone() || Area.OutlineM.Num() < 3)
            return Fail(TEXT("Every vegetation area needs a name and a closed outline."));
        if (!ValidPreset(Area.Preset))
            return Fail(PresetError);
        for (int32 I = 0; I < Area.OutlineM.Num(); ++I)
        {
            const FVector2D A = Area.OutlineM[I], B = Area.OutlineM[(I + 1) % Area.OutlineM.Num()];
            if (!Finite(A) || A.X < MinimumM.X || A.Y < MinimumM.Y || A.X > MaximumM.X || A.Y > MaximumM.Y || (B - A).Size() < 0.001)
                return Fail(TEXT("Vegetation area outlines must have distinct vertices inside the Landscape bounds."));
        }
    }
    return true;
}

double UOpenGolfTerrainDefinition::DistanceFromPlay(FVector2D PointM) const
{
    double Distance = TNumericLimits<double>::Max();
    for (const auto& Region : Regions)
        if (Region.Surface && Region.Surface->Surface != EOpenFairwaySurface::Rough)
            Distance = FMath::Min(Distance, Contains(Region.OutlineM, PointM) ? 0.0 : DistanceToOutline(Region.OutlineM, PointM));
    return Distance;
}

bool UOpenGolfTerrainDefinition::Scatter(const UOpenGolfVegetationPreset& Preset, const TArray<FVector2D>* AreaOutlineM, FGroundQuery Ground,
    TArray<FOpenGolfScatterPoint>& OutPoints, FString& Error, double TerrainYawDeg) const
{
    Error.Reset();
    OutPoints.Reset();
    if (!DefaultSurface || !ValidPreset(&Preset))
    {
        Error = TEXT("Vegetation needs a default surface and valid placement settings.");
        return false;
    }
    if (Preset.PlacementPattern == EOpenGolfPlacementPattern::Tiled)
        return ScatterTiles(Preset, AreaOutlineM, Ground, OutPoints, Error, TerrainYawDeg);
    FVector2D Minimum = DetailMinimumM, Maximum = DetailMaximumM;
    if (AreaOutlineM)
    {
        const FBox2D Box(*AreaOutlineM);
        Minimum = FVector2D::Max(Box.Min, MinimumM);
        Maximum = FVector2D::Min(Box.Max, MaximumM);
    }
    const bool bTree = Preset.Kind == EOpenGolfVegetationKind::Tree;
    const bool bBanded = Preset.MinimumDistanceFromPlayM > 0 || Preset.MaximumDistanceFromPlayM > 0;
    FRandomStream Random(Preset.Seed);
    const double Step = Preset.SpacingM;
    const bool bRandom = Preset.PlacementPattern == EOpenGolfPlacementPattern::RandomSeparated;
    TMap<FIntPoint, TArray<FVector2D>> Occupied;
    auto CellAt = [&Preset](FVector2D P)
    {
        return FIntPoint(FMath::FloorToInt(P.X / Preset.MinimumSeparationM), FMath::FloorToInt(P.Y / Preset.MinimumSeparationM));
    };
    auto Place = [&](FVector2D P)
    {
        if ((AreaOutlineM && !Contains(*AreaOutlineM, P)) || SurfaceAt(P)->Surface != Preset.Surface ||
            TeeBoxDistance(P) < 0.5)
            return;
        for (const auto& R : Regions)
            if (DistanceToOutline(R.OutlineM, P) < Preset.BoundaryClearanceM)
                return;
        if (bBanded)
        {
            const double FromPlay = Preset.bUseDistanceSurface ? DistanceFromSurface(P, Preset.DistanceSurface) : DistanceFromPlay(P);
            if (FromPlay < Preset.MinimumDistanceFromPlayM || (Preset.MaximumDistanceFromPlayM > 0 && FromPlay > Preset.MaximumDistanceFromPlayM))
                return;
        }
        if (Preset.FairwayClearanceM > 0 && DistanceFromSurface(P, EOpenFairwaySurface::Fairway) < Preset.FairwayClearanceM)
            return;
        FIntPoint Cell = FIntPoint::ZeroValue;
        if (bRandom)
        {
            Cell = CellAt(P);
            for (int32 DY = -1; DY <= 1; ++DY)
                for (int32 DX = -1; DX <= 1; ++DX)
                    if (const auto* Neighbors = Occupied.Find(Cell + FIntPoint(DX, DY)))
                        for (const FVector2D Other : *Neighbors)
                            if ((P - Other).SizeSquared() < FMath::Square(Preset.MinimumSeparationM))
                                return;
        }
        FVector GroundPosition, Normal;
        if (!Ground(P, GroundPosition, Normal) || Normal.Z < FMath::Cos(FMath::DegreesToRadians(Preset.MaximumSlopeDeg)))
            return;
        FOpenGolfScatterPoint& Point = OutPoints.Emplace_GetRef();
        const FQuat Rotation = bTree ? FQuat(FVector::UpVector, Random.FRandRange(0.f, 2.f * PI))
                                     : FQuat(Normal, Random.FRandRange(0.f, 2.f * PI)) * FQuat::FindBetweenNormals(FVector::UpVector, Normal);
        const double Scale = Random.FRandRange(Preset.ScaleRange.X, Preset.ScaleRange.Y);
        Point.Transform = FTransform(Rotation, GroundPosition, FVector(Scale));
        Point.Seed = Random.RandHelper(MAX_int32);
        if (bRandom)
            Occupied.FindOrAdd(Cell).Add(P);
    };
    if (bRandom)
    {
        const FVector2D Size = Maximum - Minimum;
        if (Size.X <= 0 || Size.Y <= 0)
            return true;
        double Area = Size.X * Size.Y;
        if (AreaOutlineM)
        {
            Area = 0;
            for (int32 I = 0; I < AreaOutlineM->Num(); ++I)
                Area += Cross((*AreaOutlineM)[I], (*AreaOutlineM)[(I + 1) % AreaOutlineM->Num()]);
            Area = FMath::Min(FMath::Abs(Area) * 0.5, Size.X * Size.Y);
        }
        const double Requested = FMath::CeilToDouble(Area / FMath::Square(Step));
        if (Requested > MaxScatterPoints)
        {
            Error = TEXT("Vegetation exceeds 100,000 candidates per preset. Increase spacing.");
            return false;
        }
        const int32 Target = static_cast<int32>(Requested);
        for (int32 Attempt = 0; Attempt < Target * 24 && OutPoints.Num() < Target; ++Attempt)
            Place(FVector2D(Random.FRandRange(Minimum.X, Maximum.X), Random.FRandRange(Minimum.Y, Maximum.Y)));
    }
    else
    {
        // Keep the original grid and random draw order for existing grass presets.
        const double FirstOffset = AreaOutlineM ? Step * 0.5 : Step;
        const double EndMargin = AreaOutlineM ? 0.0 : Step;
        for (double Y = Minimum.Y + FirstOffset; Y < Maximum.Y - EndMargin; Y += Step)
            for (double X = Minimum.X + FirstOffset; X < Maximum.X - EndMargin; X += Step)
            {
                Place(FVector2D(X + Random.FRandRange(-0.4f, 0.4f) * Step, Y + Random.FRandRange(-0.4f, 0.4f) * Step));
                if (OutPoints.Num() > MaxScatterPoints)
                {
                    Error = TEXT("Vegetation exceeds 100,000 instances per preset. Increase spacing.");
                    OutPoints.Reset();
                    return false;
                }
            }
    }
    return true;
}

double UOpenGolfTerrainDefinition::HeightAt(FVector2D PointM) const
{
    double Height = 0;
    for (const auto& Mound : Mounds)
    {
        if (Mound.RadiusM.X <= 0 || Mound.RadiusM.Y <= 0)
            continue;
        const FVector2D Delta = (PointM - Mound.CenterM) / Mound.RadiusM;
        Height += Mound.HeightM * FMath::Exp(-Delta.SizeSquared() * 2.0);
    }
    if (const double Tee = TeeBoxDistance(PointM); Tee < TNumericLimits<double>::Max())
    {
        // Level across the box, then ease back to the contours over one half-side.
        const double T = FMath::Clamp(Tee / LevelTeeHalfSizeM.GetMax(), 0.0, 1.0);
        Height *= T * T * (3 - 2 * T);
    }
    return Height;
}

FVector UOpenGolfTerrainDefinition::NormalAt(FVector2D PointM) const
{
    constexpr double Step = 0.02;
    const double DX = HeightAt(PointM + FVector2D(Step, 0)) - HeightAt(PointM - FVector2D(Step, 0));
    const double DY = HeightAt(PointM + FVector2D(0, Step)) - HeightAt(PointM - FVector2D(0, Step));
    return FVector(-DX / (2 * Step), -DY / (2 * Step), 1).GetSafeNormal();
}

double UOpenGolfTerrainDefinition::TeeBoxDistance(FVector2D PointM) const
{
    if (LevelTeeHalfSizeM.X <= 0 || LevelTeeHalfSizeM.Y <= 0)
        return TNumericLimits<double>::Max();
    const FVector2D Q = (PointM - LevelTeeCenterM).GetAbs() - LevelTeeHalfSizeM;
    return FVector2D::Max(Q, FVector2D::ZeroVector).Size() + FMath::Min(Q.GetMax(), 0.0);
}

double UOpenGolfTerrainDefinition::DistanceFromSurface(FVector2D PointM, EOpenFairwaySurface Surface) const
{
    double Distance = TNumericLimits<double>::Max();
    for (const auto& Region : Regions)
        if (Region.Surface && Region.Surface->Surface == Surface)
            Distance = FMath::Min(Distance, Contains(Region.OutlineM, PointM) ? 0.0 : DistanceToOutline(Region.OutlineM, PointM));
    return Distance;
}

UOpenGolfSurfacePreset* UOpenGolfTerrainDefinition::SurfaceAt(FVector2D PointM) const
{
    for (int32 I = Regions.Num() - 1; I >= 0; --I)
        if (Contains(Regions[I].OutlineM, PointM))
            return Regions[I].Surface;
    return DefaultSurface;
}

AOpenGolfTerrainArea::AOpenGolfTerrainArea()
{
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("TerrainRoot"));
    RootComponent->SetMobility(EComponentMobility::Static);
}

bool AOpenGolfTerrainArea::GroundAt(FVector WorldPosition, FVector& GroundPosition, FVector& GroundNormal) const
{
    if (!GetWorld())
        return false;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OpenGolfTerrainGround), false);
    Params.bReturnPhysicalMaterial = true;
    for (const AActor* Actor : GeneratedActors)
        if (IsValid(Actor) && Actor->ActorHasTag(TreeCollidersTag))
            Params.AddIgnoredActor(Actor);
    constexpr int32 MaxObstacles = 8;
    for (int32 Attempt = 0;; ++Attempt)
    {
        if (!GetWorld()->LineTraceSingleByChannel(Hit, WorldPosition + FVector(0, 0, 50000), WorldPosition - FVector(0, 0, 50000), ECC_Visibility, Params))
            return false;
        if (!AOpenFairwayBall::IsObstacle(Hit.GetComponent()))
            break;
        if (Attempt == MaxObstacles)
            return false;
        Params.AddIgnoredComponent(Hit.GetComponent());
    }
    GroundPosition = Hit.ImpactPoint;
    GroundNormal = Hit.ImpactNormal;
    return true;
}
