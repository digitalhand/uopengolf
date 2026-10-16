// SPDX-License-Identifier: MIT
// Collision handling follows ogolf-physics game/GolfBall.cs at e6d793ae9477efaf55dc9078d7260f76bb532a7d,
// with Godot/Jolt MoveAndCollide and ray casts replaced by Chaos sphere sweeps and line traces.
#include "OpenFairwayBall.h"

#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "OpenFairwayLog.h"
#include "UObject/ConstructorHelpers.h"

using namespace OpenFairway;

namespace
{
    constexpr float CmPerM = 100.0f;
    constexpr int32 MaxSubstepsPerFrame = 12;
    /** Contact sweeps use a sphere this much smaller so a ball resting on the ground does not start overlapping it. */
    constexpr float SafeMarginCm = 0.05f;
    constexpr float GroundProbeUpM = 0.05f;
    constexpr float GroundProbeDistanceM = 0.08f;
    constexpr float SnapProbeM = 50.0f;
    constexpr float OutOfBoundsM = 1000.0f;
    constexpr float FallThroughM = 100.0f;
    constexpr float TracerSpacingCm = 25.0f;
    /** OpenFairway up (+Y). */
    const FVector3f CoreUp(0.0f, 1.0f, 0.0f);

    /** Unreal (X forward, Y right, Z up) to OpenFairway (X forward, Y up, Z right) direction. */
    FVector3f ToCore(const FVector& V)
    {
        return FVector3f(static_cast<float>(V.X), static_cast<float>(V.Z), static_cast<float>(V.Y));
    }
    FVector ToUnreal(const FVector3f& V)
    {
        return FVector(V.X, V.Z, V.Y);
    }

    ESurface ToCoreSurface(EOpenFairwaySurface Surface)
    {
        return static_cast<ESurface>(Surface);
    }

    FVector3f RemoveVelocityAlongNormal(const FVector3f& Velocity, const FVector3f& Normal, bool bRemoveBothDirections)
    {
        const FVector3f FloorNormal = Normal.SizeSquared() > 0.000001f ? Normal / Normal.Size() : CoreUp;
        const float NormalComponent = FVector3f::DotProduct(Velocity, FloorNormal);
        if (!bRemoveBothDirections && NormalComponent >= 0.0f)
        {
            return Velocity;
        }
        return Velocity - FloorNormal * NormalComponent;
    }

    float AngleDegrees(const FVector3f& Velocity, const FVector3f& Normal)
    {
        const float NormalSpeed = FMath::Abs(FVector3f::DotProduct(Velocity, Normal));
        const float TangentSpeed = (Velocity - Normal * FVector3f::DotProduct(Velocity, Normal)).Size();
        return FMath::RadiansToDegrees(FMath::Atan2(NormalSpeed, FMath::Max(TangentSpeed, 0.0001f)));
    }
} // namespace

AOpenFairwayBall::AOpenFairwayBall()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;

    BallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BallMesh"));
    RootComponent = BallMesh;
    BallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BallMesh->SetGenerateOverlapEvents(false);
    BallMesh->SetMobility(EComponentMobility::Movable);
    BallMesh->CastShadow = true;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Sphere.Succeeded())
    {
        BallMesh->SetStaticMesh(Sphere.Object);
    }
}

void AOpenFairwayBall::BeginPlay()
{
    Super::BeginPlay();
    // The engine sphere is 100 cm across.
    BallMesh->SetWorldScale3D(FVector(GolfPhysics::Radius * 2.0f * VisualScale));
    TracerBatchId = GetUniqueID();
    SetSurface(DefaultSurface);
    SnapToGround();
}

void AOpenFairwayBall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearTracer();
    Super::EndPlay(EndPlayReason);
}

FName AOpenFairwayBall::GetSurfaceTag(EOpenFairwaySurface Surface)
{
    return FName(*FString::Printf(TEXT("OpenFairway.Surface.%s"), SurfaceCatalog::GetName(ToCoreSurface(Surface))));
}

EOpenFairwaySurface AOpenFairwayBall::ResolveSurface(const UPrimitiveComponent* Component) const
{
    if (Component)
    {
        const AActor* ComponentOwner = Component->GetOwner();
        for (uint8 Index = 0; Index < static_cast<uint8>(ESurface::Count); ++Index)
        {
            const EOpenFairwaySurface Surface = static_cast<EOpenFairwaySurface>(Index);
            const FName Tag = GetSurfaceTag(Surface);
            if (Component->ComponentHasTag(Tag) || (ComponentOwner && ComponentOwner->ActorHasTag(Tag)))
            {
                return Surface;
            }
        }
    }
    return DefaultSurface;
}

FLaunchData AOpenFairwayBall::ToLaunchData(const FOpenFairwayLaunch& LaunchData)
{
    FLaunchData Data;
    Data.SpeedMph = LaunchData.BallSpeedMph;
    Data.VerticalLaunchDeg = LaunchData.VerticalLaunchDeg;
    Data.HorizontalLaunchDeg = LaunchData.HorizontalLaunchDeg;
    if (LaunchData.bHasBackSpin)
    {
        Data.BackSpinRpm = LaunchData.BackSpinRpm;
    }
    if (LaunchData.bHasSideSpin)
    {
        Data.SideSpinRpm = LaunchData.SideSpinRpm;
    }
    if (LaunchData.bHasTotalSpin)
    {
        Data.TotalSpinRpm = LaunchData.TotalSpinRpm;
    }
    if (LaunchData.bHasSpinAxis)
    {
        Data.SpinAxisDeg = LaunchData.SpinAxisDeg;
    }
    return Data;
}

FAirConditions AOpenFairwayBall::GetAirConditions() const
{
    FAirConditions Air;
    Air.TemperatureF = TemperatureF;
    Air.AltitudeFt = AltitudeFt;
    Air.RelativeHumidity = RelativeHumidity;
    Air.PressurePa = PressurePa;
    Air.WindSpeedMph = WindSpeedMph;
    Air.WindFromDeg = WindFromDeg;
    return Air;
}

FShotSimulationResult AOpenFairwayBall::PredictShot(const FOpenFairwayLaunch& LaunchData, EOpenFairwaySurface Surface) const
{
    // The headless shot is not turned by AimYawDeg, so give it the world-fixed wind relative to the aim line.
    FAirConditions Air = GetAirConditions();
    Air.WindFromDeg -= AimYawDeg;
    return ShotSimulator::Simulate(ToLaunchData(LaunchData), ToCoreSurface(Surface), Profile, Air);
}

bool AOpenFairwayBall::Launch(const FOpenFairwayLaunch& LaunchData)
{
    const FLaunchData Data = ToLaunchData(LaunchData);
    if (!Data.IsValid())
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Launch rejected: speed, angles and spin must be finite, with speed and spin not negative."));
        return false;
    }

    // Launch from the current lie so play can continue from where the ball stopped.
    SnapToGround();
    FGroundHit Ground;
    if (ProbeGround(Ground, GroundProbeUpM, GolfPhysics::Radius + GroundProbeDistanceM))
    {
        CurrentSurface = ResolveSurface(Ground.Component.Get());
    }

    const FShotSpin Spin = ShotSetup::ParseSpin(Data);
    ShotParams = FResolvedShotParams::Resolve(Profile, GetAirConditions(), Data.SpeedMph, Data.VerticalLaunchDeg, Spin.TotalSpinRpm);
    SurfaceSettings = ShotParams.ResolveSurface(ToCoreSurface(CurrentSurface), Profile);
    FLaunchVectors Vectors = ShotSetup::BuildLaunchVectors(Data.SpeedMph, Data.VerticalLaunchDeg, Data.HorizontalLaunchDeg, Spin.BackSpinRpm, Spin.SideSpinRpm);
    if (!FMath::IsNearlyZero(AimYawDeg))
    {
        // A clockwise yaw in Unreal turns toward +Z (right) in the OpenFairway frame.
        const float YawRadians = FMath::DegreesToRadians(-AimYawDeg);
        Vectors.Velocity = ShotSetup::Rotate(Vectors.Velocity, CoreUp, YawRadians);
        Vectors.Omega = ShotSetup::Rotate(Vectors.Omega, CoreUp, YawRadians);
        Vectors.ShotDirection = ShotSetup::Rotate(Vectors.ShotDirection, CoreUp, YawRadians);
    }

    Velocity = Vectors.Velocity;
    Omega = Vectors.Omega;
    ShotDirection = Vectors.ShotDirection;
    BallState = EOpenFairwayBallState::Flight;
    bOnGround = false;
    FloorNormal = CoreUp;
    SubstepAccumulator = 0.0f;
    RolloutImpactSpinRpm = 0.0f;
    ShotStart = GetActorLocation();

    const int32 ShotNumber = Result.ShotNumber + 1;
    Result = FOpenFairwayShotResult();
    Result.ShotNumber = ShotNumber;
    Result.Launch = LaunchData;
    Result.LandingBucket = LandingBucket::GetName(ShotParams.LandingBucket);
    Result.Surface = CurrentSurface;
    bHasShot = true;

    ClearTracer();
    LastTracerPoint = ShotStart;

    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d: %.1f mph, VLA %.2f, HLA %.2f, spin %.0f rpm (back %.0f, side %.0f, axis %.2f), regime %s%s, landing %s, lie %s."),
        ShotNumber, Data.SpeedMph, Data.VerticalLaunchDeg, Data.HorizontalLaunchDeg, Spin.TotalSpinRpm, Spin.BackSpinRpm, Spin.SideSpinRpm, Spin.SpinAxisDeg,
        *ShotParams.RegimeKey, ShotParams.MatchedRegimeKey.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (override %s)"), *ShotParams.MatchedRegimeKey),
        *Result.LandingBucket, SurfaceCatalog::GetName(ToCoreSurface(CurrentSurface)));
    return true;
}

void AOpenFairwayBall::PlaceAt(const FVector& Location)
{
    SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
    BallState = EOpenFairwayBallState::Rest;
    Velocity = FVector3f::ZeroVector;
    Omega = FVector3f::ZeroVector;
    bOnGround = false;
    SubstepAccumulator = 0.0f;
    SnapToGround();
    FGroundHit Ground;
    SetSurface(ProbeGround(Ground, GroundProbeUpM, GolfPhysics::Radius + GroundProbeDistanceM) ? ResolveSurface(Ground.Component.Get()) : DefaultSurface);
}

bool AOpenFairwayBall::GetShotResult(FOpenFairwayShotResult& OutResult) const
{
    OutResult = Result;
    return bHasShot;
}

void AOpenFairwayBall::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (BallState == EOpenFairwayBallState::Rest)
    {
        SubstepAccumulator = 0.0f;
        return;
    }

    SubstepAccumulator += DeltaSeconds;
    int32 Substeps = 0;
    while (SubstepAccumulator >= GolfPhysics::SimulationDt && Substeps < MaxSubstepsPerFrame)
    {
        SubstepAccumulator -= GolfPhysics::SimulationDt;
        ++Substeps;
        if (!StepPhysics(GolfPhysics::SimulationDt) || BallState == EOpenFairwayBallState::Rest)
        {
            SubstepAccumulator = 0.0f;
            return;
        }
    }

    // Prevent runaway catch-up after a stall while keeping the motion continuous.
    if (Substeps == MaxSubstepsPerFrame && SubstepAccumulator > GolfPhysics::SimulationDt)
    {
        SubstepAccumulator = GolfPhysics::SimulationDt;
    }
    UpdateResult();
    AddTracerPoint(false);
}

bool AOpenFairwayBall::StepPhysics(float Dt)
{
    const bool bWasOnGround = bOnGround;
    const FVector3f PreviousVelocity = Velocity;
    const FBallState State{FVector3f::ZeroVector, Velocity, Omega};
    const FBallState Next = bWasOnGround ? GolfPhysics::StepGround(State, FloorNormal, SurfaceSettings, ShotParams.Rollout, RolloutImpactSpinRpm, Dt)
                                         : GolfPhysics::StepFlight(State, ShotParams.Environment, ShotParams.Flight, Dt);
    Velocity = Next.Velocity;
    Omega = Next.Spin;
    Result.ElapsedS += Dt;

    const FVector Start = GetActorLocation();
    const FVector Offset = Start - ShotStart;
    if (FMath::Abs(Offset.X) > OutOfBoundsM * CmPerM || FMath::Abs(Offset.Y) > OutOfBoundsM * CmPerM || Offset.Z < -FallThroughM * CmPerM)
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Ball left the playable area at %s; stopping it."), *Start.ToString());
        EnterRest();
        return false;
    }

    auto MoveWithSweep = [this](const FVector& Delta, FHitResult& Hit)
    {
        if (Delta.IsNearlyZero(1e-4))
        {
            return false;
        }
        const FVector MoveStart = GetActorLocation();
        const FVector End = MoveStart + Delta;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(OpenFairwayBallSweep), false, this);
        const bool bHit = GetWorld()->SweepSingleByChannel(
            Hit, MoveStart, End, FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(GolfPhysics::Radius * CmPerM - SafeMarginCm), Params);
        if (bHit && Hit.bBlockingHit)
        {
            // Rest the true sphere tangent to the contact (the sweep sphere was a margin smaller).
            const FVector Contact = Hit.bStartPenetrating ? MoveStart + Hit.Normal * (Hit.PenetrationDepth + SafeMarginCm) : Hit.Location + Hit.ImpactNormal * SafeMarginCm;
            SetActorLocation(Contact, false, nullptr, ETeleportType::TeleportPhysics);
            return true;
        }
        SetActorLocation(End, false, nullptr, ETeleportType::TeleportPhysics);
        return false;
    };
    FHitResult Hit;
    const bool bHit = MoveWithSweep(ToUnreal(Velocity * Dt) * CmPerM, Hit);
    HandleCollision(bHit ? &Hit : nullptr, bWasOnGround, PreviousVelocity);

    // Match the headless contact step: move with the rebound for the unspent portion
    // of this timestep. Dropping it changes the next bounce speed enough to cross
    // the restitution cutoff, creating several yards of rollout disagreement.
    if (bHit && !bWasOnGround && !Hit.bStartPenetrating && ToCore(Hit.ImpactNormal).Y > 0.7f)
    {
        const bool bSupportedAfterImpact = bOnGround;
        const FVector3f ReboundVelocity = Velocity;
        FHitResult RemainderHit;
        const bool bRemainderHit = MoveWithSweep(ToUnreal(Velocity * Dt * (1.0f - Hit.Time)) * CmPerM, RemainderHit);
        HandleCollision(bRemainderHit ? &RemainderHit : nullptr, bSupportedAfterImpact, ReboundVelocity);
    }

    Result.ApexM = FMath::Max(Result.ApexM, static_cast<float>(GetActorLocation().Z - ShotStart.Z) / CmPerM);
    if (BallState != EOpenFairwayBallState::Rest && GolfPhysics::CanRest(FBallState{FVector3f::ZeroVector, Velocity, Omega}, bOnGround, FloorNormal, SurfaceSettings, ShotParams.Rollout))
    {
        EnterRest();
    }
    return true;
}

void AOpenFairwayBall::HandleCollision(const FHitResult* Hit, bool bWasOnGround, const FVector3f& PreviousVelocity)
{
    if (Hit)
    {
        FVector3f Normal = ToCore(Hit->bStartPenetrating ? Hit->Normal : Hit->ImpactNormal);
        Normal = Normal.SizeSquared() > 0.000001f ? Normal / Normal.Size() : CoreUp;
        if (Normal.Y > 0.7f)
        {
            FloorNormal = Normal;
            SetSurface(ResolveSurface(Hit->GetComponent()));
            const bool bLandedFromFlight = BallState == EOpenFairwayBallState::Flight;
            const bool bIsLanding = bLandedFromFlight || (!bWasOnGround && FVector3f::DotProduct(Velocity, Normal) < 0.0f) ||
                                    FVector3f::DotProduct(PreviousVelocity, Normal) < -0.5f;
            if (bIsLanding)
            {
                if (bLandedFromFlight)
                {
                    // The impact spin keeps its "bite" on roll-out friction as the spin decays.
                    RolloutImpactSpinRpm = Omega.Size() / GolfPhysics::RadPerRpm;
                    Result.bLanded = true;
                    Result.LandingSpeedMps = Velocity.Size();
                    Result.LandingAngleDeg = AngleDegrees(Velocity, Normal);
                    Result.HangTimeS = Result.ElapsedS;
                    Result.LandingSurface = CurrentSurface;
                    Result.CarryM = FMath::Max(FVector3f::DotProduct(ToCore(GetActorLocation() - ShotStart) / CmPerM, ShotDirection), 0.0f);
                }

                const FBallState Resolved = GolfPhysics::ResolveImpact(
                    FBallState{FVector3f::ZeroVector, Velocity, Omega}, Normal, SurfaceSettings, ShotParams.Bounce, bLandedFromFlight, ShotParams.Landing);
                if (FVector3f::DotProduct(Velocity, Normal) < 0.0f && bLandedFromFlight)
                {
                    BallState = EOpenFairwayBallState::Rollout;
                }
                Velocity = Resolved.Velocity;
                Omega = Resolved.Spin;

                if (!GolfPhysics::HasRebound(Velocity, Normal, SurfaceSettings.SurfaceBounce) && BallState == EOpenFairwayBallState::Rollout)
                {
                    bOnGround = true;
                    Velocity = RemoveVelocityAlongNormal(Velocity, Normal, true);
                }
                else
                {
                    bOnGround = false;
                }

                if (bLandedFromFlight && BallState == EOpenFairwayBallState::Rollout)
                {
                    UpdateResult();
                    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d landed on %s: carry %.1f yd, %.1f m/s at %.1f deg, hang time %.2f s."), Result.ShotNumber,
                        SurfaceCatalog::GetName(ToCoreSurface(CurrentSurface)), Result.CarryM * YardsPerMeter, Result.LandingSpeedMps, Result.LandingAngleDeg,
                        Result.HangTimeS);
                    OnBallLanded.Broadcast(Result);
                }
            }
            else
            {
                bOnGround = true;
                Velocity = RemoveVelocityAlongNormal(Velocity, Normal, false);
            }
        }
        else
        {
            // Wall: damped reflection.
            bOnGround = false;
            FloorNormal = CoreUp;
            Velocity = (Velocity - 2.0f * FVector3f::DotProduct(Velocity, Normal) * Normal) * 0.30f;
        }
        return;
    }

    // No contact: stay grounded only while ground is still directly beneath the ball.
    FGroundHit Ground;
    if (BallState != EOpenFairwayBallState::Flight && bWasOnGround && ProbeGround(Ground, GroundProbeUpM, GolfPhysics::Radius + GroundProbeDistanceM))
    {
        bOnGround = true;
        FloorNormal = Ground.Normal;
        SetSurface(ResolveSurface(Ground.Component.Get()));
    }
    else
    {
        bOnGround = false;
        FloorNormal = CoreUp;
    }
}

bool AOpenFairwayBall::ProbeGround(FGroundHit& OutHit, float UpMeters, float DownMeters) const
{
    const UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    const FVector Location = GetActorLocation();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OpenFairwayBallGroundProbe), false, this);
    if (!World->LineTraceSingleByChannel(Hit, Location + FVector::UpVector * UpMeters * CmPerM, Location - FVector::UpVector * DownMeters * CmPerM, TraceChannel, Params))
    {
        return false;
    }

    OutHit.Location = Hit.ImpactPoint;
    const FVector3f Normal = ToCore(Hit.ImpactNormal);
    OutHit.Normal = Normal.SizeSquared() > 0.000001f ? Normal / Normal.Size() : CoreUp;
    OutHit.Component = Hit.GetComponent();
    return true;
}

void AOpenFairwayBall::SnapToGround()
{
    FGroundHit Ground;
    if (ProbeGround(Ground, SnapProbeM, SnapProbeM))
    {
        SetActorLocation(Ground.Location + FVector::UpVector * GolfPhysics::Radius * CmPerM, false, nullptr, ETeleportType::TeleportPhysics);
    }
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

void AOpenFairwayBall::EnterRest()
{
    BallState = EOpenFairwayBallState::Rest;
    Velocity = FVector3f::ZeroVector;
    Omega = FVector3f::ZeroVector;
    SubstepAccumulator = 0.0f;
    if (!Result.bLanded)
    {
        Result.CarryM = FMath::Max(FVector3f::DotProduct(ToCore(GetActorLocation() - ShotStart) / CmPerM, ShotDirection), 0.0f);
    }
    Result.bAtRest = true;
    UpdateResult();
    AddTracerPoint(true);
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d at rest on %s: carry %.1f yd, roll %.1f yd, total %.1f yd, offline %.1f yd, apex %.1f ft, %.1f s."),
        Result.ShotNumber, SurfaceCatalog::GetName(ToCoreSurface(CurrentSurface)), Result.CarryM * YardsPerMeter, Result.RollM * YardsPerMeter,
        Result.TotalM * YardsPerMeter, Result.LateralM * YardsPerMeter, Result.ApexM * FeetPerMeter, Result.ElapsedS);
    OnBallAtRest.Broadcast(Result);
}

void AOpenFairwayBall::UpdateResult()
{
    const FVector3f Offset = ToCore(GetActorLocation() - ShotStart) / CmPerM;
    Result.TotalM = FMath::Max(FVector3f::DotProduct(Offset, ShotDirection), 0.0f);
    // Offline from the aim line, positive to the right.
    const FVector3f AimRight = ShotSetup::Rotate(FVector3f(0.0f, 0.0f, 1.0f), CoreUp, FMath::DegreesToRadians(-AimYawDeg));
    Result.LateralM = FVector3f::DotProduct(Offset, AimRight);
    Result.RollM = Result.bLanded ? Result.TotalM - Result.CarryM : 0.0f;
    Result.Surface = CurrentSurface;
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
