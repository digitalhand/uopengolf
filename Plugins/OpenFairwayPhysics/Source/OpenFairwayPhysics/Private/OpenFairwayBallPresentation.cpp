// SPDX-License-Identifier: MIT
// How the ball actor shows the simulation: the actor follows the 120 Hz ball between steps, turns the mesh as it rolls
// and draws the shot's tracer. Nothing here moves the simulated ball.
#include "OpenFairwayBallInternal.h"

#include "Components/LineBatchComponent.h"
#include "Engine/World.h"

using namespace OpenFairway;
using namespace OpenFairwayBallDetail;

FQuat AOpenFairwayBall::ComputeRollRotation(const FVector& DeltaCm, const FVector& Up, float RadiusCm, float MaxAngleRad)
{
    const FVector Normal = Up.GetSafeNormal();
    if (RadiusCm <= UE_KINDA_SMALL_NUMBER || Normal.IsZero())
    {
        return FQuat::Identity;
    }
    const FVector Along = FVector::VectorPlaneProject(DeltaCm, Normal);
    const double Distance = Along.Size();
    if (Distance <= UE_KINDA_SMALL_NUMBER)
    {
        return FQuat::Identity;
    }
    // Rolling without slipping: the contact point is still, so the top moves along the motion.
    const FVector Axis = FVector::CrossProduct(Normal, Along / Distance);
    return FQuat(Axis, FMath::Min(Distance / RadiusCm, static_cast<double>(FMath::Max(MaxAngleRad, 0.0f))));
}

void AOpenFairwayBall::ApplyVisualRoll(const FVector& FrameDelta)
{
    // Ground contact only: in flight the measured spin rate is far above what a frame rate can show.
    if (!bVisualRoll || !bOnGround || BallState == EOpenFairwayBallState::Flight)
    {
        return;
    }
    const FQuat Roll = ComputeRollRotation(FrameDelta, ToUnreal(FloorNormal), GolfPhysics::Radius * VisualScale * CmPerM, MaxVisualRollRadPerFrame);
    if (!Roll.Equals(FQuat::Identity))
    {
        AddActorWorldRotation(Roll);
    }
}

void AOpenFairwayBall::PresentAt(const FVector& Location)
{
    SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    PresentedLocation = GetActorLocation();
}

void AOpenFairwayBall::AdoptExternalMove()
{
    const FVector Location = GetActorLocation();
    if (Location != PresentedLocation)
    {
        // A teleport, not motion: nothing to show between, so the simulation simply carries on from there.
        SimLocation = PreviousSimLocation = PresentedLocation = Location;
    }
}

void AOpenFairwayBall::AddTracerPoint(bool bForce)
{
    const UWorld* World = GetWorld();
    if (!bDrawTracer || !World)
    {
        return;
    }
    const FVector Location = GetActorLocation();
    if (!bForce && FVector::DistSquared(Location, LastTracerPoint) < TracerSpacingCm * TracerSpacingCm)
    {
        return;
    }
    if (ULineBatchComponent* Lines = World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent))
    {
        Lines->DrawLine(LastTracerPoint, Location, TracerColor, SDPG_World, 1.5f, 0.0f, TracerBatchId);
    }
    LastTracerPoint = Location;
}

void AOpenFairwayBall::ClearTracer()
{
    const UWorld* World = GetWorld();
    if (World && TracerBatchId != 0)
    {
        if (ULineBatchComponent* Lines = World->GetLineBatcher(UWorld::ELineBatcherType::WorldPersistent))
        {
            Lines->ClearBatch(TracerBatchId);
        }
    }
}
