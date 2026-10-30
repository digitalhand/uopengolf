// SPDX-License-Identifier: MIT
#include "OpenFairway/CupCapture.h"
#include "OpenFairway/GolfPhysics.h"

namespace OpenFairway::CupCapture
{
    namespace
    {
        constexpr float StoppedSpeed = 1e-4f;
        // Height gates: the contact or centre must be at green level, so a ball on the flagstick is never holed.
        constexpr float MaxContactHeight = GolfPhysics::Radius;
        constexpr float MaxCenterHeight = 2.0f * GolfPhysics::Radius;

        float HorizontalDistanceSquared(const FVector3f& A, const FVector3f& B)
        {
            return FMath::Square(A.X - B.X) + FMath::Square(A.Z - B.Z);
        }
    } // namespace

    float MaxCaptureSpeed(float OffsetM)
    {
        const float HalfChordSquared = CupRadiusM * CupRadiusM - OffsetM * OffsetM;
        if (HalfChordSquared <= 0.0f)
        {
            return 0.0f;
        }
        // sqrt(g / 2r): the inverse of the time to fall one ball radius, per unit of chord.
        const float FallRate = FMath::Sqrt(GolfPhysics::Gravity / (2.0f * GolfPhysics::Radius));
        return 2.0f * FMath::Sqrt(HalfChordSquared) * FallRate;
    }

    namespace
    {
        // A grounded ball over the cup at green level: its horizontal speed and the fastest that drops on its line.
        bool OverCup(const FVector3f& BallPosition, const FVector3f& Velocity, const FVector3f& Cup, float& OutSpeed, float& OutMaxSpeed)
        {
            if (FMath::Abs(BallPosition.Y - Cup.Y) > MaxCenterHeight)
            {
                return false;
            }
            const float DistanceSquared = HorizontalDistanceSquared(BallPosition, Cup);
            if (DistanceSquared > CupRadiusM * CupRadiusM)
            {
                return false;
            }
            const float DX = Cup.X - BallPosition.X;
            const float DZ = Cup.Z - BallPosition.Z;
            OutSpeed = FMath::Sqrt(Velocity.X * Velocity.X + Velocity.Z * Velocity.Z);
            const float Offset = OutSpeed < StoppedSpeed ? FMath::Sqrt(DistanceSquared) : FMath::Abs(DX * Velocity.Z - DZ * Velocity.X) / OutSpeed;
            OutMaxSpeed = MaxCaptureSpeed(Offset);
            return true;
        }
    } // namespace

    bool CapturesRolling(const FVector3f& BallPosition, const FVector3f& Velocity, const FVector3f& Cup)
    {
        float Speed = 0.0f, MaxSpeed = 0.0f;
        return OverCup(BallPosition, Velocity, Cup, Speed, MaxSpeed) && Speed <= MaxSpeed;
    }

    bool CrossesTooFast(const FVector3f& BallPosition, const FVector3f& Velocity, const FVector3f& Cup, float& OutSpeed, float& OutMaxSpeed)
    {
        return OverCup(BallPosition, Velocity, Cup, OutSpeed, OutMaxSpeed) && OutSpeed > OutMaxSpeed;
    }

    bool CapturesLanding(const FVector3f& ContactPosition, const FVector3f& Cup)
    {
        return FMath::Abs(ContactPosition.Y - Cup.Y) <= MaxContactHeight && HorizontalDistanceSquared(ContactPosition, Cup) <= CupRadiusM * CupRadiusM;
    }

    FVector3f DropPoint(const FVector3f& BallPosition, const FVector3f& Cup, float PoleRadiusM)
    {
        const FVector2f ToBall(BallPosition.X - Cup.X, BallPosition.Z - Cup.Z);
        const float Distance = ToBall.Size();
        const FVector2f Direction = Distance > StoppedSpeed ? ToBall / Distance : FVector2f(-1.0f, 0.0f);
        const float Radial = FMath::Min(PoleRadiusM + GolfPhysics::Radius, CupRadiusM);
        return FVector3f(Cup.X + Direction.X * Radial, Cup.Y - GolfPhysics::Radius - DropDepthM, Cup.Z + Direction.Y * Radial);
    }

    float TimeSinceRimEntry(const FVector3f& BallPosition, const FVector3f& Velocity, const FVector3f& Cup)
    {
        const FVector2f Offset(BallPosition.X - Cup.X, BallPosition.Z - Cup.Z);
        const FVector2f Horizontal(Velocity.X, Velocity.Z);
        const float SpeedSquared = Horizontal.SizeSquared();
        const float Inside = CupRadiusM * CupRadiusM - Offset.SizeSquared();
        if (Inside <= 0.0f || SpeedSquared < StoppedSpeed * StoppedSpeed)
        {
            return 0.0f;
        }
        // The positive root of |Offset - s * Horizontal| = CupRadiusM: back along the line to the rim.
        const float Along = FVector2f::DotProduct(Offset, Horizontal);
        return (Along + FMath::Sqrt(Along * Along + SpeedSquared * Inside)) / SpeedSquared;
    }

    namespace
    {
        float HorizontalDistance(const FVector3f& Position, const FVector3f& Cup)
        {
            return FMath::Sqrt(HorizontalDistanceSquared(Position, Cup));
        }

        // Distance to the solid green: the plane outside the aperture, its rim inside it.
        // Below surface level inside the hole is handled by the capture rule before this query.
        float ContactGap(const FVector3f& Position, const FVector3f& Cup, FVector3f& Normal)
        {
            const FVector2f Offset(Position.X - Cup.X, Position.Z - Cup.Z);
            const float Distance = Offset.Size();
            if (Distance >= CupRadiusM)
            {
                Normal = FVector3f(0, 1, 0);
                return Position.Y - Cup.Y - GolfPhysics::Radius;
            }
            const FVector2f Direction = Distance > StoppedSpeed ? Offset / Distance : FVector2f(1, 0);
            const FVector3f Rim(Cup.X + Direction.X * CupRadiusM, Cup.Y, Cup.Z + Direction.Y * CupRadiusM);
            const FVector3f FromRim = Position - Rim;
            Normal = FromRim.GetSafeNormal();
            return FromRim.Size() - GolfPhysics::Radius;
        }

        void StrikeRim(FBallState& State, const FVector3f& Normal)
        {
            const float Approach = FVector3f::DotProduct(State.Velocity, Normal);
            if (Approach >= 0.0f)
            {
                return;
            }

            const float NormalImpulse = -(1.0f + RimRestitution) * GolfPhysics::Mass * Approach;
            const FVector3f Lever = -GolfPhysics::Radius * Normal;
            const FVector3f ContactVelocity = State.Velocity + FVector3f::CrossProduct(State.Spin, Lever);
            const FVector3f Slip = ContactVelocity - FVector3f::DotProduct(ContactVelocity, Normal) * Normal;
            State.Velocity += NormalImpulse / GolfPhysics::Mass * Normal;
            const float SlipSpeed = Slip.Size();
            if (SlipSpeed > StoppedSpeed)
            {
                // Coulomb, and never more than stops the slip (2/7 of the mass for a solid sphere, as on the ground).
                const FVector3f Friction = -FMath::Min(RimFriction * NormalImpulse, 2.0f * GolfPhysics::Mass / 7.0f * SlipSpeed) * Slip / SlipSpeed;
                State.Velocity += Friction / GolfPhysics::Mass;
                State.Spin += FVector3f::CrossProduct(Lever, Friction) / GolfPhysics::Inertia;
            }
        }
    } // namespace

    FCrossingStep AdvanceCrossing(FBallState& State, const FVector3f& Cup, const FVector3f& Gravity, float Dt, float MaxSubstep)
    {
        FCrossingStep Result;
        if (!FMath::IsFinite(Dt) || Dt <= 0.0f)
            return Result;
        if (!FMath::IsFinite(MaxSubstep) || MaxSubstep <= 0.0f)
            MaxSubstep = CrossingSubstepS;
        // A ballistic step is exact between contacts. Resolve a bracketed contact to microsecond precision,
        // rather than pushing an already penetrating ball up and adding potential energy.
        auto Predict = [&Gravity](const FBallState& From, float Time)
        {
            FBallState Next = From;
            Next.Position += From.Velocity * Time + 0.5f * Gravity * Time * Time;
            Next.Velocity += Gravity * Time;
            return Next;
        };
        while (Result.ConsumedSeconds < Dt)
        {
            const float Step = FMath::Min(MaxSubstep, Dt - Result.ConsumedSeconds);
            FBallState Next = Predict(State, Step);
            FVector3f Normal;
            if (ContactGap(Next.Position, Cup, Normal) < 0 && Next.Position.Y > Cup.Y)
            {
                float Low = 0, High = Step;
                for (int32 Iteration = 0; Iteration < 16 && High - Low > 1e-7f; ++Iteration)
                {
                    const float Mid = (Low + High) * 0.5f;
                    if (ContactGap(Predict(State, Mid).Position, Cup, Normal) < 0)
                        High = Mid;
                    else
                        Low = Mid;
                }
                State = Predict(State, Low);
                ContactGap(State.Position, Cup, Normal);
                StrikeRim(State, Normal);
                // Only a sub-micrometre separation to avoid resolving the same floating point contact forever.
                State.Position += Normal * 1e-7f;
                Result.ConsumedSeconds += FMath::Max(Low, 1e-7f);
            }
            else
            {
                State = Next;
                Result.ConsumedSeconds += Step;
            }
            const float Distance = HorizontalDistance(State.Position, Cup);
            if (Distance < CupRadiusM && State.Position.Y <= Cup.Y)
            {
                Result.Outcome = ECupCrossing::Holed;
                break;
            }
            if (Distance > CupRadiusM && State.Position.Y >= Cup.Y + GolfPhysics::Radius - 1e-6f)
            {
                Result.Outcome = ECupCrossing::Out;
                break;
            }
        }
        Result.ConsumedSeconds = FMath::Min(Result.ConsumedSeconds, Dt);
        return Result;
    }

    ECupCrossing StepOverCup(FBallState& State, const FVector3f& Cup, float Dt)
    {
        return AdvanceCrossing(State, Cup, FVector3f(0, -GolfPhysics::Gravity, 0), Dt).Outcome;
    }
} // namespace OpenFairway::CupCapture
