// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "LaunchMonitorTypes.h"
#include "Misc/AutomationTest.h"
#include "OpenFairwayBall.h"
#include "OpenFairway/WindProfile.h"
#include "OpenFairwayPhysicalMaterial.h"
#include "OpenGolfCourse.h"
#include "Tests/AutomationCommon.h"
#include "UOpenGolfGameMode.h"

namespace RolloutSurfaceTestDetail
{
    /**
     * Replay shot 0, the low wood, decoded from its Square Home frame: 51.17 m/s, 6.95 degrees up, 0.63 left,
     * 1933 rpm (1932 back, 48 left) on a 1.42-degree left axis. The game converts it the same way.
     */
    FOpenFairwayLaunch LowWood()
    {
        FLaunchMonitorShot Shot;
        Shot.BallSpeedMps = 51.17f;
        Shot.VerticalLaunchDeg = 6.95f;
        Shot.HorizontalLaunchDeg = -0.63f;
        Shot.TotalSpinRpm = 1933;
        Shot.SpinAxisDeg = -1.42f;
        Shot.BackSpinRpm = 1932;
        Shot.SideSpinRpm = -48;
        for (ELaunchMonitorValueSource* Source : {&Shot.BallSpeedSource, &Shot.VerticalLaunchSource, &Shot.HorizontalLaunchSource,
                 &Shot.TotalSpinSource, &Shot.SpinAxisSource, &Shot.BackSpinSource, &Shot.SideSpinSource})
            *Source = ELaunchMonitorValueSource::Measured;
        FOpenFairwayLaunch Launch;
        AUOpenGolfGameMode::ToLaunch(Shot, Launch);
        return Launch;
    }

    /** A level slab whose top is at Z = 0, from MinM to MaxM in metres. */
    UStaticMeshComponent* AddGround(UWorld& World, const FVector2D& MinM, const FVector2D& MaxM)
    {
        const FVector2D Center = (MinM + MaxM) * 0.5;
        const FVector2D Size = MaxM - MinM;
        // The engine cube is 1 m on a side, centred on its origin.
        const FTransform Transform(FRotator::ZeroRotator, FVector(Center.X * 100, Center.Y * 100, -50), FVector(Size.X, Size.Y, 1));
        auto* Actor = World.SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Transform);
        UStaticMeshComponent* Ground = Actor->GetStaticMeshComponent();
        Ground->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
        Ground->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
        return Ground;
    }

    void TagSurface(UStaticMeshComponent& Ground, EOpenFairwaySurface Surface)
    {
        Ground.ComponentTags = {AOpenFairwayBall::GetSurfaceTag(Surface)};
    }

    /** Hits the shot from the tee at the origin and steps the ball at 60 fps until it rests. */
    bool Play(AOpenFairwayBall& Ball, const FOpenFairwayLaunch& Launch, FOpenFairwayShotResult& Result, FAutomationTestBase& Test)
    {
        Ball.PlaceAt(FVector(0, 0, 50));
        if (!Test.TestTrue(TEXT("Shot launches"), Ball.Launch(Launch)))
            return false;
        for (int32 Step = 0; Ball.IsMoving() && Step < 7200; ++Step)
            Ball.Tick(1.0f / 60.0f);
        const bool bHasResult = Ball.GetShotResult(Result);
        return Test.TestTrue(TEXT("Shot comes to rest"), bHasResult && Result.bAtRest && Result.TerminationReason == EOpenFairwayShotTermination::Rest);
    }

    float Yards(float Meters)
    {
        return Meters * OpenFairway::YardsPerMeter;
    }
} // namespace RolloutSurfaceTestDetail

// The live ball on flat test ground, independent of any map: on fairway it matches the headless prediction,
// tags cannot change first-contact carry, and a green rolls farther than the same path tagged fairway.
// Untagged ground plays its physical material.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRolloutSurfaceTest, "UOpenGolf.Range.RolloutSurfaces",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRolloutSurfaceTest::RunTest(const FString& Parameters)
{
    using namespace RolloutSurfaceTestDetail;
    FTestWorldWrapper Wrapper;
    if (!Wrapper.CreateTestWorld(EWorldType::EditorPreview))
    {
        Wrapper.ForwardErrorMessages(this);
        return false;
    }
    UWorld& World = *Wrapper.GetTestWorld();
    // The ball lands on the approach about 111 m out and runs out across the far strip.
    UStaticMeshComponent* Approach = AddGround(World, FVector2D(-10, -30), FVector2D(120, 30));
    UStaticMeshComponent* Far = AddGround(World, FVector2D(120, -30), FVector2D(250, 30));
    AddGround(World, FVector2D(0, 60), FVector2D(10, 70))
        ->SetPhysMaterialOverride(LoadObject<UOpenFairwayPhysicalMaterial>(nullptr, *OpenGolfCourse::SurfacePhysicalMaterialPath(TEXT("Rough"))));
    AddGround(World, FVector2D(0, -70), FVector2D(10, -60))
        ->SetPhysMaterialOverride(LoadObject<UOpenFairwayPhysicalMaterial>(nullptr, *OpenGolfCourse::SurfacePhysicalMaterialPath(TEXT("Green"))));
    Wrapper.TickTestWorld(1.0f / 60.0f);
    FHitResult Probe;
    if (!TestTrue(TEXT("The test ground answers scene queries"),
            World.LineTraceSingleByChannel(Probe, FVector(0, 0, 100), FVector(0, 0, -100), ECC_Visibility)))
    {
        Wrapper.DestroyTestWorld(false);
        return false;
    }

    AOpenFairwayBall* Ball = World.SpawnActor<AOpenFairwayBall>();
    const FOpenFairwayLaunch Launch = LowWood();
    FOpenFairwayShotResult OnFairway, OnGreen;
    TagSurface(*Approach, EOpenFairwaySurface::Fairway);
    TagSurface(*Far, EOpenFairwaySurface::Fairway);
    const bool bFairwayPlayed = Play(*Ball, Launch, OnFairway, *this);
    TagSurface(*Far, EOpenFairwaySurface::Green);
    const bool bGreenPlayed = Play(*Ball, Launch, OnGreen, *this);
    const auto Predicted = Ball->PredictShot(Launch, EOpenFairwaySurface::Fairway);
    const auto OnRough = Ball->PredictShot(Launch, EOpenFairwaySurface::Rough);
    AddInfo(FString::Printf(TEXT("Fairway: carry %.4f, roll %.4f, total %.4f yd. Green: carry %.4f, roll %.4f, total %.4f yd. Headless fairway: carry %.4f, total %.4f yd."),
        Yards(OnFairway.CarryM), Yards(OnFairway.RollM), Yards(OnFairway.TotalM), Yards(OnGreen.CarryM), Yards(OnGreen.RollM),
        Yards(OnGreen.TotalM), Yards(Predicted.CarryM), Yards(Predicted.TotalM)));
    if (bFairwayPlayed && bGreenPlayed)
    {
        TestTrue(TEXT("The shot runs out across the far strip"), OnFairway.TotalM > 125.0f && OnFairway.CarryM < 120.0f);
        TestEqual(TEXT("Live carry matches the headless prediction (yd)"), Yards(OnFairway.CarryM), Yards(Predicted.CarryM), 0.1f);
        TestEqual(TEXT("Live total matches the headless prediction (yd)"), Yards(OnFairway.TotalM), Yards(Predicted.TotalM), 0.1f);
        TestEqual(TEXT("Surface tags cannot change first-contact carry (yd)"), Yards(OnGreen.CarryM), Yards(OnFairway.CarryM), 0.02f);
        TestEqual(TEXT("The green run ends on green"), OnGreen.Surface, EOpenFairwaySurface::Green);
        TestTrue(FString::Printf(TEXT("Green rolls farther than the same path tagged fairway (%.2f vs %.2f yd)"), Yards(OnGreen.RollM), Yards(OnFairway.RollM)),
            OnGreen.RollM > OnFairway.RollM);
    }

    // In wind the live ball flies through the same height profile as the prediction.
    Ball->WindSpeedMph = 10.0f;
    Ball->WindFromDeg = 45.0f;
    TagSurface(*Far, EOpenFairwaySurface::Fairway);
    FOpenFairwayShotResult InWind;
    const bool bWindPlayed = Play(*Ball, Launch, InWind, *this);
    const auto PredictedInWind = Ball->PredictShot(Launch, EOpenFairwaySurface::Fairway);
    AddInfo(FString::Printf(TEXT("10 mph from 45 deg: live carry %.4f, total %.4f, lateral %.4f yd; headless carry %.4f, total %.4f, lateral %.4f yd."),
        Yards(InWind.CarryM), Yards(InWind.TotalM), Yards(InWind.LateralM), Yards(PredictedInWind.CarryM), Yards(PredictedInWind.TotalM),
        Yards(PredictedInWind.LateralM)));
    if (bWindPlayed)
    {
        TestEqual(TEXT("In wind, live carry matches the headless prediction (yd)"), Yards(InWind.CarryM), Yards(PredictedInWind.CarryM), 0.1f);
        TestEqual(TEXT("In wind, live total matches the headless prediction (yd)"), Yards(InWind.TotalM), Yards(PredictedInWind.TotalM), 0.1f);
        TestEqual(TEXT("In wind, live lateral matches the headless prediction (yd)"), Yards(InWind.LateralM), Yards(PredictedInWind.LateralM), 0.1f);
    }
    // 1 m above the ground gets about 60% of the 10 m wind (UOpenGolf.Physics.WindProfile).
    const float OneMetreShare = OpenFairway::WindProfile::Factor(1.0f, Ball->GetWindReferenceHeightM(), Ball->GetWindRoughnessM());
    TestTrue(TEXT("The ball reports the wind at a height through its profile"),
        Ball->GetWindVelocityAtHeightMps(1.0f).Equals(Ball->GetWindVelocityMps() * OneMetreShare, 0.01));
    TestEqual(TEXT("The ball's profile is the 10 m standard"), Ball->GetWindReferenceHeightM(), OpenFairway::WindProfile::StandardReferenceHeightM);
    Ball->WindSpeedMph = 0.0f;

    auto LieAt = [Ball](float XM, float YM)
    {
        Ball->PlaceAt(FVector(XM * 100, YM * 100, 50));
        FOpenFairwayShotResult Lie;
        Ball->GetShotResult(Lie);
        return Lie.Surface;
    };
    TestEqual(TEXT("Untagged PM_Rough ground plays as rough"), LieAt(5, 65), EOpenFairwaySurface::Rough);
    TestEqual(TEXT("Untagged PM_Green ground plays as green"), LieAt(5, -65), EOpenFairwaySurface::Green);
    Wrapper.DestroyTestWorld(false);

    const OpenFairway::FBallPhysicsProfile Profile;
    const auto Params = OpenFairway::FResolvedShotParams::Resolve(Profile, OpenFairway::FAirConditions(), 51.17f / OpenFairway::MpsPerMph, 6.95f, 1933.0f);
    TestEqual(TEXT("Regime kinetic friction neutral"), Params.Regime.KineticFrictionMultiplier, 1.0f, 1e-6f);
    TestEqual(TEXT("Regime rolling friction neutral"), Params.Regime.RollingFrictionMultiplier, 1.0f, 1e-6f);
    TestEqual(TEXT("Regime grass drag neutral"), Params.Regime.GrassViscosityMultiplier, 1.0f, 1e-6f);
    TestEqual(TEXT("Run adds no bite"), Params.Landing.SpinBiteScale, 0.0f);

    // The same shot on rough carries the same and runs out much shorter.
    const float FairwayRollYd = Yards(Predicted.TotalM - Predicted.CarryM);
    const float RoughRollYd = Yards(OnRough.TotalM - OnRough.CarryM);
    AddInfo(FString::Printf(TEXT("Headless roll: fairway %.2f yd, rough %.2f yd"), FairwayRollYd, RoughRollYd));
    TestEqual(TEXT("Rough carry matches fairway"), OnRough.CarryM, Predicted.CarryM, 0.01f);
    TestTrue(TEXT("Rough rolls much shorter than fairway"), RoughRollYd < 0.8f * FairwayRollYd);
    return true;
}
#endif
