// SPDX-License-Identifier: MIT
#include "OpenFairway/Obstacles.h"

#include "Math/RandomStream.h"

namespace OpenFairway
{
    FVector3f Obstacles::ResolveObstacleHit(const FVector3f& Velocity, const FVector3f& Normal, const FVector3f& AwayFromObstacle)
    {
        auto Horizontal = [](const FVector3f& V) { return FVector3f(V.X, 0.0f, V.Z); };
        // Obstacles are upright, so only the sideways part of the contact deflects the ball.
        FVector3f Out = Horizontal(Normal);
        const bool bTop = Out.SizeSquared() < 0.04f;
        if (bTop)
        {
            Out = Horizontal(AwayFromObstacle);
            if (Out.SizeSquared() < 1e-8f)
            {
                Out = -Horizontal(Velocity);
            }
            if (Out.SizeSquared() < 1e-8f)
            {
                Out = FVector3f(-1.0f, 0.0f, 0.0f);
            }
        }
        Out.Normalize();
        const float Into = FVector3f::DotProduct(Velocity, Out);
        FVector3f Result = (Into < 0.0f ? Velocity - 2.0f * Into * Out : Velocity) * ObstacleRestitution;
        const float Outward = FVector3f::DotProduct(Result, Out);
        // Any contact facing upward (a flat top or its rim) could otherwise hold a falling ball in place.
        if (Normal.Y > 0.05f && Outward < ObstacleEscapeMps)
        {
            Result += (ObstacleEscapeMps - Outward) * Out;
        }
        return Result;
    }

    FVector3f Obstacles::ResolveWallHit(const FVector3f& Velocity, const FVector3f& Normal)
    {
        return (Velocity - 2.0f * FVector3f::DotProduct(Velocity, Normal) * Normal) * WallRestitution;
    }

    float Obstacles::CanopySpeedFactor(float PathM)
    {
        return FMath::IsFinite(PathM) && PathM > 0.0f ? FMath::Exp(-PathM / CanopyDecayLengthM) : 1.0f;
    }

    FVector3f Obstacles::ResolveCanopyDrag(const FVector3f& Velocity, float PathM, uint32 Seed)
    {
        if (!FMath::IsFinite(PathM) || PathM <= 0.0f || Velocity.IsNearlyZero())
        {
            return Velocity;
        }
        // Branches and leaves knock the ball a little off line in any direction around its path.
        FRandomStream Random(static_cast<int32>(Seed));
        const FVector3f Direction = Velocity.GetSafeNormal();
        FVector3f Side, Up;
        Direction.FindBestAxisVectors(Side, Up);
        const float Azimuth = Random.FRandRange(0.0f, UE_TWO_PI);
        const FVector3f Axis = (Side * FMath::Cos(Azimuth) + Up * FMath::Sin(Azimuth)).GetSafeNormal();
        const float Angle = FMath::DegreesToRadians(CanopyDeflectionDegPerSqrtM * FMath::Sqrt(PathM)) * Random.FRand();
        return FQuat4f(Axis, Angle).RotateVector(Direction) * (Velocity.Size() * CanopySpeedFactor(PathM));
    }
} // namespace OpenFairway
