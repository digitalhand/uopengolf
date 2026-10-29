// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/BallProfile.h"
#include "OpenFairway/GolfPhysics.h"
#include "OpenFairway/ShotSimulator.h"
#include "OpenFairway/WindProfile.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayWindProfileTest, "UOpenGolf.Physics.WindProfile",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayWindHeightTest, "UOpenGolf.Physics.WindHeight",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace WindProfileTestDetail
{
    FLaunchData Shot(float SpeedMph, float VerticalLaunchDeg, float TotalSpinRpm)
    {
        FLaunchData Launch;
        Launch.SpeedMph = SpeedMph;
        Launch.VerticalLaunchDeg = VerticalLaunchDeg;
        Launch.TotalSpinRpm = TotalSpinRpm;
        Launch.SpinAxisDeg = 0.0f;
        return Launch;
    }

    FLaunchData Driver() { return Shot(167.0f, 10.9f, 2686.0f); }
    FLaunchData Wedge() { return Shot(102.0f, 24.2f, 9304.0f); }
    FLaunchData Punch() { return Shot(95.0f, 8.0f, 4200.0f); }
    FLaunchData WindIron() { return Shot(120.0f, 16.3f, 7097.0f); }

    FAirConditions Air(float SpeedMph, float FromDeg, bool bProfiled)
    {
        FAirConditions Conditions;
        Conditions.WindSpeedMph = SpeedMph;
        Conditions.WindFromDeg = FromDeg;
        if (!bProfiled)
        {
            Conditions.WindReferenceHeightM = 0.0f;
        }
        return Conditions;
    }

    float Yards(float Meters)
    {
        return Meters * YardsPerMeter;
    }
} // namespace WindProfileTestDetail

bool FOpenFairwayWindProfileTest::RunTest(const FString& Parameters)
{
    constexpr float H = WindProfile::StandardReferenceHeightM;
    constexpr float Z = WindProfile::OpenGrassRoughnessM;
    constexpr float Tolerance = 0.002f;
    auto F = [](float HeightM, float ReferenceHeightM, float RoughnessM) { return WindProfile::Factor(HeightM, ReferenceHeightM, RoughnessM); };

    // Neutral log law over open short grass, with the authored wind as the 10 m reading.
    TestEqual(TEXT("The reference height gets the authored wind"), F(10.0f, H, Z), 1.0f, Tolerance);
    TestEqual(TEXT("1 m gets about 60%"), F(1.0f, H, Z), 0.6036f, Tolerance);
    TestEqual(TEXT("The flag centre gets about 72%"), F(1.9558f, H, Z), 0.7191f, Tolerance);
    TestEqual(TEXT("A 30 m apex gets about 119%"), F(30.0f, H, Z), 1.1891f, Tolerance);
    TestEqual(TEXT("100 m gets about 140%"), F(100.0f, H, Z), 1.3964f, Tolerance);
    TestEqual(TEXT("Heights above 100 m are capped"), F(1000.0f, H, Z), F(100.0f, H, Z));

    // At and below the roughness length, and for invalid heights, there is no wind.
    TestEqual(TEXT("No wind at the roughness length"), F(0.03f, H, Z), 0.0f);
    TestEqual(TEXT("No wind at the ground"), F(0.0f, H, Z), 0.0f);
    TestEqual(TEXT("No wind below the launch ground"), F(-5.0f, H, Z), 0.0f);
    TestEqual(TEXT("No wind at a non-finite height"), F(NAN, H, Z), 0.0f);

    // Settings that cannot describe a profile fall back to uniform wind.
    TestEqual(TEXT("No reference height is uniform"), F(5.0f, 0.0f, Z), 1.0f);
    TestEqual(TEXT("A reference height inside the roughness is uniform"), F(5.0f, 0.02f, Z), 1.0f);
    TestEqual(TEXT("A non-finite reference height is uniform"), F(5.0f, NAN, Z), 1.0f);
    TestEqual(TEXT("Zero roughness is uniform"), F(5.0f, H, 0.0f), 1.0f);
    TestEqual(TEXT("Negative roughness is uniform"), F(5.0f, H, -1.0f), 1.0f);
    TestEqual(TEXT("Non-finite roughness is uniform"), F(5.0f, H, NAN), 1.0f);
    // A roughness too small for Height / Roughness to fit in a float must still give a finite share.
    for (const float Tiny : {2e-38f, 1e-37f})
    {
        const float Share = F(100.0f, H, Tiny);
        TestTrue(FString::Printf(TEXT("A tiny roughness (%g m) gives a finite share (%g)"), Tiny, Share), FMath::IsFinite(Share) && Share >= 0.0f);
    }

    const FVector3f Reference(4.0f, 0.0f, 3.0f);
    TestTrue(TEXT("At scales the reference wind by the factor"),
        WindProfile::At(Reference, 1.0f, H, Z).Equals(Reference * F(1.0f, H, Z), 1e-5f));
    return true;
}

bool FOpenFairwayWindHeightTest::RunTest(const FString& Parameters)
{
    using namespace WindProfileTestDetail;
    const FBallPhysicsProfile Profile;

    const FResolvedShotParams Params = FResolvedShotParams::Resolve(Profile, FAirConditions(), 120.0f, 16.3f, 7097.0f);
    TestEqual(TEXT("Resolve copies the reference height"), Params.Environment.WindReferenceHeightM, 10.0f);
    TestEqual(TEXT("Resolve copies the roughness"), Params.Environment.WindRoughnessM, 0.03f);

    // One flight step reads the ball's height from its position.
    FFlightEnvironment Windy;
    Windy.Wind = FVector3f(-4.47f, 0.0f, 0.0f);
    Windy.WindReferenceHeightM = 10.0f;
    FFlightEnvironment Still = Windy;
    Still.Wind = FVector3f::ZeroVector;
    FFlightEnvironment Uniform = Windy;
    Uniform.WindReferenceHeightM = 0.0f;
    auto Step = [](const FFlightEnvironment& Environment, float HeightM)
    {
        const FBallState State{FVector3f(0.0f, HeightM, 0.0f), FVector3f(40.0f, 15.0f, 0.0f), FVector3f(0.0f, 0.0f, 300.0f)};
        return GolfPhysics::StepFlight(State, Environment, FFlightProfile(), GolfPhysics::SimulationDt);
    };
    const FBallState AtLaunch = Step(Windy, GolfPhysics::Radius);
    const FBallState CalmLaunch = Step(Still, GolfPhysics::Radius);
    TestTrue(TEXT("At launch height the ball is below the roughness and feels no wind"),
        AtLaunch.Velocity.Equals(CalmLaunch.Velocity, 1e-6f) && AtLaunch.Spin.Equals(CalmLaunch.Spin, 1e-6f));
    const FBallState AtReference = Step(Windy, 10.0f);
    const FBallState UniformReference = Step(Uniform, 10.0f);
    TestTrue(TEXT("At the reference height the step matches uniform wind"),
        AtReference.Velocity.Equals(UniformReference.Velocity, 1e-5f) && AtReference.Spin.Equals(UniformReference.Spin, 1e-5f));
    const FBallState BelowGround = Step(Windy, -3.0f);
    TestTrue(TEXT("Below the launch ground the step stays finite"), !BelowGround.Velocity.ContainsNaN() && !BelowGround.Spin.ContainsNaN());

    // Calm air is untouched by the profile settings.
    const FShotSimulationResult Calm = ShotSimulator::Simulate(WindIron(), ESurface::Fairway, Profile, FAirConditions());
    const FShotSimulationResult CalmUniform = ShotSimulator::Simulate(WindIron(), ESurface::Fairway, Profile, Air(0.0f, 0.0f, false));
    TestTrue(TEXT("Calm air gives identical results with and without the profile"),
        Calm.CarryM == CalmUniform.CarryM && Calm.TotalM == CalmUniform.TotalM && Calm.LateralM == CalmUniform.LateralM);

    const FAirConditions WithWind = Air(10.0f, 0.0f, true);
    TestTrue(TEXT("Below the roughness length there is no wind"), WithWind.WindVelocityAt(0.01f).IsNearlyZero(1e-6f));

    // A low punch stays under the reference height, so a tailwind changes its carry less than uniform wind
    // would. A headwind is not used: this low, high-spin launch carries farther into a headwind, and that
    // gain peaks below 10 mph, so less wind can lengthen it more.
    const FShotSimulationResult PunchCalm = ShotSimulator::Simulate(Punch(), ESurface::Fairway, Profile, FAirConditions());
    TestTrue(FString::Printf(TEXT("The punch stays low (apex %.1f m)"), PunchCalm.ApexM), PunchCalm.ApexM < 9.0f);
    const float PunchProfiledChange = ShotSimulator::Simulate(Punch(), ESurface::Fairway, Profile, Air(10.0f, 180.0f, true)).CarryM - PunchCalm.CarryM;
    const float PunchUniformChange = ShotSimulator::Simulate(Punch(), ESurface::Fairway, Profile, Air(10.0f, 180.0f, false)).CarryM - PunchCalm.CarryM;
    TestTrue(FString::Printf(TEXT("A tailwind changes a low punch's carry less than uniform wind (%+.2f vs %+.2f yd)"), Yards(PunchProfiledChange),
                 Yards(PunchUniformChange)),
        PunchUniformChange != 0.0f && FMath::Abs(PunchProfiledChange) < FMath::Abs(PunchUniformChange));

    // A high wedge spends more of its flight in stronger air than a low punch does.
    auto CrossShare = [&Profile](const FLaunchData& Launch)
    {
        const float CalmLateral = ShotSimulator::Simulate(Launch, ESurface::Fairway, Profile, FAirConditions()).LateralM;
        const float Profiled = ShotSimulator::Simulate(Launch, ESurface::Fairway, Profile, Air(10.0f, 90.0f, true)).LateralM - CalmLateral;
        const float UniformDrift = ShotSimulator::Simulate(Launch, ESurface::Fairway, Profile, Air(10.0f, 90.0f, false)).LateralM - CalmLateral;
        return Profiled / UniformDrift;
    };
    const float WedgeShare = CrossShare(Wedge());
    const float PunchShare = CrossShare(Punch());
    TestTrue(FString::Printf(TEXT("A high wedge keeps more of its crosswind drift than a low punch (%.3f vs %.3f)"), WedgeShare, PunchShare),
        WedgeShare > PunchShare);

    // Measured effect, for Docs/PhasesStatus.md: uniform against profiled, 10 mph.
    const TPair<const TCHAR*, FLaunchData> Shots[] = {{TEXT("Driver"), Driver()}, {TEXT("Wedge"), Wedge()}, {TEXT("Punch"), Punch()}};
    const TPair<const TCHAR*, float> Winds[] = {{TEXT("head"), 0.0f}, {TEXT("tail"), 180.0f}, {TEXT("cross"), 90.0f}};
    for (const TPair<const TCHAR*, FLaunchData>& Entry : Shots)
    {
        const FShotSimulationResult CalmShot = ShotSimulator::Simulate(Entry.Value, ESurface::Fairway, Profile, FAirConditions());
        FString Line = FString::Printf(TEXT("%s: calm carry %.1f yd, apex %.1f m"), Entry.Key, Yards(CalmShot.CarryM), CalmShot.ApexM);
        for (const TPair<const TCHAR*, float>& Wind : Winds)
        {
            const FShotSimulationResult U = ShotSimulator::Simulate(Entry.Value, ESurface::Fairway, Profile, Air(10.0f, Wind.Value, false));
            const FShotSimulationResult P = ShotSimulator::Simulate(Entry.Value, ESurface::Fairway, Profile, Air(10.0f, Wind.Value, true));
            Line += FString::Printf(TEXT("; %s carry %.1f -> %.1f yd, lateral %.1f -> %.1f yd"), Wind.Key, Yards(U.CarryM), Yards(P.CarryM),
                Yards(U.LateralM), Yards(P.LateralM));
        }
        AddInfo(Line);
    }
    return true;
}
#endif
