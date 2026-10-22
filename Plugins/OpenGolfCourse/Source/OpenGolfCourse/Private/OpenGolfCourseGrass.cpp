// SPDX-License-Identifier: MIT
#include "OpenGolfTerrain.h"
#include "Algo/BinarySearch.h"

namespace
{
    enum class ECellFit : uint8
    {
        Inside,
        Outside,
        Straddles
    };

    /** Whether a cell lies wholly past Limit (Inside), wholly short of it (Outside), or across it. Distance is from its centre. */
    ECellFit FitBeyond(double Distance, double Limit, double HalfDiagonal)
    {
        if (Distance >= Limit + HalfDiagonal)
            return ECellFit::Inside;
        return Distance + HalfDiagonal < Limit ? ECellFit::Outside : ECellFit::Straddles;
    }

    FQuat GroundRotation(const FVector& Normal, double YawRad)
    {
        return FQuat(Normal, YawRad) * FQuat::FindBetweenNormals(FVector::UpVector, Normal);
    }

    double BoxDistance(const FBox2D& Box, FVector2D Point)
    {
        return FVector2D(FMath::Max3(Box.Min.X - Point.X, 0.0, Point.X - Box.Max.X), FMath::Max3(Box.Min.Y - Point.Y, 0.0, Point.Y - Box.Max.Y)).Size();
    }
} // namespace

bool UOpenGolfTerrainDefinition::ScatterTiles(const UOpenGolfVegetationPreset& P, const TArray<FVector2D>* AreaOutlineM, FGroundQuery Ground,
    TArray<FOpenGolfScatterPoint>& OutPoints, FString& Error, double TerrainYawDeg) const
{
    FBox2D Bounds(DetailMinimumM, DetailMaximumM);
    if (AreaOutlineM)
    {
        const FBox2D Area(*AreaOutlineM);
        Bounds = FBox2D(FVector2D::Max(Area.Min, MinimumM), FVector2D::Min(Area.Max, MaximumM));
    }
    const double Top = P.Tiles[0].SizeM;
    const int32 Smallest = P.Tiles.Num() - 1;
    const double Clearance = P.BoundaryClearanceM;
    const double MinimumFromPlay = P.MinimumDistanceFromPlayM, MaximumFromPlay = P.MaximumDistanceFromPlayM;
    const double CosTilt = FMath::Cos(FMath::DegreesToRadians(P.MaximumTileTiltDeg));
    const double CosSlope = FMath::Cos(FMath::DegreesToRadians(P.MaximumSlopeDeg));
    const double Yaw = FMath::DegreesToRadians(TerrainYawDeg);
    // A banded preset only looks near play; boxes reject far cells before any outline is measured.
    TArray<FBox2D> Play;
    for (const auto& Region : Regions)
        if (Region.Surface && Region.Surface->Surface != EOpenFairwaySurface::Rough && Region.OutlineM.Num() >= 3)
            Play.Emplace(Region.OutlineM);
    FRandomStream Random(P.Seed);
    bool bOverBudget = false;

    // Keeps, drops or halves one cell. Every test either decides the whole cell or asks for a closer look.
    auto Visit = [&](auto& Self, FVector2D Center, int32 Level) -> void
    {
        if (bOverBudget)
            return;
        const double Half = P.Tiles[Level].SizeM * 0.5;
        const bool bSmallest = Level == Smallest;
        // The smallest tile is judged at its centre; larger ones by their whole footprint.
        const double Reach = bSmallest ? 0.0 : Half * UE_SQRT_2;
        bool bSplit = false;
        auto Judge = [&](ECellFit Fit)
        {
            if (Fit == ECellFit::Outside)
                return false;
            bSplit |= Fit == ECellFit::Straddles;
            return true;
        };

        const FBox2D Cell(Center - FVector2D(Half), Center + FVector2D(Half));
        if (bSmallest ? !Bounds.IsInsideOrOn(Center) : !Bounds.Intersect(Cell))
            return;
        bSplit |= !bSmallest && !Bounds.IsInside(Cell);
        if (MaximumFromPlay > 0)
        {
            bool bNear = false;
            for (const FBox2D& Box : Play)
                bNear |= BoxDistance(Box, Center) <= MaximumFromPlay + Half * UE_SQRT_2;
            if (!bNear)
                return;
        }
        if (MinimumFromPlay > 0 || MaximumFromPlay > 0)
        {
            const double FromPlay = DistanceFromPlay(Center);
            if (FromPlay < MinimumFromPlay || (MaximumFromPlay > 0 && FromPlay > MaximumFromPlay))
                return;
        }
        if (AreaOutlineM && !Judge(FitBeyond(Contains(*AreaOutlineM, Center) ? DistanceToOutline(*AreaOutlineM, Center) : -DistanceToOutline(*AreaOutlineM, Center), 0, Reach)))
            return;
        if (SurfaceAt(Center)->Surface != P.Surface)
        {
            // Off the surface: drop, unless the cell reaches back onto it.
            double Edge = TNumericLimits<double>::Max();
            for (const auto& Region : Regions)
                Edge = FMath::Min(Edge, DistanceToOutline(Region.OutlineM, Center));
            if (bSmallest || Edge >= Reach)
                return;
            bSplit = true;
        }
        else
        {
            double Edge = TNumericLimits<double>::Max();
            for (const auto& Region : Regions)
                Edge = FMath::Min(Edge, DistanceToOutline(Region.OutlineM, Center));
            if (!Judge(FitBeyond(Edge, Clearance, Reach)))
                return;
        }
        if (LevelTeeRadiusM > 0 && !Judge(FitBeyond((Center - LevelTeeCenterM).Size(), LevelTeeRadiusM + Clearance, Reach)))
            return;
        if (!bSplit && !bSmallest)
        {
            // Curved ground: a rigid tile would float at its corners, so smaller tiles follow it.
            const FVector Middle = NormalAt(Center);
            for (const FVector2D Corner : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1)})
                bSplit |= FVector::DotProduct(NormalAt(Center + Corner * Half), Middle) < CosTilt;
        }
        if (bSplit && !bSmallest)
        {
            const double Quarter = Half * 0.5;
            for (const FVector2D Offset : {FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1)})
                Self(Self, Center + Offset * Quarter, Level + 1);
            return;
        }
        FVector Position, Normal;
        if (!Ground(Center, Position, Normal) || Normal.Z < CosSlope)
            return;
        auto& Point = OutPoints.Emplace_GetRef();
        // Quarter turns keep the square on the grid; the terrain's yaw keeps the grid on the terrain.
        Point.Transform = FTransform(GroundRotation(Normal, Yaw + HALF_PI * Random.RandHelper(4)), Position - Normal * P.TileSinkM * 100);
        Point.Seed = Random.RandHelper(MAX_int32);
        Point.Tile = Level;
        bOverBudget = OutPoints.Num() > MaxScatterPoints;
    };

    const FVector2D First(FMath::FloorToDouble(Bounds.Min.X / Top) * Top + Top * 0.5, FMath::FloorToDouble(Bounds.Min.Y / Top) * Top + Top * 0.5);
    for (double Y = First.Y; Y - Top * 0.5 < Bounds.Max.Y && !bOverBudget; Y += Top)
        for (double X = First.X; X - Top * 0.5 < Bounds.Max.X && !bOverBudget; X += Top)
            Visit(Visit, FVector2D(X, Y), 0);
    if (bOverBudget)
    {
        OutPoints.Reset();
        Error = TEXT("Tiled vegetation exceeds 100,000 tiles per preset. Narrow its distance band or use larger tiles.");
        return false;
    }
    return true;
}

bool UOpenGolfTerrainDefinition::ScatterBoundary(const UOpenGolfVegetationPreset& P, FGroundQuery Ground, TArray<FOpenGolfScatterPoint>& OutPoints,
    FString& Error, double TerrainYawDeg) const
{
    OutPoints.Reset();
    Error.Reset();
    const FVector2D Strip = P.EdgeStripSizeM;
    if (!DefaultSurface || P.Kind != EOpenGolfVegetationKind::GroundCover || !FMath::IsFinite(Strip.X) || !FMath::IsFinite(Strip.Y) ||
        Strip.X < 0.1 || Strip.Y < 0.05 || !FMath::IsFinite(P.BoundaryClearanceM) || P.BoundaryClearanceM < 0 ||
        !FMath::IsFinite(P.ScaleRange.X) || !FMath::IsFinite(P.ScaleRange.Y) || P.ScaleRange.X <= 0 || P.ScaleRange.Y < P.ScaleRange.X ||
        !FMath::IsFinite(P.TileSinkM) || P.TileSinkM < 0 ||
        !FMath::IsFinite(P.MaximumSlopeDeg) || P.MaximumSlopeDeg < 0 || P.MaximumSlopeDeg > 60)
    {
        Error = TEXT("Boundary grass needs ground cover with a strip of at least 0.1 x 0.05 m, a clearance and a valid slope limit.");
        return false;
    }
    // Strips overlap by 15%, so a bend opens no gap between neighbours.
    const double Step = Strip.X * 0.85;
    const double Offset = P.BoundaryClearanceM + Strip.Y * 0.5;
    const double CosSlope = FMath::Cos(FMath::DegreesToRadians(P.MaximumSlopeDeg));
    const double Yaw = FMath::DegreesToRadians(TerrainYawDeg);
    double Candidates = 0;
    for (const auto& Region : Regions)
        for (int32 I = 0; I < Region.OutlineM.Num(); ++I)
            Candidates += (Region.OutlineM[(I + 1) % Region.OutlineM.Num()] - Region.OutlineM[I]).Size() / Step;
    if (Candidates > MaxScatterPoints)
    {
        Error = TEXT("Boundary grass exceeds 100,000 strips. Use longer strips.");
        return false;
    }
    // Distance to the nearest region on another surface; a strip must stay clear of every one, not just its own.
    auto FromOtherSurfaces = [this, &P](FVector2D Point)
    {
        double Distance = TNumericLimits<double>::Max();
        for (const auto& Region : Regions)
            if (Region.Surface && Region.Surface->Surface != P.Surface)
                Distance = FMath::Min(Distance, Contains(Region.OutlineM, Point) ? 0.0 : DistanceToOutline(Region.OutlineM, Point));
        return Distance;
    };
    FRandomStream Random(P.Seed);
    for (const auto& Region : Regions)
    {
        const TArray<FVector2D>& Outline = Region.OutlineM;
        if (!Region.Surface || Region.Surface->Surface == P.Surface || Outline.Num() < 3)
            continue;
        TArray<double> Along = {0};
        double SignedArea = 0;
        for (int32 I = 0; I < Outline.Num(); ++I)
        {
            const FVector2D A = Outline[I], B = Outline[(I + 1) % Outline.Num()];
            Along.Add(Along.Last() + (B - A).Size());
            SignedArea += A.X * B.Y - B.X * A.Y;
        }
        const double Perimeter = Along.Last();
        if (Perimeter < UE_SMALL_NUMBER)
            continue;
        // Outward is to the right of a counter-clockwise outline.
        const double Outward = SignedArea > 0 ? 1 : -1;
        auto PointAt = [&](double S)
        {
            S = FMath::Fmod(FMath::Fmod(S, Perimeter) + Perimeter, Perimeter);
            const int32 I = FMath::Clamp(Algo::UpperBound(Along, S) - 1, 0, Outline.Num() - 1);
            const double Length = Along[I + 1] - Along[I];
            return FMath::Lerp(Outline[I], Outline[(I + 1) % Outline.Num()], Length > UE_SMALL_NUMBER ? (S - Along[I]) / Length : 0.0);
        };
        const int32 Count = FMath::Max(1, FMath::CeilToInt(Perimeter / Step));
        const double Spacing = Perimeter / Count;
        for (int32 Index = 0; Index < Count; ++Index)
        {
            const double S = (Index + 0.5) * Spacing;
            // The tangent spans the whole strip, so a strip lies along the edge rather than one short segment.
            const FVector2D Tangent = (PointAt(S + Strip.X * 0.5) - PointAt(S - Strip.X * 0.5)).GetSafeNormal();
            if (Tangent.IsNearlyZero())
                continue;
            const FVector2D Center = PointAt(S) + FVector2D(Tangent.Y, -Tangent.X) * Outward * Offset;
            if (Center.X < DetailMinimumM.X || Center.Y < DetailMinimumM.Y || Center.X > DetailMaximumM.X || Center.Y > DetailMaximumM.Y ||
                SurfaceAt(Center)->Surface != P.Surface || FromOtherSurfaces(Center) < P.BoundaryClearanceM + Strip.Y * 0.4 ||
                (LevelTeeRadiusM > 0 && (Center - LevelTeeCenterM).Size() < LevelTeeRadiusM + Offset))
                continue;
            FVector Position, Normal;
            if (!Ground(Center, Position, Normal) || Normal.Z < CosSlope)
                continue;
            // Either way round keeps the strip's footprint; flipping varies the tufts along an edge.
            const double Heading = Yaw + FMath::Atan2(Tangent.Y, Tangent.X) + PI * Random.RandHelper(2);
            auto& Point = OutPoints.Emplace_GetRef();
            Point.Transform = FTransform(GroundRotation(Normal, Heading), Position - Normal * P.TileSinkM * 100,
                FVector(Random.FRandRange(P.ScaleRange.X, P.ScaleRange.Y)));
            Point.Seed = Random.RandHelper(MAX_int32);
        }
    }
    return true;
}
