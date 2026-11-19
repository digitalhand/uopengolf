// SPDX-License-Identifier: MIT
// What the ball actor touches: surface resolution (tags and physical materials), the obstacle and canopy tags, and the
// Chaos line traces that find the ground under the ball.
#include "OpenFairwayBallInternal.h"

#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "OpenFairway/Obstacles.h"
#include "OpenFairwayLog.h"
#include "OpenFairwayPhysicalMaterial.h"

using namespace OpenFairway;
using namespace OpenFairwayBallDetail;

FName AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface Surface)
{
    return FName(*FString::Printf(TEXT("OpenFairway.Surface.%s"), SurfaceCatalog::GetName(ToCoreSurface(Surface))));
}

FName AOpenFairwayBall::GetObstacleTag()
{
    static const FName Tag(TEXT("OpenFairway.Obstacle"));
    return Tag;
}

bool AOpenFairwayBall::IsObstacle(const UPrimitiveComponent* Component)
{
    if (!Component)
    {
        return false;
    }
    const AActor* Owner = Component->GetOwner();
    return Component->ComponentHasTag(GetObstacleTag()) || (Owner && Owner->ActorHasTag(GetObstacleTag()));
}

FName AOpenFairwayBall::GetCanopyTag()
{
    static const FName Tag(TEXT("OpenFairway.Canopy"));
    return Tag;
}

bool AOpenFairwayBall::IsCanopy(const UPrimitiveComponent* Component)
{
    if (!Component)
    {
        return false;
    }
    const AActor* Owner = Component->GetOwner();
    return Component->ComponentHasTag(GetCanopyTag()) || (Owner && Owner->ActorHasTag(GetCanopyTag()));
}

float AOpenFairwayBall::CanopySpeedFactor(float PathM)
{
    return Obstacles::CanopySpeedFactor(PathM);
}

FVector3f AOpenFairwayBall::ResolveCanopyDrag(const FVector3f& Velocity, float PathM, uint32 Seed)
{
    return Obstacles::ResolveCanopyDrag(Velocity, PathM, Seed);
}

FVector3f AOpenFairwayBall::ResolveObstacleHit(const FVector3f& Velocity, const FVector3f& Normal, const FVector3f& AwayFromObstacle)
{
    return Obstacles::ResolveObstacleHit(Velocity, Normal, AwayFromObstacle);
}

EOpenFairwaySurface AOpenFairwayBall::ResolveSurface(const UPrimitiveComponent* Component, const UPhysicalMaterial* PhysicalMaterial) const
{
    auto FindTag = [](auto HasTag, EOpenFairwaySurface& OutSurface)
    {
        for (uint8 Index = 0; Index < static_cast<uint8>(ESurface::Count); ++Index)
        {
            if (HasTag(GetSurfaceTag(static_cast<EOpenFairwaySurface>(Index))))
            {
                OutSurface = static_cast<EOpenFairwaySurface>(Index);
                return true;
            }
        }
        return false;
    };

    EOpenFairwaySurface Surface = DefaultSurface;
    if (Component && FindTag([Component](FName Tag)
                         { return Component->ComponentHasTag(Tag); }, Surface))
    {
        return Surface;
    }
    if (const UOpenFairwayPhysicalMaterial* Declared = Cast<UOpenFairwayPhysicalMaterial>(PhysicalMaterial))
    {
        return Declared->Surface;
    }
    // Simple collision has one material for the whole body: the override, else its graphics material's.
    if (const FBodyInstance* Body = Component ? Component->GetBodyInstance() : nullptr)
    {
        if (const UOpenFairwayPhysicalMaterial* Declared = Cast<UOpenFairwayPhysicalMaterial>(Body->GetSimplePhysicalMaterial()))
        {
            return Declared->Surface;
        }
    }
    const AActor* ComponentOwner = Component ? Component->GetOwner() : nullptr;
    if (ComponentOwner && FindTag([ComponentOwner](FName Tag)
                              { return ComponentOwner->ActorHasTag(Tag); }, Surface))
    {
        return Surface;
    }
    return DefaultSurface;
}

void AOpenFairwayBall::SetSurface(EOpenFairwaySurface Surface)
{
    if (Surface == CurrentSurface && BallState != EOpenFairwayBallState::Rest)
    {
        return;
    }
    CurrentSurface = Surface;
    SurfaceSettings = ShotParams.ResolveSurface(ToCoreSurface(Surface), Profile);
    Result.Surface = Surface;
}

bool AOpenFairwayBall::TryRecoverToGround()
{
    // Probe from above: a ball inside or under the terrain still finds the ground's top face.
    FGroundHit Ground;
    const FVector Location = SimLocation;
    if (BallState == EOpenFairwayBallState::Rest || bHoled || !ProbeGround(Ground, RecoveryProbeUpM, RecoveryProbeDownM) || !IsGroundNormal(Ground.Normal) ||
        Ground.Location.Z - Location.Z < BelowGroundRecoveryM * CmPerM)
    {
        return false;
    }

    SimLocation = Ground.Location + ToUnreal(Ground.Normal) * (GolfPhysics::Radius + GroundSnapOffsetM) * CmPerM;
    FloorNormal = Ground.Normal;
    SetSurface(ResolveSurface(Ground.Component.Get(), Ground.PhysMaterial.Get()));
    Velocity = RemoveVelocityAlongNormal(Velocity, Ground.Normal, false);
    bOnGround = true;
    UE_LOG(LogOpenFairway, Warning, TEXT("Shot %d fell through the ground at %s; recovered onto %s."), Result.ShotNumber, *Location.ToString(),
        SurfaceCatalog::GetName(ToCoreSurface(CurrentSurface)));
    if (BallState == EOpenFairwayBallState::Flight)
    {
        RecordLanding(Ground.Normal);
        BallState = EOpenFairwayBallState::Rollout;
        UpdateResult();
        BroadcastLanded();
    }
    return true;
}

bool AOpenFairwayBall::ProbeGround(FGroundHit& OutHit, float UpMeters, float DownMeters) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector Location = SimLocation;
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OpenFairwayBallGroundProbe), false, this);
    Params.bReturnPhysicalMaterial = true;
    // Obstacles are never ground: look through them to the surface below.
    constexpr int32 MaxObstacles = 4;
    for (int32 Attempt = 0;; ++Attempt)
    {
        if (!World->LineTraceSingleByChannel(Hit, Location + FVector::UpVector * UpMeters * CmPerM, Location - FVector::UpVector * DownMeters * CmPerM, TraceChannel, Params))
        {
            return false;
        }
        if (!IsObstacle(Hit.GetComponent()))
        {
            break;
        }
        if (Attempt == MaxObstacles)
        {
            return false;
        }
        Params.AddIgnoredComponent(Hit.GetComponent());
    }

    OutHit.Location = Hit.ImpactPoint;
    OutHit.Normal = SafeNormal(ToCore(Hit.ImpactNormal));
    OutHit.Component = Hit.GetComponent();
    OutHit.PhysMaterial = Hit.PhysMaterial;
    return true;
}

void AOpenFairwayBall::SnapToGround()
{
    FGroundHit Ground;
    if (ProbeGround(Ground, SnapProbeM, SnapProbeM))
    {
        SimLocation = Ground.Location + FVector::UpVector * GolfPhysics::Radius * CmPerM;
    }
    PreviousSimLocation = SimLocation;
    PresentAt(SimLocation);
}
