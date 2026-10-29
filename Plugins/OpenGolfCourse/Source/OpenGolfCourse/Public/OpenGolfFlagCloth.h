// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Templates/Function.h"

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
        /**
         * Pressure across the cloth, per unit of normal airspeed. Below an isolated flat plate's 1.2:
         * every triangle meets free-stream air, unsheltered by the cloth upwind of it. 0.7 holds the
         * flag to the Beaufort bands (UOpenGolf.Course.FlagBeaufort); 0.65-0.75 all pass, and 1.2 lifted
         * the flag to "extended" a force early, at 6 mph.
         */
        float NormalForceCoefficient = 0.7f;
        /** Skin friction along the cloth. */
        float FrictionCoefficient = 0.015f;
    };

    /** Lays the flag out flat from the pole along HeadingYawDeg; PoleTop is the pole's axis at its top. */
    void Reset(const FSettings& InSettings, const FVector& PoleTop, float HeadingYawDeg);

    /** Lets the flag fall and swing still in calm air for Seconds, so it starts hanging. */
    void Settle(float Seconds, const FVector& PoleTop);

    /** World wind in m/s and air density in kg/m^3. Gusts are added on top. Non-finite values read as calm, standard air. */
    void SetWind(const FVector& WindMps, float AirDensityKgM3);

    /**
     * Advances by DeltaSeconds (capped at 1/20 s) with the pole top at PoleTop. A pole that jumped
     * since the last call carries the flag with it rather than dragging it across the gap.
     */
    void Advance(float DeltaSeconds, const FVector& PoleTop);
    /** As Advance, calling OnSubstep with the cloth and the substep's seconds after each fixed substep. */
    void Advance(float DeltaSeconds, const FVector& PoleTop, TFunctionRef<void(const FOpenGolfFlagCloth&, float)> OnSubstep);

    static constexpr int32 Index(int32 Column, int32 Row) { return Row * Columns + Column; }
    const TArray<FVector>& GetPositions() const { return Positions; }
    FVector GetPosition(int32 Column, int32 Row) const { return Positions[Index(Column, Row)]; }
    /** Triangle corner indices, three per triangle; laid out along +Y, the normals face +X. */
    const TArray<int32>& GetTriangles() const { return Triangles; }
    const FVector& GetWindMps() const { return WindMps; }
    /** Direction the hoist sleeve faces, radians from +X; it turns downwind. */
    float GetHeadingRad() const { return HeadingRad; }
    /** Largest relative stretch of any along or down edge; 0.02 is 2%. */
    float GetMaxStretch() const;
    /** Angle of the middle row below horizontal, from hoist to fly end: 0 is straight out (or above), 90 hangs. */
    float GetDroopDeg() const;

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
    FVector LastPoleTop = FVector::ZeroVector;
    TArray<FVector> Positions;
    TArray<FVector> Previous;
    TArray<FVector> Velocities;
    TArray<FVector> Forces;
    TArray<float> InverseMass;
    TArray<FConstraint> Constraints;
    TArray<int32> Triangles;
};

/**
 * Live readings of a flying flag, for comparing it with real wind: the flag's droop and how often its fly
 * end flaps across the wind, both averaged over about AverageSeconds. A flap is two crossings of the fly
 * end's mean line, each by more than a small dead band, so ripples near the line do not count.
 */
struct OPENGOLFCOURSE_API FOpenGolfFlagMotion
{
    float AverageSeconds = 3.0f;
    /** Mean of FOpenGolfFlagCloth::GetDroopDeg. */
    float DroopDeg = 90.0f;
    float FlapHz = 0.0f;

    void Reset();
    /**
     * Call after each substep of the cloth (Advance's OnSubstep), with its seconds. Once a frame would miss
     * crossings of a flag flapping faster than about a quarter of the frame rate.
     */
    void Sample(const FOpenGolfFlagCloth& Cloth, float DeltaSeconds);

private:
    float CrossingsPerSecond = 0.0f;
    float LateralMeanCm = 0.0f;
    int32 Side = 0;
    bool bStarted = false;
};
