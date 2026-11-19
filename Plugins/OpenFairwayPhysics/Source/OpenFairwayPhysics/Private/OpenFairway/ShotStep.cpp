// SPDX-License-Identifier: MIT
#include "OpenFairway/ShotStep.h"

namespace OpenFairway
{
    ShotStep::FImpactOutcome ShotStep::Impact(const FVector3f& Velocity, const FVector3f& Spin, const FVector3f& Normal,
        const FSurfaceSettings& Surface, const FResolvedShotParams& Params, bool bFirstImpact)
    {
        const FBallState Resolved =
            GolfPhysics::ResolveImpact(FBallState{FVector3f::ZeroVector, Velocity, Spin}, Normal, Surface, Params.Bounce, bFirstImpact, Params.Landing);
        return {Resolved.Velocity, Resolved.Spin, GolfPhysics::HasRebound(Resolved.Velocity, Normal, Surface.SurfaceBounce)};
    }

    FVector3f ShotStep::Settle(const FVector3f& Velocity, const FVector3f& Normal)
    {
        return Velocity - Normal * FVector3f::DotProduct(Velocity, Normal);
    }

    float ShotStep::ImpactAngleDeg(const FVector3f& Velocity, const FVector3f& Normal)
    {
        const float NormalSpeed = FMath::Abs(FVector3f::DotProduct(Velocity, Normal));
        const float TangentSpeed = Settle(Velocity, Normal).Size();
        return FMath::RadiansToDegrees(FMath::Atan2(NormalSpeed, FMath::Max(TangentSpeed, 0.0001f)));
    }

    float ShotStep::StruckWeight(const FVector3f& Velocity, const FVector3f& Normal)
    {
        return GolfPhysics::StruckAlongGroundWeight(Settle(Velocity, Normal).Size());
    }

    ShotStep::FContactFlight ShotStep::FlyToContact(const FBallState& State, const FResolvedShotParams& Params, float Dt, const FVector3f& Normal,
        float PlaneHeight, const FVector3f& WindUp)
    {
        FContactFlight Contact;
        Contact.TimeS = GolfPhysics::FlightContactTime(State, Params.Environment, Params.Flight, Dt, Normal, PlaneHeight);
        Contact.State = GolfPhysics::StepFlight(State, Params.Environment, Params.Flight, Contact.TimeS, WindUp);
        return Contact;
    }
} // namespace OpenFairway
