// SPDX-License-Identifier: MIT
// Shared by the files that implement AOpenFairwayBall: the frame conversion between Unreal and the OpenFairway core, and the
// query distances. Named (not anonymous) so the unity build can combine those files.
#pragma once

#include "OpenFairwayBall.h"

namespace OpenFairwayBallDetail
{
    using namespace OpenFairway;

    constexpr int32 MaxSubstepsPerFrame = 12;
    /** Contact sweeps use a sphere this much smaller so a ball resting on the ground does not start overlapping it. */
    constexpr float SafeMarginCm = 0.05f;
    constexpr float GroundProbeUpM = 0.05f;
    constexpr float GroundProbeDistanceM = 0.08f;
    constexpr float SnapProbeM = 50.0f;
    constexpr float OutOfBoundsM = 1000.0f;
    constexpr float FallThroughM = 100.0f;
    /** A ball centre this far below the ground under it has tunnelled through and is lifted back. */
    constexpr float BelowGroundRecoveryM = 0.5f;
    constexpr float RecoveryProbeUpM = 2.0f;
    constexpr float RecoveryProbeDownM = 8.0f;
    constexpr float GroundSnapOffsetM = 0.001f;
    constexpr float TracerSpacingCm = 25.0f;
    /** Visual roll per frame stays under half a turn: without motion blur, faster spin reads as turning backwards. */
    constexpr float MaxVisualRollRadPerFrame = 1.0f;
    /** A contact whose normal is this close to up (its OpenFairway Y) is ground; a steeper one is a wall. */
    constexpr float MinGroundNormalY = 0.7f;
    /** OpenFairway up (+Y). */
    inline const FVector3f CoreUp(0.0f, 1.0f, 0.0f);

    /** Unreal (X forward, Y right, Z up) to OpenFairway (X forward, Y up, Z right) direction. */
    inline FVector3f ToCore(const FVector& V)
    {
        return FVector3f(static_cast<float>(V.X), static_cast<float>(V.Z), static_cast<float>(V.Y));
    }

    inline FVector ToUnreal(const FVector3f& V)
    {
        return FVector(V.X, V.Z, V.Y);
    }

    inline ESurface ToCoreSurface(EOpenFairwaySurface Surface)
    {
        return static_cast<ESurface>(Surface);
    }

    inline bool IsGroundNormal(const FVector3f& CoreNormal)
    {
        return CoreNormal.Y > MinGroundNormalY;
    }

    /** The unit normal, or up for a degenerate one. */
    inline FVector3f SafeNormal(const FVector3f& Normal)
    {
        return Normal.SizeSquared() > 0.000001f ? Normal / Normal.Size() : CoreUp;
    }

    /** Velocity without its part along Normal: only the part into the surface, or both directions. */
    inline FVector3f RemoveVelocityAlongNormal(const FVector3f& Velocity, const FVector3f& Normal, bool bRemoveBothDirections)
    {
        const FVector3f FloorNormal = SafeNormal(Normal);
        const float NormalComponent = FVector3f::DotProduct(Velocity, FloorNormal);
        if (!bRemoveBothDirections && NormalComponent >= 0.0f)
        {
            return Velocity;
        }
        return Velocity - FloorNormal * NormalComponent;
    }

    /** A cup's own frame: its centre at the surface, Y along its normal, meters. CupCapture works in it. */
    struct FCupFrame
    {
        FVector Origin;
        FQuat4f Rotation;

        explicit FCupFrame(const FOpenFairwayCup& Cup)
            : Origin(Cup.Position), Rotation(FQuat4f::FindBetweenNormals(CoreUp, ToCore(Cup.GetNormal())))
        {
        }

        /** A world location (cm) in the cup frame (m). */
        FVector3f ToLocal(const FVector& World) const
        {
            return Rotation.UnrotateVector(ToCore(World - Origin) / CmPerM);
        }

        /** A world displacement (cm) in the cup frame (m). */
        FVector3f ToLocalOffset(const FVector& WorldOffset) const
        {
            return Rotation.UnrotateVector(ToCore(WorldOffset) / CmPerM);
        }

        /** A core-frame direction or velocity in the cup frame. */
        FVector3f ToLocalDirection(const FVector3f& Core) const
        {
            return Rotation.UnrotateVector(Core);
        }

        /** A cup-frame location (m) in the world (cm). */
        FVector ToWorld(const FVector3f& Local) const
        {
            return Origin + ToUnreal(Rotation.RotateVector(Local)) * CmPerM;
        }

        /** A cup-frame direction or velocity in the core frame. */
        FVector3f ToCoreDirection(const FVector3f& Local) const
        {
            return Rotation.RotateVector(Local);
        }
    };
} // namespace OpenFairwayBallDetail
