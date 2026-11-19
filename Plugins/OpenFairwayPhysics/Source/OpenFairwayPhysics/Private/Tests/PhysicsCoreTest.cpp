// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayPhysicsCoreTest, "UOpenGolf.Physics.Core",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayPhysicsCoreTest::RunTest(const FString& Parameters)
{
    // Air at 75 F, sea level (the headless default), and Sutherland's reference viscosity at 0 C.
    const float TemperatureC = Atmosphere::FahrenheitToCelsius(75.0f);
    TestEqual(TEXT("Air density"), Atmosphere::Density(0.0f, TemperatureC), 1.1883f, 1e-4f);
    // Water vapour is lighter than dry air; the dry share is the partial pressure, not the total.
    TestEqual(TEXT("Humid air density"), Atmosphere::Density(0.0f, TemperatureC, 0.5f), 1.18175f, 1e-4f);
    TestEqual(TEXT("Station pressure density (77 F, 50%, 14.69 psi)"), Atmosphere::Density(0.0f, 25.0f, 0.5f, 101283.0f), 1.17642f, 2e-4f);
    TestEqual(TEXT("Humidity clamps at saturation"), Atmosphere::Density(0.0f, TemperatureC, 1.5f), Atmosphere::Density(0.0f, TemperatureC, 1.0f));
    TestEqual(TEXT("NaN humidity is dry air"), Atmosphere::Density(0.0f, TemperatureC, NAN), Atmosphere::Density(0.0f, TemperatureC));
    TestEqual(TEXT("Air viscosity at 0 C"), Atmosphere::DynamicViscosity(0.0f), 1.716e-5f, 1e-10f);
    TestEqual(TEXT("Air viscosity at 75 F"), Atmosphere::DynamicViscosity(TemperatureC), 1.8318e-5f, 1e-8f);

    // Launch vectors for the reference wood shot. Velocity matches the openfairway shot debug
    // output (50.79, 6.190979, -0.55538); ogolf-physics also turns the spin with the launch
    // direction, so the 202.3108 rad/s of backspin gains a small X component.
    FLaunchData Wood;
    Wood.SpeedMph = 114.461673984528f;
    Wood.VerticalLaunchDeg = 6.94929695129395f;
    Wood.HorizontalLaunchDeg = -0.626494884490967f;
    Wood.BackSpinRpm = 1931.92130977561f;
    Wood.SideSpinRpm = -47.7512358712742f;
    Wood.TotalSpinRpm = 1932.51135253906f;
    Wood.SpinAxisDeg = -1.41588962078094f;
    TestTrue(TEXT("Wood launch valid"), Wood.IsValid());
    const FShotSpin Spin = ShotSetup::ParseSpin(Wood);
    TestEqual(TEXT("Consistent total spin kept"), Spin.TotalSpinRpm, 1932.51135253906f, 1e-3f);
    const FLaunchVectors Vectors = ShotSetup::BuildLaunchVectors(Wood.SpeedMph, Wood.VerticalLaunchDeg, Wood.HorizontalLaunchDeg, Spin.BackSpinRpm, Spin.SideSpinRpm);
    TestEqual(TEXT("Velocity X"), Vectors.Velocity.X, 50.79f, 0.005f);
    TestEqual(TEXT("Velocity Y"), Vectors.Velocity.Y, 6.190979f, 1e-3f);
    TestEqual(TEXT("Velocity Z"), Vectors.Velocity.Z, -0.55538f, 1e-4f);
    TestEqual(TEXT("Omega X"), Vectors.Omega.X, 2.2121f, 1e-3f);
    TestEqual(TEXT("Omega Y"), Vectors.Omega.Y, 5.000509f, 1e-3f);
    TestEqual(TEXT("Omega Z"), Vectors.Omega.Z, 202.2987f, 1e-3f);
    TestEqual(TEXT("Spin rate kept"), Vectors.Omega.Size(), FMath::Sqrt(202.3108f * 202.3108f + 5.000509f * 5.000509f), 1e-3f);
    TestEqual(TEXT("Shot direction"), Vectors.ShotDirection.Z, -0.010934f, 1e-5f);

    // Spin parsing derives what was not measured; measured components win over a stale total.
    FLaunchData AxisOnly;
    AxisOnly.SpeedMph = 100.0f;
    AxisOnly.TotalSpinRpm = 3000.0f;
    AxisOnly.SpinAxisDeg = 10.0f;
    const FShotSpin Derived = ShotSetup::ParseSpin(AxisOnly);
    TestEqual(TEXT("Derived backspin"), Derived.BackSpinRpm, 3000.0f * FMath::Cos(FMath::DegreesToRadians(10.0f)), 0.1f);
    TestEqual(TEXT("Derived sidespin"), Derived.SideSpinRpm, 3000.0f * FMath::Sin(FMath::DegreesToRadians(10.0f)), 0.1f);
    FLaunchData Inconsistent;
    Inconsistent.BackSpinRpm = 3000.0f;
    Inconsistent.SideSpinRpm = 400.0f;
    Inconsistent.TotalSpinRpm = 2000.0f;
    TestEqual(TEXT("Components win"), ShotSetup::ParseSpin(Inconsistent).TotalSpinRpm, FMath::Sqrt(3000.0f * 3000.0f + 400.0f * 400.0f), 0.1f);
    FLaunchData Invalid;
    Invalid.SpeedMph = -1.0f;
    TestFalse(TEXT("Negative speed rejected"), Invalid.IsValid());

    const FBallPhysicsProfile Profile;
    // Ground friction, rest and rebound rules.
    const FRolloutProfile Rollout;
    TestEqual(TEXT("No spin, no extra friction"), GolfPhysics::SpinFrictionMultiplier(0.0f, 40.0f, Rollout), 1.0f, 1e-6f);
    TestEqual(TEXT("High spin friction"), GolfPhysics::SpinFrictionMultiplier(3000.0f, 40.0f, Rollout), 2.5f, 1e-5f);
    const FSurfaceSettings Fairway = FResolvedShotParams().ResolveSurface(ESurface::Fairway, Profile);
    const FVector3f LevelNormal(0.0f, 1.0f, 0.0f);
    TestTrue(TEXT("Slow ball rests on flat ground"), GolfPhysics::CanRest(FBallState{FVector3f::ZeroVector, FVector3f(0.01f, 0.0f, 0.0f), FVector3f::ZeroVector}, true, LevelNormal, Fairway));
    TestFalse(TEXT("Unsupported ball does not rest"), GolfPhysics::CanRest(FBallState(), false, LevelNormal, Fairway));
    const FVector3f Steep = FVector3f(0.5f, 0.866f, 0.0f).GetSafeNormal();
    TestFalse(TEXT("Steep slope keeps the ball moving"), GolfPhysics::CanRest(FBallState(), true, Steep, Fairway));
    TestTrue(TEXT("Fast upward ball rebounds"), GolfPhysics::HasRebound(FVector3f(0.0f, 2.0f, 0.0f), LevelNormal, TOptional<FSurfaceBounceSettings>()));
    TestFalse(TEXT("Slow upward ball settles"), GolfPhysics::HasRebound(FVector3f(0.0f, 0.2f, 0.0f), LevelNormal, TOptional<FSurfaceBounceSettings>()));
    const FBallState Leaving{FVector3f::ZeroVector, FVector3f(10.0f, 3.0f, 0.0f), FVector3f(0.0f, 0.0f, 100.0f)};
    const FBallState Unchanged = GolfPhysics::ResolveImpact(Leaving, LevelNormal, Fairway, FBounceProfile());
    TestTrue(TEXT("A ball leaving the surface is not bounced"), Unchanged.Velocity.Equals(Leaving.Velocity));

    // Green speed: a faster green rolls more freely; stimps are clamped to the supported range.
    TestTrue(TEXT("Faster green, less friction"), GreenSpeed::RollingFriction(13.0f) < GreenSpeed::RollingFriction(9.0f));
    TestEqual(TEXT("Stimp clamped"), GreenSpeed::Clamp(30.0f), GreenSpeed::MaxStimpFeet);
    // The friction is solved with the green's grass drag and the air: more drag leaves less for rolling friction, and
    // no drag at all is close to the constant-deceleration stimp rule.
    TestEqual(TEXT("Default green drag"), FBallPhysicsProfile().GreenGrassViscosity, 0.0009f);
    TestTrue(TEXT("More grass drag, less rolling friction"), GreenSpeed::RollingFriction(10.0f, 0.011f) < GreenSpeed::RollingFriction(10.0f));
    const float StimpDistanceM = 10.0f * MetersPerFoot;
    const float ConstantDeceleration = 7.0f * GreenSpeed::StimpmeterSpeedMps * GreenSpeed::StimpmeterSpeedMps / (10.0f * GolfPhysics::Gravity * StimpDistanceM);
    TestTrue(TEXT("Drag-free green friction is below the constant-deceleration rule"), GreenSpeed::RollingFriction(10.0f, 0.0f) < ConstantDeceleration);
    TestEqual(TEXT("and within 5% of it"), GreenSpeed::RollingFriction(10.0f, 0.0f), ConstantDeceleration, ConstantDeceleration * 0.05f);
    return true;
}

#endif
