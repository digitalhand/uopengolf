// SPDX-License-Identifier: MIT
#include "OpenGolfFlagCloth.h"

namespace
{
    constexpr float GravityCmPerS2 = 980.665f;
    constexpr int32 Iterations = 4;
    /** Per second; with the air's own drag this settles a limp flag in a few seconds. */
    constexpr float Damping = 4.0f;
    /**
     * XPBD compliance, in cm/N-like units for these kilogram particle masses. Along and down the
     * flag the cloth is inextensible; fabric shears easily on the bias, which is what lets a flag
     * pinned along its hoist fall, and it bends freely.
     */
    constexpr float StretchCompliance = 0.0f;
    constexpr float ShearCompliance = 10.0f;
    constexpr float BendCompliance = 1.0f;
    /** Cloth stays this far outside the pole's surface. */
    constexpr float PoleClearanceCm = 0.4f;
    /** The hoist sleeve turns downwind with this time constant; in calm air it stays put. */
    constexpr float HeadingSeconds = 1.0f;
    constexpr float CalmWindMps = 0.1f;
    constexpr float MaxFrameSeconds = 1.0f / 20.0f;
    /** A pole moving farther than this between frames was placed, not swung, and takes its flag along. */
    constexpr double PoleJumpCm = 10.0;
    constexpr float StandardAirDensity = 1.225f;
    constexpr float CmPerM = 100.0f;
    constexpr float Cm2PerM2 = 10000.0f;
    /** The fly end must pass this far beyond its mean line for a crossing to count as a flap. */
    constexpr float FlapDeadBandCm = 0.5f;
} // namespace

void FOpenGolfFlagCloth::Reset(const FSettings& InSettings, const FVector& PoleTop, float HeadingYawDeg)
{
    Settings = InSettings;
    HeadingRad = FMath::DegreesToRadians(HeadingYawDeg);
    ColumnCm = Settings.LengthCm / (Columns - 1);
    RowCm = Settings.HeightCm / (Rows - 1);
    constexpr int32 Count = Columns * Rows;
    ParticleKg = Settings.ClothKgPerM2 * Settings.LengthCm * Settings.HeightCm / Cm2PerM2 / Count;
    TimeSeconds = 0.0;
    Pending = 0.0f;
    LastPoleTop = PoleTop;

    const FVector Along(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad), 0.0);
    Positions.SetNum(Count);
    InverseMass.SetNum(Count);
    for (int32 Row = 0; Row < Rows; ++Row)
    {
        for (int32 Column = 0; Column < Columns; ++Column)
        {
            Positions[Index(Column, Row)] = HoistPoint(Row, PoleTop) + Along * (Column * ColumnCm);
            InverseMass[Index(Column, Row)] = Column == 0 ? 0.0f : 1.0f / ParticleKg;
        }
    }
    Previous = Positions;
    Velocities.Init(FVector::ZeroVector, Count);
    Forces.Init(FVector::ZeroVector, Count);

    // Ordered outward from the pole so each Gauss-Seidel pass carries the pin across the flag.
    Constraints.Reset();
    const float Diagonal = FMath::Sqrt(ColumnCm * ColumnCm + RowCm * RowCm);
    for (int32 Column = 0; Column < Columns; ++Column)
    {
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            if (Column + 1 < Columns)
                Constraints.Add({Index(Column, Row), Index(Column + 1, Row), ColumnCm, StretchCompliance});
            if (Row + 1 < Rows)
                Constraints.Add({Index(Column, Row), Index(Column, Row + 1), RowCm, StretchCompliance});
            if (Column + 1 < Columns && Row + 1 < Rows)
            {
                Constraints.Add({Index(Column, Row), Index(Column + 1, Row + 1), Diagonal, ShearCompliance});
                Constraints.Add({Index(Column + 1, Row), Index(Column, Row + 1), Diagonal, ShearCompliance});
            }
            if (Column + 2 < Columns)
                Constraints.Add({Index(Column, Row), Index(Column + 2, Row), 2.0f * ColumnCm, BendCompliance});
            if (Row + 2 < Rows)
                Constraints.Add({Index(Column, Row), Index(Column, Row + 2), 2.0f * RowCm, BendCompliance});
        }
    }

    Triangles.Reset();
    for (int32 Row = 0; Row + 1 < Rows; ++Row)
    {
        for (int32 Column = 0; Column + 1 < Columns; ++Column)
        {
            const int32 A = Index(Column, Row), B = Index(Column + 1, Row), C = Index(Column + 1, Row + 1), D = Index(Column, Row + 1);
            Triangles.Append({A, D, B, B, D, C});
        }
    }
}

void FOpenGolfFlagCloth::Settle(float Seconds, const FVector& PoleTop)
{
    const FVector Wind = WindMps;
    WindMps = FVector::ZeroVector;
    for (float Elapsed = 0.0f; Elapsed < Seconds; Elapsed += MaxFrameSeconds)
        Advance(MaxFrameSeconds, PoleTop);
    WindMps = Wind;
}

void FOpenGolfFlagCloth::SetWind(const FVector& InWindMps, float AirDensityKgM3)
{
    // One NaN would spread through the forces into every particle for good.
    WindMps = InWindMps.ContainsNaN() ? FVector::ZeroVector : InWindMps;
    AirDensity = FMath::IsFinite(AirDensityKgM3) ? FMath::Max(0.0f, AirDensityKgM3) : StandardAirDensity;
}

void FOpenGolfFlagCloth::Advance(float DeltaSeconds, const FVector& PoleTop)
{
    Advance(DeltaSeconds, PoleTop, [](const FOpenGolfFlagCloth&, float) {});
}

void FOpenGolfFlagCloth::Advance(float DeltaSeconds, const FVector& PoleTop, TFunctionRef<void(const FOpenGolfFlagCloth&, float)> OnSubstep)
{
    if (Positions.IsEmpty())
        return;
    const FVector Jump = PoleTop - LastPoleTop;
    if (Jump.SizeSquared() > PoleJumpCm * PoleJumpCm)
    {
        // Velocities come from position changes, so a jump left in place would fling the cloth.
        for (FVector& Position : Positions)
            Position += Jump;
        for (FVector& Position : Previous)
            Position += Jump;
    }
    LastPoleTop = PoleTop;
    Pending += FMath::Clamp(DeltaSeconds, 0.0f, MaxFrameSeconds);
    while (Pending >= SubstepSeconds)
    {
        Substep(SubstepSeconds, PoleTop);
        Pending -= SubstepSeconds;
        OnSubstep(*this, SubstepSeconds);
    }
}

float FOpenGolfFlagCloth::GetMaxStretch() const
{
    float Stretch = 0.0f;
    for (const FConstraint& Constraint : Constraints)
    {
        if (Constraint.Compliance == StretchCompliance)
            Stretch = FMath::Max(Stretch, static_cast<float>(FVector::Dist(Positions[Constraint.A], Positions[Constraint.B])) / Constraint.RestCm - 1.0f);
    }
    return Stretch;
}

float FOpenGolfFlagCloth::GetDroopDeg() const
{
    if (Positions.IsEmpty())
        return 90.0f;
    const FVector Span = GetPosition(Columns - 1, Rows / 2) - GetPosition(0, Rows / 2);
    // In a gale the flapping fly end rises above the hoist; that is still straight out.
    return FMath::Max(0.0f, FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(-Span.Z), static_cast<float>(Span.Size2D()))));
}

FVector FOpenGolfFlagCloth::HoistPoint(int32 Row, const FVector& PoleTop) const
{
    return PoleTop + FVector(FMath::Cos(HeadingRad), FMath::Sin(HeadingRad), 0.0) * Settings.PoleRadiusCm - FVector(0.0, 0.0, Row * RowCm);
}

FVector FOpenGolfFlagCloth::WindAt(const FVector& Position) const
{
    const float Speed = static_cast<float>(WindMps.Size2D());
    if (Speed < UE_KINDA_SMALL_NUMBER)
        return FVector::ZeroVector;
    // Surface wind is gusty: about 15% in speed and 8 degrees in direction, drifting down the flag
    // with the wind. A few incommensurate sines keep it smooth and deterministic.
    const float T = static_cast<float>(TimeSeconds);
    const float S = static_cast<float>(Position.X * WindMps.X + Position.Y * WindMps.Y) / Speed;
    const float Gust = 1.0f + 0.10f * FMath::Sin(1.3f * T + 0.07f * S) + 0.05f * FMath::Sin(3.7f * T + 1.7f + 0.03f * S);
    const float Veer = FMath::DegreesToRadians(8.0f) * (0.6f * FMath::Sin(0.9f * T + 0.05f * S) + 0.4f * FMath::Sin(2.9f * T + 0.4f));
    const float Lift = 0.05f * FMath::Sin(2.1f * T + 0.11f * S);
    const float Cos = FMath::Cos(Veer), Sin = FMath::Sin(Veer);
    return FVector((WindMps.X * Cos - WindMps.Y * Sin) * Gust, (WindMps.X * Sin + WindMps.Y * Cos) * Gust, Lift * Speed);
}

void FOpenGolfFlagCloth::Substep(float Seconds, const FVector& PoleTop)
{
    if (WindMps.Size2D() > CalmWindMps)
    {
        const float Downwind = FMath::Atan2(static_cast<float>(WindMps.Y), static_cast<float>(WindMps.X));
        HeadingRad += FMath::UnwindRadians(Downwind - HeadingRad) * (1.0f - FMath::Exp(-Seconds / HeadingSeconds));
    }

    // Air on each triangle: flat-plate pressure along the normal plus skin friction along the cloth,
    // from the air's speed relative to the cloth, shared by the three corners.
    for (FVector& Force : Forces)
        Force = FVector::ZeroVector;
    for (int32 Corner = 0; Corner < Triangles.Num(); Corner += 3)
    {
        const int32 A = Triangles[Corner], B = Triangles[Corner + 1], C = Triangles[Corner + 2];
        const FVector Cross = FVector::CrossProduct(Positions[B] - Positions[A], Positions[C] - Positions[A]);
        const double TwiceArea = Cross.Size();
        if (TwiceArea < UE_SMALL_NUMBER)
            continue;
        const FVector Normal = Cross / TwiceArea;
        const double AreaM2 = 0.5 * TwiceArea / Cm2PerM2;
        const FVector Center = (Positions[A] + Positions[B] + Positions[C]) / 3.0;
        const FVector ClothMps = (Velocities[A] + Velocities[B] + Velocities[C]) / (3.0 * CmPerM);
        const FVector Air = WindAt(Center) - ClothMps;
        const double Speed = Air.Size();
        if (Speed < UE_SMALL_NUMBER)
            continue;
        const double NormalSpeed = FVector::DotProduct(Air, Normal);
        const FVector Force = (Normal * (Settings.NormalForceCoefficient * Speed * NormalSpeed) + (Air - Normal * NormalSpeed) * (Settings.FrictionCoefficient * Speed)) * (0.5 * AirDensity * AreaM2 / 3.0);
        Forces[A] += Force;
        Forces[B] += Force;
        Forces[C] += Force;
    }

    const float Retain = FMath::Max(0.0f, 1.0f - Damping * Seconds);
    for (int32 Particle = 0; Particle < Positions.Num(); ++Particle)
    {
        Previous[Particle] = Positions[Particle];
        if (InverseMass[Particle] == 0.0f)
            continue;
        const FVector Acceleration = FVector(0.0, 0.0, -GravityCmPerS2) + Forces[Particle] * (CmPerM / ParticleKg);
        Velocities[Particle] = (Velocities[Particle] + Acceleration * Seconds) * Retain;
        Positions[Particle] += Velocities[Particle] * Seconds;
    }
    for (int32 Row = 0; Row < Rows; ++Row)
        Positions[Index(0, Row)] = HoistPoint(Row, PoleTop);

    const float InverseSecondsSquared = 1.0f / (Seconds * Seconds);
    for (int32 Iteration = 0; Iteration < Iterations; ++Iteration)
    {
        for (const FConstraint& Constraint : Constraints)
        {
            const FVector Delta = Positions[Constraint.B] - Positions[Constraint.A];
            const double Length = Delta.Size();
            const double Weight = InverseMass[Constraint.A] + InverseMass[Constraint.B] + Constraint.Compliance * InverseSecondsSquared;
            if (Length < UE_SMALL_NUMBER || Weight <= 0.0)
                continue;
            const FVector Correction = Delta * ((Length - Constraint.RestCm) / (Weight * Length));
            Positions[Constraint.A] += Correction * InverseMass[Constraint.A];
            Positions[Constraint.B] -= Correction * InverseMass[Constraint.B];
        }
        // Long-range attachments: no point gets farther from its row's hoist than along the cloth,
        // which keeps a long hanging chain from stretching under its own weight.
        for (int32 Row = 0; Row < Rows; ++Row)
        {
            const FVector Hoist = Positions[Index(0, Row)];
            for (int32 Column = 1; Column < Columns; ++Column)
            {
                FVector& Position = Positions[Index(Column, Row)];
                const FVector Offset = Position - Hoist;
                const double Reach = Column * ColumnCm;
                if (Offset.SizeSquared() > Reach * Reach)
                    Position = Hoist + Offset * (Reach / Offset.Size());
            }
        }
    }

    const double Clearance = Settings.PoleRadiusCm + PoleClearanceCm;
    for (int32 Particle = 0; Particle < Positions.Num(); ++Particle)
    {
        if (InverseMass[Particle] == 0.0f)
            continue;
        FVector& Position = Positions[Particle];
        const FVector2D FromAxis(Position.X - PoleTop.X, Position.Y - PoleTop.Y);
        const double Radius = FromAxis.Size();
        if (Radius < Clearance)
        {
            const FVector2D Out = Radius > UE_SMALL_NUMBER ? FromAxis * (Clearance / Radius) : FVector2D(Clearance, 0.0);
            Position.X = PoleTop.X + Out.X;
            Position.Y = PoleTop.Y + Out.Y;
        }
        Velocities[Particle] = (Position - Previous[Particle]) / Seconds;
    }
    TimeSeconds += Seconds;
}

void FOpenGolfFlagMotion::Reset()
{
    const float Seconds = AverageSeconds;
    *this = FOpenGolfFlagMotion();
    AverageSeconds = Seconds;
}

void FOpenGolfFlagMotion::Sample(const FOpenGolfFlagCloth& Cloth, float DeltaSeconds)
{
    if (Cloth.GetPositions().IsEmpty() || !(DeltaSeconds > 0.0f))
        return;
    using FCloth = FOpenGolfFlagCloth;
    const FVector Span = Cloth.GetPosition(FCloth::Columns - 1, FCloth::Rows / 2) - Cloth.GetPosition(0, FCloth::Rows / 2);
    const float Heading = Cloth.GetHeadingRad();
    const float Lateral = static_cast<float>(Span.Y * FMath::Cos(Heading) - Span.X * FMath::Sin(Heading));
    const float Droop = Cloth.GetDroopDeg();
    if (!bStarted)
    {
        DroopDeg = Droop;
        LateralMeanCm = Lateral;
        bStarted = true;
    }
    const float Blend = 1.0f - FMath::Exp(-DeltaSeconds / FMath::Max(AverageSeconds, 0.1f));
    DroopDeg += (Droop - DroopDeg) * Blend;
    LateralMeanCm += (Lateral - LateralMeanCm) * Blend;
    const float Offset = Lateral - LateralMeanCm;
    const int32 NewSide = Offset > FlapDeadBandCm ? 1 : Offset < -FlapDeadBandCm ? -1 : Side;
    const float Crossings = Side != 0 && NewSide != Side ? 1.0f : 0.0f;
    Side = NewSide;
    // An exponentially weighted count of crossings per second; two crossings make one flap.
    CrossingsPerSecond += (Crossings / DeltaSeconds - CrossingsPerSecond) * Blend;
    FlapHz = 0.5f * CrossingsPerSecond;
}
