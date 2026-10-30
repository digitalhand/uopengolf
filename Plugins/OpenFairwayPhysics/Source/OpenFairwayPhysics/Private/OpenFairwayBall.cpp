// SPDX-License-Identifier: MIT
// Collision handling follows ogolf-physics game/GolfBall.cs at e6d793ae9477efaf55dc9078d7260f76bb532a7d,
// with Godot/Jolt MoveAndCollide and ray casts replaced by Chaos sphere sweeps and line traces.
#include "OpenFairwayBall.h"
#include "OpenFairway/CupCapture.h"
#include "OpenFairway/WindProfile.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/LineBatchComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "OpenFairwayLog.h"
#include "OpenFairwayPhysicalMaterial.h"
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
    /** A ball centre this far below the ground under it has tunnelled through and is lifted back (ogolf-physics BELOW_GROUND_RECOVERY_Y). */
    constexpr float BelowGroundRecoveryM = 0.5f;
    constexpr float RecoveryProbeUpM = 2.0f;
    constexpr float RecoveryProbeDownM = 8.0f;
    constexpr float GroundSnapOffsetM = 0.001f;
    constexpr float TracerSpacingCm = 25.0f;
    /** Visual roll per frame stays under half a turn: without motion blur, faster spin reads as turning backwards. */
    constexpr float MaxVisualRollRadPerFrame = 1.0f;
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

    FQuat4f CupRotation(const FOpenFairwayCup& Cup)
    {
        return FQuat4f::FindBetweenNormals(CoreUp, ToCore(Cup.GetNormal()));
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

void AOpenFairwayBall::SetCupLocations(const TArray<FVector>& Locations)
{
    TArray<FOpenFairwayCup> Descriptors;
    for (const FVector& Location : Locations)
        Descriptors.Add({Location, FVector::UpVector});
    SetCups(Descriptors);
}

void AOpenFairwayBall::SetCups(const TArray<FOpenFairwayCup>& InCups)
{
    Cups.Reset();
    CupLocations.Reset();
    for (const FOpenFairwayCup& Cup : InCups)
    {
        if (Cup.Position.ContainsNaN())
            continue;
        Cups.Add({Cup.Position, Cup.GetNormal()});
        CupLocations.Add(Cup.Position);
    }
    CrossingCup.Reset();
    MeasuredCup.Reset();
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
    return FMath::IsFinite(PathM) && PathM > 0.0f ? FMath::Exp(-PathM / CanopyDecayLengthM) : 1.0f;
}

FVector3f AOpenFairwayBall::ResolveCanopyDrag(const FVector3f& Velocity, float PathM, uint32 Seed)
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

FVector3f AOpenFairwayBall::ResolveObstacleHit(const FVector3f& Velocity, const FVector3f& Normal, const FVector3f& AwayFromObstacle)
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

FVector AOpenFairwayBall::GetWindVelocityMps() const
{
    return ToUnreal(IsMoving() ? ShotParams.Environment.Wind : GetAirConditions().WindVelocity());
}

FVector AOpenFairwayBall::GetWindVelocityAtHeightMps(float HeightM) const
{
    return ToUnreal(IsMoving() ? WindProfile::At(ShotParams.Environment, HeightM) : GetAirConditions().WindVelocityAt(HeightM));
}

float AOpenFairwayBall::GetAirDensityKgM3() const
{
    return IsMoving() ? ShotParams.Environment.AirDensity : GetAirConditions().Density();
}

float AOpenFairwayBall::GetWindReferenceHeightM() const
{
    return IsMoving() ? ShotParams.Environment.WindReferenceHeightM : GetAirConditions().WindReferenceHeightM;
}

float AOpenFairwayBall::GetWindRoughnessM() const
{
    return IsMoving() ? ShotParams.Environment.WindRoughnessM : GetAirConditions().WindRoughnessM;
}

FShotSimulationResult AOpenFairwayBall::PredictShot(const FOpenFairwayLaunch& LaunchData, EOpenFairwaySurface Surface) const
{
    // The headless shot is not turned by AimYawDeg, so give it the world-fixed wind relative to the aim line.
    FAirConditions Air = GetAirConditions();
    Air.WindFromDeg -= AimYawDeg;
    FBallPhysicsProfile Predicted = Profile;
    Predicted.GreenStimpFeet = GreenSpeed::Clamp(GreenStimpFeet);
    return ShotSimulator::Simulate(ToLaunchData(LaunchData), ToCoreSurface(Surface), Predicted, Air);
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
        CurrentSurface = ResolveSurface(Ground.Component.Get(), Ground.PhysMaterial.Get());
    }

    const FShotSpin Spin = ShotSetup::ParseSpin(Data);
    // Every surface this shot touches resolves its green from this stimp.
    Profile.GreenStimpFeet = GreenSpeed::Clamp(GreenStimpFeet);
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
    ShotStep = 0;
    RolloutImpactSpinRpm = 0.0f;
    bHoled = false;
    CrossingCup.Reset();
    MeasuredCup.Reset();
    HoleDropElapsed = -1.0f;
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

    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d: %.1f mph, VLA %.2f, HLA %.2f, spin %.0f rpm (back %.0f, side %.0f, axis %.2f), regime %s%s, landing %s, lie %s, green stimp %.1f ft."),
        ShotNumber, Data.SpeedMph, Data.VerticalLaunchDeg, Data.HorizontalLaunchDeg, Spin.TotalSpinRpm, Spin.BackSpinRpm, Spin.SideSpinRpm, Spin.SpinAxisDeg,
        *ShotParams.RegimeKey, ShotParams.MatchedRegimeKey.IsEmpty() ? TEXT("") : *FString::Printf(TEXT(" (override %s)"), *ShotParams.MatchedRegimeKey),
        *Result.LandingBucket, SurfaceCatalog::GetName(ToCoreSurface(CurrentSurface)), Profile.GreenStimpFeet);
    return true;
}

void AOpenFairwayBall::PlaceAt(const FVector& Location)
{
    if (IsMoving() && bHasShot)
    {
        // Finish the interrupted shot where it was, so its result never stays "in flight".
        Result.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
        Result.bAtRest = true;
        UpdateResult();
    }
    SetActorLocation(Location, false, nullptr, ETeleportType::ResetPhysics);
    BallState = EOpenFairwayBallState::Rest;
    bHoled = false;
    CrossingCup.Reset();
    MeasuredCup.Reset();
    MeasuredCup.Reset();
    Result.bOverCup = false;
    HoleDropElapsed = -1.0f;
    Velocity = FVector3f::ZeroVector;
    Omega = FVector3f::ZeroVector;
    bOnGround = false;
    SubstepAccumulator = 0.0f;
    SnapToGround();
    FGroundHit Ground;
    SetSurface(ProbeGround(Ground, GroundProbeUpM, GolfPhysics::Radius + GroundProbeDistanceM) ? ResolveSurface(Ground.Component.Get(), Ground.PhysMaterial.Get()) : DefaultSurface);
    // Each re-tee faces the aim line, so a marked ball's line points at the target.
    SetActorRotation(FRotator(0.0f, AimYawDeg, 0.0f));
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

    if (HoleDropElapsed >= 0.0f)
    {
        StepHoleDrop(DeltaSeconds);
        return;
    }

    const FVector FrameStart = GetActorLocation();
    SubstepAccumulator += DeltaSeconds;
    int32 Substeps = 0;
    while (SubstepAccumulator >= GolfPhysics::SimulationDt && Substeps < MaxSubstepsPerFrame)
    {
        SubstepAccumulator -= GolfPhysics::SimulationDt;
        ++Substeps;
        if (!StepPhysics(GolfPhysics::SimulationDt) || BallState == EOpenFairwayBallState::Rest)
        {
            SubstepAccumulator = 0.0f;
            ApplyVisualRoll(GetActorLocation() - FrameStart);
            return;
        }
    }

    // Prevent runaway catch-up after a stall while keeping the motion continuous.
    if (Substeps == MaxSubstepsPerFrame && SubstepAccumulator > GolfPhysics::SimulationDt)
    {
        SubstepAccumulator = GolfPhysics::SimulationDt;
    }
    ApplyVisualRoll(GetActorLocation() - FrameStart);
    UpdateResult();
    AddTracerPoint(false);
}

bool AOpenFairwayBall::StepPhysics(float Dt)
{
    if (CrossingCup)
    {
        return StepCupCrossing(Dt);
    }
    const bool bWasOnGround = bOnGround;
    const FVector3f PreviousVelocity = Velocity;
    const FVector3f PreviousSpin = Omega;
    // Flight reads the height above the launch ground: ShotStart is the ball centre, one radius up.
    const FVector3f Position = ToCore(GetActorLocation() - ShotStart) / CmPerM + FVector3f(0.0f, GolfPhysics::Radius, 0.0f);
    const FBallState State{Position, Velocity, Omega};
    const FBallState Next = bWasOnGround ? GolfPhysics::StepGround(State, FloorNormal, SurfaceSettings, ShotParams.Rollout, RolloutImpactSpinRpm, Dt)
                                         : GolfPhysics::StepFlight(State, ShotParams.Environment, ShotParams.Flight, Dt);
    Velocity = Next.Velocity;
    Omega = Next.Spin;
    Result.ElapsedS += Dt;
    if (Result.ElapsedS > MaxShotSeconds)
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Shot %d still moving after %.1f s; stopping it."), Result.ShotNumber, MaxShotSeconds);
        Result.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
        EnterRest();
        return false;
    }

    const FVector Start = GetActorLocation();
    const FVector Offset = Start - ShotStart;
    if (FMath::Abs(Offset.X) > OutOfBoundsM * CmPerM || FMath::Abs(Offset.Y) > OutOfBoundsM * CmPerM || Offset.Z < -FallThroughM * CmPerM)
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Ball left the playable area at %s; stopping it."), *Start.ToString());
        Result.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
        EnterRest();
        return false;
    }

    // Intersect the actual predicted segment before sweeping against the uncut green. This also catches
    // a complete crossing between two samples and keeps entry height/normal velocity on sloped greens.
    if (bWasOnGround && TryEnterCup(Start, Start + ToUnreal(Velocity * Dt) * CmPerM, PreviousVelocity, PreviousSpin, Dt))
    {
        UpdateCupHeight();
        return !bHoled;
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
        Params.bReturnPhysicalMaterial = true;
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
    if (bHoled)
    {
        return false;
    }

    // Match the headless contact step: move with the rebound for the unspent portion
    // of this timestep. Dropping it changes the next bounce speed enough to cross
    // the restitution cutoff, creating several yards of rollout disagreement.
    if (bHit && !bWasOnGround && !Hit.bStartPenetrating && ToCore(Hit.ImpactNormal).Y > 0.7f && !IsObstacle(Hit.GetComponent()))
    {
        const bool bSupportedAfterImpact = bOnGround;
        const FVector3f ReboundVelocity = Velocity;
        FHitResult RemainderHit;
        const bool bRemainderHit = MoveWithSweep(ToUnreal(Velocity * Dt * (1.0f - Hit.Time)) * CmPerM, RemainderHit);
        HandleCollision(bRemainderHit ? &RemainderHit : nullptr, bSupportedAfterImpact, ReboundVelocity);
        if (bHoled)
        {
            return false;
        }
    }
    TryRecoverToGround();

    ++ShotStep;
    if (!bOnGround)
    {
        ApplyCanopy(Dt);
    }

    UpdateCupHeight();
    Result.ApexM = FMath::Max(Result.ApexM, static_cast<float>(GetActorLocation().Z - ShotStart.Z) / CmPerM);
    // Before the rest check: a ball that stops over the cup drops, it never rests there.
    if (BallState == EOpenFairwayBallState::Rollout && bOnGround && TryCaptureInCup(GetActorLocation(), false))
    {
        return false;
    }
    if (BallState != EOpenFairwayBallState::Rest && GolfPhysics::CanRest(FBallState{FVector3f::ZeroVector, Velocity, Omega}, bOnGround, FloorNormal, SurfaceSettings, ShotParams.Rollout))
    {
        EnterRest();
    }
    return true;
}

void AOpenFairwayBall::ApplyCanopy(float Dt)
{
    // Canopy only overlaps the contact channel, so the move sweep and ground probes never see it.
    CanopyOverlaps.Reset();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(OpenFairwayBallCanopy), false, this);
    GetWorld()->OverlapMultiByChannel(CanopyOverlaps, GetActorLocation(), FQuat::Identity, TraceChannel, FCollisionShape::MakeSphere(GolfPhysics::Radius * CmPerM), Params);
    for (const FOverlapResult& Overlap : CanopyOverlaps)
    {
        if (IsCanopy(Overlap.GetComponent()))
        {
            const float PathM = Velocity.Size() * Dt;
            Omega *= CanopySpeedFactor(PathM);
            Velocity = ResolveCanopyDrag(Velocity, PathM, HashCombineFast(GetTypeHash(Overlap.ItemIndex), ShotStep));
            return;
        }
    }
}

void AOpenFairwayBall::HandleCollision(const FHitResult* Hit, bool bWasOnGround, const FVector3f& PreviousVelocity)
{
    if (Hit)
    {
        FVector3f Normal = ToCore(Hit->bStartPenetrating ? Hit->Normal : Hit->ImpactNormal);
        Normal = Normal.SizeSquared() > 0.000001f ? Normal / Normal.Size() : CoreUp;
        if (IsObstacle(Hit->GetComponent()))
        {
            // Trees and other upright obstacles deflect the ball; it never lands or rests on them.
            FVector Axis = Hit->GetComponent()->GetComponentLocation();
            FTransform Instance;
            if (const auto* Instances = Cast<UInstancedStaticMeshComponent>(Hit->GetComponent());
                Instances && Instances->GetInstanceTransform(Hit->Item, Instance, true))
            {
                Axis = Instance.GetLocation();
            }
            bOnGround = false;
            FloorNormal = CoreUp;
            Velocity = ResolveObstacleHit(Velocity, Normal, ToCore(GetActorLocation() - Axis));
        }
        else if (Normal.Y > 0.7f)
        {
            FloorNormal = Normal;
            SetSurface(ResolveSurface(Hit->GetComponent(), Hit->PhysMaterial.Get()));
            const bool bLandedFromFlight = BallState == EOpenFairwayBallState::Flight;
            const bool bIsLanding = bLandedFromFlight || (!bWasOnGround && FVector3f::DotProduct(Velocity, Normal) < 0.0f) ||
                                    FVector3f::DotProduct(PreviousVelocity, Normal) < -0.5f;
            if (bIsLanding)
            {
                if (bLandedFromFlight)
                {
                    RecordLanding(Normal);
                }
                // Descending into the cup, from flight or a bounce: a dunk instead of a bounce.
                if (FVector3f::DotProduct(Velocity, Normal) < 0.0f && TryCaptureInCup(Hit->ImpactPoint, true))
                {
                    if (bLandedFromFlight)
                    {
                        OnBallLanded.Broadcast(Result);
                    }
                    return;
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
        SetSurface(ResolveSurface(Ground.Component.Get(), Ground.PhysMaterial.Get()));
    }
    else
    {
        bOnGround = false;
        FloorNormal = CoreUp;
    }
}

void AOpenFairwayBall::RecordLanding(const FVector3f& Normal)
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

bool AOpenFairwayBall::TryCaptureInCup(const FVector& Location, bool bLanding)
{
    for (const FOpenFairwayCup& Cup : Cups)
    {
        const FQuat4f Rotation = CupRotation(Cup);
        const FVector3f Probe = Rotation.UnrotateVector(ToCore(Location - Cup.Position) / CmPerM);
        const FVector3f LocalVelocity = Rotation.UnrotateVector(Velocity);
        if (bLanding ? CupCapture::CapturesLanding(Probe, FVector3f::ZeroVector)
                     : CupCapture::CapturesRolling(Probe, LocalVelocity, FVector3f::ZeroVector))
        {
            StartHoleDrop(Cup, bLanding ? TEXT(" on the fly") : TEXT(""));
            return true;
        }
    }
    return false;
}

bool AOpenFairwayBall::TryEnterCup(const FVector& Start, const FVector& End, const FVector3f& PreviousVelocity,
    const FVector3f& PreviousSpin, float Dt)
{
    // Choose the first crossed aperture, independent of the order courses register their cups.
    const FOpenFairwayCup* First = nullptr;
    float EntryFraction = 2;
    for (const FOpenFairwayCup& Cup : Cups)
    {
        const FQuat4f Rotation = CupRotation(Cup);
        const FVector3f A = Rotation.UnrotateVector(ToCore(Start - Cup.Position) / CmPerM);
        const FVector3f D = Rotation.UnrotateVector(ToCore(End - Start) / CmPerM);
        const float LengthSquared = D.X * D.X + D.Z * D.Z;
        if (LengthSquared < 1e-12f)
            continue;
        const float Along = A.X * D.X + A.Z * D.Z;
        const float Outside = A.X * A.X + A.Z * A.Z - FMath::Square(CupCapture::CupRadiusM);
        const float Discriminant = Along * Along - LengthSquared * Outside;
        if (Discriminant < 0)
            continue;
        const float Fraction = Outside < 0 ? 0 : (-Along - FMath::Sqrt(Discriminant)) / LengthSquared;
        if (Fraction < 0 || Fraction > 1 || Fraction >= EntryFraction)
            continue;
        const FVector3f Entry = A + D * Fraction;
        if (FMath::Abs(Entry.Y) > 2 * GolfPhysics::Radius)
            continue;
        First = &Cup;
        EntryFraction = Fraction;
    }
    if (!First)
        return false;
    const FOpenFairwayCup Cup = *First;
    const FQuat4f Rotation = CupRotation(Cup);
    SetActorLocation(FMath::Lerp(Start, End, EntryFraction), false, nullptr, ETeleportType::TeleportPhysics);
    Velocity = FMath::Lerp(PreviousVelocity, Velocity, EntryFraction);
    Omega = FMath::Lerp(PreviousSpin, Omega, EntryFraction);
    const FVector3f Local = Rotation.UnrotateVector(ToCore(GetActorLocation() - Cup.Position) / CmPerM);
    const FVector3f LocalVelocity = Rotation.UnrotateVector(Velocity);
    const float Speed = FVector2f(LocalVelocity.X, LocalVelocity.Z).Size();
    const float Offset = Speed > 1e-4f ? FMath::Abs(Local.X * LocalVelocity.Z - Local.Z * LocalVelocity.X) / Speed : 0;
    const float MaxSpeed = CupCapture::MaxCaptureSpeed(Offset);
    if (Speed <= MaxSpeed)
    {
        StartHoleDrop(Cup, TEXT(""));
        return true;
    }
    Result.bOverCup = true;
    Result.OverCupSpeedMps = Speed;
    Result.OverCupMaxSpeedMps = MaxSpeed;
    Result.CupEntrySpinRpm = Omega.Size() / GolfPhysics::RadPerRpm;
    Result.CupEntryOffsetM = Offset;
    Result.CupExitVelocityMps = FVector::ZeroVector;
    Result.CupMaxHeightM = 0;
    MeasuredCup = Cup;
    CrossingCup = Cup;
    CrossingEntrySpeed = Speed;
    bOnGround = false;
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d crossed the cup at %.3f m/s, offset %.4f m, spin %.1f rpm, normal %s (capture limit %.3f m/s)."),
        Result.ShotNumber, Speed, Offset, Result.CupEntrySpinRpm, *Cup.Normal.ToString(), MaxSpeed);
    AdvanceCupCrossing(Dt * (1 - EntryFraction));
    return true;
}

void AOpenFairwayBall::StartHoleDrop(const FOpenFairwayCup& Cup, const TCHAR* How)
{
    bHoled = true;
    Result.bHoled = true;
    // Even one that crossed too fast first: it dropped.
    Result.bOverCup = false;
    HoleDropFrom = GetActorLocation();
    const FQuat4f Rotation = CupRotation(Cup);
    const FVector3f LocalFrom = Rotation.UnrotateVector(ToCore(HoleDropFrom - Cup.Position) / CmPerM);
    HoleDropTo = Cup.Position + ToUnreal(Rotation.RotateVector(CupCapture::DropPoint(LocalFrom, FVector3f::ZeroVector, PoleRadiusCm / CmPerM))) * CmPerM;
    HoleDropElapsed = 0.0f;
    // Still moving until the drop finishes; off the ground so nothing rolls it on.
    BallState = EOpenFairwayBallState::Rollout;
    bOnGround = false;
    Velocity = FVector3f::ZeroVector;
    Omega = FVector3f::ZeroVector;
    UpdateResult();
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d holed%s after %.1f s."), Result.ShotNumber, How, Result.ElapsedS);
    OnBallHoled.Broadcast(Result);
}

bool AOpenFairwayBall::StepCupCrossing(float Dt)
{
    Result.ElapsedS += Dt;
    if (Result.ElapsedS > MaxShotSeconds)
    {
        UE_LOG(LogOpenFairway, Warning, TEXT("Shot %d still moving after %.1f s; stopping it."), Result.ShotNumber, MaxShotSeconds);
        Result.TerminationReason = EOpenFairwayShotTermination::SafetyStop;
        CrossingCup.Reset();
        EnterRest();
        return false;
    }
    ++ShotStep;
    AdvanceCupCrossing(Dt);
    return !bHoled;
}

bool AOpenFairwayBall::AdvanceCupCrossing(float Dt)
{
    const FOpenFairwayCup Cup = *CrossingCup;
    const FQuat4f Rotation = CupRotation(Cup);
    FBallState State{Rotation.UnrotateVector(ToCore(GetActorLocation() - Cup.Position) / CmPerM),
        Rotation.UnrotateVector(Velocity), Rotation.UnrotateVector(Omega)};
    const auto Crossing = CupCapture::AdvanceCrossing(State, FVector3f::ZeroVector,
        Rotation.UnrotateVector(FVector3f(0, -GolfPhysics::Gravity, 0)), Dt);
    SetActorLocation(Cup.Position + ToUnreal(Rotation.RotateVector(State.Position)) * CmPerM, false, nullptr, ETeleportType::TeleportPhysics);
    Velocity = Rotation.RotateVector(State.Velocity);
    Omega = Rotation.RotateVector(State.Spin);
    UpdateCupHeight();
    if (Crossing.Outcome == CupCapture::ECupCrossing::OverCup)
        return true;

    CrossingCup.Reset();
    if (Crossing.Outcome == CupCapture::ECupCrossing::Holed)
    {
        StartHoleDrop(Cup, TEXT(" off the back of the cup"));
        return false;
    }
    // Above the surface is flight even for a small positive departure velocity. Do not erase a genuine hop.
    bOnGround = State.Position.Y <= GolfPhysics::Radius + 1e-6f && State.Velocity.Y <= 0;
    FloorNormal = ToCore(Cup.Normal);
    if (bOnGround)
        Velocity = RemoveVelocityAlongNormal(Velocity, FloorNormal, false);
    Result.CupExitVelocityMps = ToUnreal(Velocity);
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d left the cup: local velocity %s m/s (entered %.3f m/s)%s."),
        Result.ShotNumber, *State.Velocity.ToString(), CrossingEntrySpeed, bOnGround ? TEXT("") : TEXT(", airborne"));
    const float Remaining = Dt - Crossing.ConsumedSeconds;
    if (Remaining > 1e-7f)
    {
        // The caller already counted Dt; ordinary physics accounts for the remainder itself.
        Result.ElapsedS -= Remaining;
        StepPhysics(Remaining);
    }
    return false;
}

void AOpenFairwayBall::UpdateCupHeight()
{
    if (MeasuredCup)
    {
        const float Height = FVector::DotProduct(GetActorLocation() - MeasuredCup->Position, MeasuredCup->Normal) / CmPerM - GolfPhysics::Radius;
        Result.CupMaxHeightM = FMath::Max(Result.CupMaxHeightM, Height);
    }
}

void AOpenFairwayBall::StepHoleDrop(float DeltaSeconds)
{
    // Slides over the cup in the first half while falling with an eased (t squared) drop.
    HoleDropElapsed += DeltaSeconds;
    const float T = FMath::Min(HoleDropElapsed / HoleDropSeconds, 1.0f);
    FVector Location = FMath::Lerp(HoleDropFrom, HoleDropTo, FMath::Min(T * 2.0f, 1.0f));
    Location.Z = FMath::Lerp(HoleDropFrom.Z, HoleDropTo.Z, T * T);
    SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    if (T >= 1.0f)
    {
        HoleDropElapsed = -1.0f;
        EnterRest();
    }
}

bool AOpenFairwayBall::TryRecoverToGround()
{
    // Probe from above: a ball inside or under the terrain still finds the ground's top face.
    FGroundHit Ground;
    const FVector Location = GetActorLocation();
    if (BallState == EOpenFairwayBallState::Rest || bHoled || !ProbeGround(Ground, RecoveryProbeUpM, RecoveryProbeDownM) || Ground.Normal.Y <= 0.7f ||
        Ground.Location.Z - Location.Z < BelowGroundRecoveryM * CmPerM)
    {
        return false;
    }

    SetActorLocation(Ground.Location + ToUnreal(Ground.Normal) * (GolfPhysics::Radius + GroundSnapOffsetM) * CmPerM, false, nullptr, ETeleportType::TeleportPhysics);
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
        OnBallLanded.Broadcast(Result);
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

    const FVector Location = GetActorLocation();
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
    const FVector3f Normal = ToCore(Hit.ImpactNormal);
    OutHit.Normal = Normal.SizeSquared() > 0.000001f ? Normal / Normal.Size() : CoreUp;
    OutHit.Component = Hit.GetComponent();
    OutHit.PhysMaterial = Hit.PhysMaterial;
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
    if (Result.TerminationReason == EOpenFairwayShotTermination::None)
        Result.TerminationReason = EOpenFairwayShotTermination::Rest;
    BallState = EOpenFairwayBallState::Rest;
    Velocity = FVector3f::ZeroVector;
    Omega = FVector3f::ZeroVector;
    SubstepAccumulator = 0.0f;
    if (!Result.bLanded && Result.TerminationReason != EOpenFairwayShotTermination::SafetyStop)
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
