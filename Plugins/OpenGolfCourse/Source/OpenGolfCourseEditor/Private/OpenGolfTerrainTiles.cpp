// SPDX-License-Identifier: MIT
#include "OpenGolfTerrainTiles.h"
#include "CompGeom/Delaunay2.h"
#include "OpenGolfTerrain.h"

namespace
{
    using UE::Geometry::FIndex2i;
    using UE::Geometry::FIndex3i;
    using OpenGolfTerrainTiles::EdgeSpacingM;
    using OpenGolfTerrainTiles::SnapM;

    /** One region as seen by a square: its outline pieces inside the square and its outline clipped to it. */
    struct FClipped
    {
        int32 RegionIndex = INDEX_NONE;
        UOpenGolfSurfacePreset* Surface = nullptr;
        double SpacingM = 1;
        TArray<TPair<FVector2D, FVector2D>> Pieces;
        TArray<FVector2D> Polygon;
        FBox2D Bounds;
    };

    /**
     * Where segment A-B meets a square's side. Both squares sharing a side clip the same outline segment through
     * this one formula, so they get bit-identical crossings.
     */
    double CrossY(FVector2D A, FVector2D B, double X)
    {
        if (B.X < A.X || (B.X == A.X && B.Y < A.Y))
            Swap(A, B);
        return A.Y + (X - A.X) * (B.Y - A.Y) / (B.X - A.X);
    }

    double CrossX(FVector2D A, FVector2D B, double Y)
    {
        if (B.Y < A.Y || (B.Y == A.Y && B.X < A.X))
            Swap(A, B);
        return A.X + (Y - A.Y) * (B.X - A.X) / (B.Y - A.Y);
    }

    /** Moves a crossing onto the nearest regular edge vertex when it is closer than SnapM, so no sliver forms. */
    double Snap(double Value, double SideStart)
    {
        const double Regular = SideStart + FMath::RoundToDouble((Value - SideStart) / EdgeSpacingM) * EdgeSpacingM;
        return FMath::Abs(Regular - Value) < SnapM ? Regular : Value;
    }

    /** The point where A-B enters or leaves Box through Side (0 west, 1 east, 2 south, 3 north). */
    FVector2D OnSide(FVector2D A, FVector2D B, double T, int32 Side, const FBox2D& Box)
    {
        switch (Side)
        {
        case 0: return FVector2D(Box.Min.X, Snap(FMath::Clamp(CrossY(A, B, Box.Min.X), Box.Min.Y, Box.Max.Y), Box.Min.Y));
        case 1: return FVector2D(Box.Max.X, Snap(FMath::Clamp(CrossY(A, B, Box.Max.X), Box.Min.Y, Box.Max.Y), Box.Min.Y));
        case 2: return FVector2D(Snap(FMath::Clamp(CrossX(A, B, Box.Min.Y), Box.Min.X, Box.Max.X), Box.Min.X), Box.Min.Y);
        case 3: return FVector2D(Snap(FMath::Clamp(CrossX(A, B, Box.Max.Y), Box.Min.X, Box.Max.X), Box.Min.X), Box.Max.Y);
        default: return T <= 0 ? A : T >= 1 ? B : A + (B - A) * T;
        }
    }

    /** Liang-Barsky clipping of A-B to Box; crossings land exactly on the square's sides. */
    bool ClipSegment(FVector2D A, FVector2D B, const FBox2D& Box, FVector2D& P, FVector2D& Q)
    {
        const FVector2D D = B - A;
        double T0 = 0, T1 = 1;
        int32 Side0 = INDEX_NONE, Side1 = INDEX_NONE;
        auto Clip = [&](double Num, double Limit, int32 Side)
        {
            if (Num == 0)
                return Limit >= 0;
            const double R = Limit / Num;
            if (Num < 0)
            {
                if (R > T1) return false;
                if (R > T0) { T0 = R; Side0 = Side; }
            }
            else
            {
                if (R < T0) return false;
                if (R < T1) { T1 = R; Side1 = Side; }
            }
            return true;
        };
        if (!Clip(-D.X, A.X - Box.Min.X, 0) || !Clip(D.X, Box.Max.X - A.X, 1) || !Clip(-D.Y, A.Y - Box.Min.Y, 2) ||
            !Clip(D.Y, Box.Max.Y - A.Y, 3) || T0 >= T1)
            return false;
        P = OnSide(A, B, T0, Side0, Box);
        Q = OnSide(A, B, T1, Side1, Box);
        const bool bAlongSide = (P.X == Q.X && (P.X == Box.Min.X || P.X == Box.Max.X)) ||
            (P.Y == Q.Y && (P.Y == Box.Min.Y || P.Y == Box.Max.Y));
        return P != Q && !bAlongSide;
    }

    /** Sutherland-Hodgman: the outline cut to the square, used only to test which surface a point lies on. */
    TArray<FVector2D> ClipPolygon(const TArray<FVector2D>& Outline, const FBox2D& Box)
    {
        TArray<FVector2D> Result = Outline;
        for (int32 Side = 0; Side < 4 && !Result.IsEmpty(); ++Side)
        {
            auto Inside = [&](FVector2D P)
            {
                return Side == 0 ? P.X >= Box.Min.X : Side == 1 ? P.X <= Box.Max.X : Side == 2 ? P.Y >= Box.Min.Y : P.Y <= Box.Max.Y;
            };
            auto Cross = [&](FVector2D A, FVector2D B)
            {
                return Side < 2 ? FVector2D(Side == 0 ? Box.Min.X : Box.Max.X, CrossY(A, B, Side == 0 ? Box.Min.X : Box.Max.X))
                                : FVector2D(CrossX(A, B, Side == 2 ? Box.Min.Y : Box.Max.Y), Side == 2 ? Box.Min.Y : Box.Max.Y);
            };
            TArray<FVector2D> Input = MoveTemp(Result);
            Result.Reset();
            for (int32 I = 0; I < Input.Num(); ++I)
            {
                const FVector2D A = Input[I], B = Input[(I + 1) % Input.Num()];
                if (Inside(B))
                {
                    if (!Inside(A))
                        Result.Add(Cross(A, B));
                    Result.Add(B);
                }
                else if (Inside(A))
                    Result.Add(Cross(A, B));
            }
        }
        return Result;
    }

    TArray<FClipped> ClipRegions(const UOpenGolfTerrainDefinition& D, const FBox2D& Box)
    {
        TArray<FClipped> Result;
        for (int32 R = 0; R < D.Regions.Num(); ++R)
        {
            const auto& Region = D.Regions[R];
            if (Region.OutlineM.Num() < 3 || !Region.Surface || !FBox2D(Region.OutlineM).Intersect(Box))
                continue;
            FClipped Clipped;
            Clipped.RegionIndex = R;
            Clipped.Surface = Region.Surface;
            Clipped.SpacingM = FMath::Max(Region.SpacingM, 0.1f);
            for (int32 I = 0; I < Region.OutlineM.Num(); ++I)
            {
                FVector2D P, Q;
                if (ClipSegment(Region.OutlineM[I], Region.OutlineM[(I + 1) % Region.OutlineM.Num()], Box, P, Q))
                    Clipped.Pieces.Emplace(P, Q);
            }
            Clipped.Polygon = ClipPolygon(Region.OutlineM, Box);
            if (Clipped.Polygon.Num() < 3)
                continue;
            Clipped.Bounds = FBox2D(Clipped.Polygon);
            Result.Add(MoveTemp(Clipped));
        }
        return Result;
    }

    struct FBuilder
    {
        TArray<FVector2D>& Points;
        TArray<FIndex2i> Edges;
        TMap<FVector2D, int32> Lookup;

        int32 Add(FVector2D P)
        {
            if (const int32* Found = Lookup.Find(P))
                return *Found;
            return Lookup.Add(P, Points.Add(P));
        }

        void AddEdge(int32 A, int32 B)
        {
            if (A != B)
                Edges.Emplace(A, B);
        }

        /**
         * The square's edge loop: regular vertices plus every outline crossing, in order around the square. Points are
         * kept exactly as computed, never rebuilt from an offset, so a neighbour sharing the side gets the same bits.
         */
        void AddBoundary(const FBox2D& Box, const TArray<FClipped>& Regions)
        {
            const int32 Steps = FMath::RoundToInt((Box.Max.X - Box.Min.X) / EdgeSpacingM);
            // Each side runs anticlockwise: south west to east, east south to north, north east to west, west north to south.
            TArray<TArray<TPair<double, FVector2D>>> Sides;
            Sides.SetNum(4);
            for (int32 K = 0; K <= Steps; ++K)
            {
                const double T = K * EdgeSpacingM;
                Sides[0].Emplace(T, FVector2D(Box.Min.X + T, Box.Min.Y));
                Sides[1].Emplace(T, FVector2D(Box.Max.X, Box.Min.Y + T));
                Sides[2].Emplace(T, FVector2D(Box.Max.X - T, Box.Max.Y));
                Sides[3].Emplace(T, FVector2D(Box.Min.X, Box.Max.Y - T));
            }
            auto AddCrossing = [&](FVector2D P)
            {
                if (P.Y == Box.Min.Y) Sides[0].Emplace(P.X - Box.Min.X, P);
                if (P.X == Box.Max.X) Sides[1].Emplace(P.Y - Box.Min.Y, P);
                if (P.Y == Box.Max.Y) Sides[2].Emplace(Box.Max.X - P.X, P);
                if (P.X == Box.Min.X) Sides[3].Emplace(Box.Max.Y - P.Y, P);
            };
            for (const auto& Region : Regions)
                for (const auto& [P, Q] : Region.Pieces)
                {
                    AddCrossing(P);
                    AddCrossing(Q);
                }
            TArray<int32> Loop;
            for (auto& Side : Sides)
            {
                Side.Sort([](const auto& A, const auto& B) { return A.Key < B.Key; });
                for (const auto& [T, P] : Side)
                {
                    const int32 Index = Add(P);
                    if (Loop.IsEmpty() || Loop.Last() != Index)
                        Loop.Add(Index);
                }
            }
            if (Loop.Num() > 1 && Loop[0] == Loop.Last())
                Loop.Pop();
            for (int32 I = 0; I < Loop.Num(); ++I)
                AddEdge(Loop[I], Loop[(I + 1) % Loop.Num()]);
        }

        void AddPiece(FVector2D P, FVector2D Q, double Spacing)
        {
            const int32 Steps = FMath::Max(1, FMath::CeilToInt(FVector2D::Distance(P, Q) / Spacing));
            int32 Previous = Add(P);
            for (int32 K = 1; K <= Steps; ++K)
            {
                const int32 Next = Add(K == Steps ? Q : FMath::Lerp(P, Q, static_cast<double>(K) / Steps));
                AddEdge(Previous, Next);
                Previous = Next;
            }
        }
    };

    double DistanceToSegment(FVector2D P, FVector2D A, FVector2D B)
    {
        const FVector2D D = B - A;
        const double Length = D.SizeSquared();
        const double T = Length > 0 ? FMath::Clamp(((P - A) | D) / Length, 0.0, 1.0) : 0.0;
        return FVector2D::Distance(P, A + D * T);
    }

    bool NearOutline(FVector2D P, const TArray<FClipped>& Regions, double Distance)
    {
        for (const auto& Region : Regions)
        {
            if (!Region.Bounds.ExpandBy(Distance).IsInside(P))
                continue;
            for (const auto& [A, B] : Region.Pieces)
                if (DistanceToSegment(P, A, B) < Distance)
                    return true;
        }
        return false;
    }

    bool Inside(const FClipped& Region, FVector2D P)
    {
        return Region.Bounds.IsInside(P) && UOpenGolfTerrainDefinition::Contains(Region.Polygon, P);
    }

    /**
     * True if a finer region owns P, so that region samples it instead. Between two regions of equal spacing the later
     * one samples; the rough grid (Self none) gives way only to finer regions, so equally spaced ones keep its points.
     */
    bool OwnedByFiner(const TArray<FClipped>& Regions, int32 Self, double Spacing, FVector2D P)
    {
        for (int32 I = 0; I < Regions.Num(); ++I)
        {
            const bool bFiner = Regions[I].SpacingM < Spacing || (Self != INDEX_NONE && Regions[I].SpacingM == Spacing && I > Self);
            if (I != Self && bFiner && Inside(Regions[I], P))
                return true;
        }
        return false;
    }

    void AddGrid(FBuilder& Builder, const FBox2D& Square, const FBox2D& Area, double Spacing, const TArray<FClipped>& Regions, int32 Self)
    {
        const double Border = EdgeSpacingM * 0.5;
        const double Start = Square.Min.X + Spacing * 0.5, StartY = Square.Min.Y + Spacing * 0.5;
        const int32 I0 = FMath::Max(0, FMath::FloorToInt((Area.Min.X - Start) / Spacing));
        const int32 J0 = FMath::Max(0, FMath::FloorToInt((Area.Min.Y - StartY) / Spacing));
        for (double Y = StartY + J0 * Spacing; Y <= FMath::Min(Area.Max.Y, Square.Max.Y - Border); Y += Spacing)
            for (double X = Start + I0 * Spacing; X <= FMath::Min(Area.Max.X, Square.Max.X - Border); X += Spacing)
            {
                const FVector2D P(X, Y);
                if (X < Square.Min.X + Border || Y < Square.Min.Y + Border)
                    continue;
                if (Self != INDEX_NONE && !Inside(Regions[Self], P))
                    continue;
                if (OwnedByFiner(Regions, Self, Spacing, P) || NearOutline(P, Regions, Spacing * 0.25))
                    continue;
                Builder.Add(P);
            }
    }
} // namespace

FBox2D OpenGolfTerrainTiles::Bounds(FIntPoint Tile)
{
    const FVector2D Min(Tile.X * TileM, Tile.Y * TileM);
    return FBox2D(Min, Min + FVector2D(TileM, TileM));
}

bool OpenGolfTerrainTiles::Triangulate(const UOpenGolfTerrainDefinition& D, FIntPoint Tile, FTile& Out, FString& Error)
{
    Out = FTile();
    const FBox2D Square = Bounds(Tile);
    const FVector2D GridEnd = D.ElevationOriginM + FVector2D(D.ElevationSize.X - 1, D.ElevationSize.Y - 1) * D.ElevationSpacingM;
    if (!D.DefaultSurface || D.ElevationM.IsEmpty() || !FBox2D(D.ElevationOriginM, GridEnd).IsInsideOrOn(Square.Min) ||
        !FBox2D(D.ElevationOriginM, GridEnd).IsInsideOrOn(Square.Max) || D.DetailSpacingM < EdgeSpacingM)
    {
        Error = FString::Printf(TEXT("Square %d,%d needs a default surface, elevation covering it and rough spacing of at least %.1f m."),
            Tile.X, Tile.Y, EdgeSpacingM);
        return false;
    }
    const TArray<FClipped> Regions = ClipRegions(D, Square);
    FBuilder Builder{Out.Points};
    Builder.AddBoundary(Square, Regions);
    for (const auto& Region : Regions)
        for (const auto& [P, Q] : Region.Pieces)
            Builder.AddPiece(P, Q, Region.SpacingM);
    AddGrid(Builder, Square, Square, D.DetailSpacingM, Regions, INDEX_NONE);
    for (int32 R = 0; R < Regions.Num(); ++R)
        if (Regions[R].SpacingM < D.DetailSpacingM)
            AddGrid(Builder, Square, Regions[R].Bounds, Regions[R].SpacingM, Regions, R);

    UE::Geometry::FDelaunay2 Delaunay;
    Delaunay.bAutomaticallyFixEdgesToDuplicateVertices = true;
    if (!Delaunay.Triangulate(Out.Points, Builder.Edges))
    {
        Error = FString::Printf(TEXT("Square %d,%d could not be triangulated. Check for crossing surface outlines."), Tile.X, Tile.Y);
        return false;
    }
    Out.Triangles = Delaunay.GetTriangles();
    Out.Surfaces.Reserve(Out.Triangles.Num());
    for (const FIndex3i& T : Out.Triangles)
    {
        const FVector2D Centre = (Out.Points[T.A] + Out.Points[T.B] + Out.Points[T.C]) / 3;
        UOpenGolfSurfacePreset* Surface = D.DefaultSurface;
        for (const auto& Region : Regions)
            if (Inside(Region, Centre))
                Surface = Region.Surface;
        Out.Surfaces.Add(Surface);
    }
    return !Out.Triangles.IsEmpty();
}
