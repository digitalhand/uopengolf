// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"

/**
 * Cloth for a flag on a round, upright pole: a small position-based (XPBD) solver in world space,
 * centimeters and seconds. The hoist column is pinned to the pole; gravity drops the rest, and the
 * wind acts on each triangle with flat-plate normal pressure plus skin friction. A flag in its own
 * plane feels only friction, so what holds it out in the wind is the drag of its own flapping, as on
 * a real flag. Fixed substeps make it frame-rate independent. No engine objects; the flagstick
 * component renders it.
 */
class OPENGOLFCOURSE_API FOpenGolfFlagCloth
{
public:
    /** Particles along the flag and down it: 3.2 by 4.4 cm cells on a 20 by 14 inch flag. */
    static constexpr int32 Columns = 17;
    static constexpr int32 Rows = 9;
    static constexpr float SubstepSeconds = 1.0f / 480.0f;

    struct FSettings
    {
        float LengthCm = 50.8f;
        float HeightCm = 35.56f;
        float PoleRadiusCm = 0.635f;
        /** Cloth weight per area; a nylon flag is about 0.11 kg/m^2. */
        float ClothKgPerM2 = 0.11f;
        /** Flat-plate pressure across the cloth, per unit of normal airspeed. */
        float NormalForceCoefficient = 1.2f;
        /** Skin friction along the cloth. */
        float FrictionCoefficient = 0.015f;
    };

    /** Lays the flag out flat from the pole along HeadingYawDeg; PoleTop is the pole's axis at its top. */
    void Reset(const FSettings& InSettings, const FVector& PoleTop, float HeadingYawDeg);

    /** Lets the flag fall and swing still in calm air for Seconds, so it starts hanging. */
    void Settle(float Seconds, const FVector& PoleTop);

    /** World wind in m/s and air density in kg/m^3. Gusts are added on top. */
    void SetWind(const FVector& WindMps, float AirDensityKgM3);

    /** Advances by DeltaSeconds (capped at 1/20 s) with the pole top at PoleTop. */
    void Advance(float DeltaSeconds, const FVector& PoleTop);

    static constexpr int32 Index(int32 Column, int32 Row) { return Row * Columns + Column; }
    const TArray<FVector>& GetPositions() const { return Positions; }
    FVector GetPosition(int32 Column, int32 Row) const { return Positions[Index(Column, Row)]; }
    /** Triangle corner indices, three per triangle; laid out along +Y, the normals face +X. */
    const TArray<int32>& GetTriangles() const { return Triangles; }
    const FVector& GetWindMps() const { return WindMps; }
    /** Largest relative stretch of any along or down edge; 0.02 is 2%. */
    float GetMaxStretch() const;

private:
    struct FConstraint
    {
        int32 A;
        int32 B;
        float RestCm;
        float Compliance;
    };

    FVector HoistPoint(int32 Row, const FVector& PoleTop) const;
    FVector WindAt(const FVector& Position) const;
    void Substep(float Seconds, const FVector& PoleTop);

    FSettings Settings;
    FVector WindMps = FVector::ZeroVector;
    float AirDensity = 1.225f;
    float HeadingRad = 0.0f;
    float ParticleKg = 0.0f;
    float ColumnCm = 0.0f;
    float RowCm = 0.0f;
    double TimeSeconds = 0.0;
    float Pending = 0.0f;
    TArray<FVector> Positions;
    TArray<FVector> Previous;
    TArray<FVector> Velocities;
    TArray<FVector> Forces;
    TArray<float> InverseMass;
    TArray<FConstraint> Constraints;
    TArray<int32> Triangles;
};
