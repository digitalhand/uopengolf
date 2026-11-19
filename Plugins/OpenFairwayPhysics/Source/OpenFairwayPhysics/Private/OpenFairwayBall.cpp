// SPDX-License-Identifier: MIT
// The ball actor's lifecycle, launch, frame stepping, results and air. Contact handling follows ogolf-physics
// game/GolfBall.cs with Godot/Jolt MoveAndCollide and ray casts replaced by Chaos sphere sweeps and line traces; see
// OpenFairwayBallMotion.cpp (stepping and contact), OpenFairwayBallSurface.cpp (ground queries and surfaces),
// OpenFairwayBallCup.cpp and OpenFairwayBallPresentation.cpp.
#include "OpenFairwayBallInternal.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "OpenFairwayLog.h"
#include "UObject/ConstructorHelpers.h"

using namespace OpenFairway;
using namespace OpenFairwayBallDetail;

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
    SimLocation = GetActorLocation();
    SnapToGround();
}

void AOpenFairwayBall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearTracer();
    Super::EndPlay(EndPlayReason);
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

FFlightEnvironment AOpenFairwayBall::GetAirEnvironment() const
{
    return IsMoving() ? ShotParams.Environment : GetAirConditions().ToEnvironment();
}

FVector AOpenFairwayBall::GetWindVelocityMps() const
{
    return ToUnreal(GetAirEnvironment().Wind);
}

FVector AOpenFairwayBall::GetWindVelocityAtHeightMps(float HeightM) const
{
    return ToUnreal(WindProfile::At(GetAirEnvironment(), HeightM));
}

float AOpenFairwayBall::GetAirDensityKgM3() const
{
    return GetAirEnvironment().AirDensity;
}

float AOpenFairwayBall::GetWindReferenceHeightM() const
{
    return GetAirEnvironment().WindReferenceHeightM;
}

float AOpenFairwayBall::GetWindRoughnessM() const
{
    return GetAirEnvironment().WindRoughnessM;
}

FShotSimulationResult AOpenFairwayBall::PredictShot(const FOpenFairwayLaunch& LaunchData, EOpenFairwaySurface Surface) const
{
    // The headless shot is not turned by AimYawDeg, so give it the world-fixed wind relative to the aim line.
    FAirConditions Air = GetAirConditions();
    Air.WindFromDeg -= AimYawDeg;
    FBallPhysicsProfile Predicted = Profile;
    Predicted.GreenStimpFeet = GreenSpeed::Clamp(GreenStimpFeet);
    return ShotSimulator::Simulate(ToLaunchData(LaunchData), ToCoreSurface(Surface), Predicted, Air, MaxShotSeconds);
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
    AdoptExternalMove();
    SnapToGround();
    FGroundHit Ground;
    if (ProbeGround(Ground, GroundProbeUpM, GolfPhysics::Radius + GroundProbeDistanceM))
    {
        CurrentSurface = ResolveSurface(Ground.Component.Get(), Ground.PhysMaterial.Get());
    }

    const FShotSpin Spin = ShotSetup::ParseSpin(Data);
    // Every surface this shot touches resolves its green from this stimp.
    Profile.GreenStimpFeet = GreenSpeed::Clamp(GreenStimpFeet);
    ShotParams = FResolvedShotParams::Resolve(Profile, GetAirConditions(), Data.VerticalLaunchDeg, Spin.TotalSpinRpm, Spin.BackSpinRpm);
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
    StruckWeight = 0.0f;
    bHoled = false;
    CrossingCup.Reset();
    MeasuredCup.Reset();
    HoleDropElapsed = -1.0f;
    ShotStart = SimLocation;

    const int32 ShotNumber = Result.ShotNumber + 1;
    Result = FOpenFairwayShotResult();
    Result.ShotNumber = ShotNumber;
    Result.Launch = LaunchData;
    Result.LandingBucket = LandingBucket::GetName(ShotParams.LandingBucket);
    Result.Surface = CurrentSurface;
    bHasShot = true;

    ClearTracer();
    LastTracerPoint = ShotStart;

    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d: %.1f mph, VLA %.2f, HLA %.2f, spin %.0f rpm (back %.0f, side %.0f, axis %.2f), landing %s, lie %s, green stimp %.1f ft."),
        ShotNumber, Data.SpeedMph, Data.VerticalLaunchDeg, Data.HorizontalLaunchDeg, Spin.TotalSpinRpm, Spin.BackSpinRpm, Spin.SideSpinRpm, Spin.SpinAxisDeg,
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
    SimLocation = PresentedLocation = Location;
    BallState = EOpenFairwayBallState::Rest;
    bHoled = false;
    CrossingCup.Reset();
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

    AdoptExternalMove();
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
        PreviousSimLocation = SimLocation;
        if (!StepPhysics(GolfPhysics::SimulationDt) || BallState == EOpenFairwayBallState::Rest)
        {
            // Stopped or holed: shown exactly where the simulation left it.
            SubstepAccumulator = 0.0f;
            PresentAt(SimLocation);
            ApplyVisualRoll(GetActorLocation() - FrameStart);
            return;
        }
    }

    // Prevent runaway catch-up after a stall while keeping the motion continuous.
    if (Substeps == MaxSubstepsPerFrame && SubstepAccumulator > GolfPhysics::SimulationDt)
    {
        SubstepAccumulator = GolfPhysics::SimulationDt;
    }
    // Frames rarely end on a step. The time left over places the ball between the last two steps, so it advances
    // every frame at any frame rate instead of holding still or jumping two steps.
    PresentAt(FMath::Lerp(PreviousSimLocation, SimLocation, FMath::Clamp(SubstepAccumulator / GolfPhysics::SimulationDt, 0.0f, 1.0f)));
    ApplyVisualRoll(GetActorLocation() - FrameStart);
    UpdateResult();
    AddTracerPoint(false);
}

void AOpenFairwayBall::BroadcastLanded()
{
    PresentAt(SimLocation);
    OnBallLanded.Broadcast(Result);
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
        Result.CarryM = FMath::Max(FVector3f::DotProduct(CoreOffset(), ShotDirection), 0.0f);
    }
    Result.bAtRest = true;
    // At rest the ball is shown exactly where the simulation stopped it.
    PresentAt(SimLocation);
    UpdateResult();
    AddTracerPoint(true);
    UE_LOG(LogOpenFairway, Log, TEXT("Shot %d at rest on %s: carry %.1f yd, roll %.1f yd, total %.1f yd, offline %.1f yd, apex %.1f ft, %.1f s."),
        Result.ShotNumber, SurfaceCatalog::GetName(ToCoreSurface(CurrentSurface)), Result.CarryM * YardsPerMeter, Result.RollM * YardsPerMeter,
        Result.TotalM * YardsPerMeter, Result.LateralM * YardsPerMeter, Result.ApexM * FeetPerMeter, Result.ElapsedS);
    OnBallAtRest.Broadcast(Result);
}

void AOpenFairwayBall::UpdateResult()
{
    const FVector3f Offset = CoreOffset();
    Result.TotalM = FMath::Max(FVector3f::DotProduct(Offset, ShotDirection), 0.0f);
    // Offline from the aim line, positive to the right.
    const FVector3f AimRight = ShotSetup::Rotate(FVector3f(0.0f, 0.0f, 1.0f), CoreUp, FMath::DegreesToRadians(-AimYawDeg));
    Result.LateralM = FVector3f::DotProduct(Offset, AimRight);
    Result.RollM = Result.bLanded ? Result.TotalM - Result.CarryM : 0.0f;
    Result.Surface = CurrentSurface;
}

FVector3f AOpenFairwayBall::CoreOffset() const
{
    return ToCore(SimLocation - ShotStart) / CmPerM;
}

FVector3f AOpenFairwayBall::CorePosition() const
{
    return CoreOffset() + CoreUp * GolfPhysics::Radius;
}
