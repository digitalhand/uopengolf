// SPDX-License-Identifier: MIT
// Cup capture, after ogolf tests/OGolf.Tests/CupCaptureTests.cs.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/CupCapture.h"
#include "OpenFairway/GolfPhysics.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenFairwayCupCaptureTest, "UOpenGolf.Physics.CupCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenFairwayCupCaptureTest::RunTest(const FString& Parameters)
{
    using namespace CupCapture;
    constexpr float PoleRadius = 0.0125f;
    constexpr float PoleHeight = 2.13f;
    const FVector3f Cup(50.0f, 0.0f, -3.0f);
    auto Rolling = [&Cup](float OffsetX, float OffsetZ) { return Cup + FVector3f(OffsetX, GolfPhysics::Radius, OffsetZ); };

    TestEqual(TEXT("Regulation cup is 4.25 in across"), CupDiameterM / 0.0254f, 4.25f, 0.001f);
    TestTrue(TEXT("Regulation cup is at least 4 in deep"), CupDepthM >= 4.0f * 0.0254f - 1e-5f);

    const float Center = MaxCaptureSpeed(0.0f);
    TestEqual(TEXT("Dead-centre capture speed matches the Holmes limit"), Center, 1.64f, 0.05f);
    const float Half = MaxCaptureSpeed(CupRadiusM * 0.5f);
    const float NearRim = MaxCaptureSpeed(CupRadiusM * 0.95f);
    TestTrue(TEXT("Capture speed falls with offset"), Half < Center && NearRim < Half);
    TestEqual(TEXT("No capture at the rim"), MaxCaptureSpeed(CupRadiusM), 0.0f);
    TestEqual(TEXT("No capture past the rim"), MaxCaptureSpeed(CupRadiusM * 2.0f), 0.0f);

    TestTrue(TEXT("A slow ball through the centre drops"), CapturesRolling(Rolling(-0.02f, 0.0f), FVector3f(1.2f, 0.0f, 0.0f), Cup));
    TestFalse(TEXT("A fast ball through the centre lips over"), CapturesRolling(Rolling(-0.02f, 0.0f), FVector3f(2.5f, 0.0f, 0.0f), Cup));
    TestFalse(TEXT("Near the rim, a firm ball lips out"), CapturesRolling(Rolling(0.0f, 0.05f), FVector3f(1.2f, 0.0f, 0.0f), Cup));
    TestTrue(TEXT("Near the rim, a dying ball drops"), CapturesRolling(Rolling(0.0f, 0.05f), FVector3f(0.1f, 0.0f, 0.0f), Cup));
    TestFalse(TEXT("A stopped ball outside the cup is not holed"), CapturesRolling(Rolling(0.0f, CupRadiusM + 0.01f), FVector3f::ZeroVector, Cup));
    TestFalse(TEXT("A rolling ball outside the cup is not holed"), CapturesRolling(Rolling(0.0f, CupRadiusM + 0.01f), FVector3f(0.2f, 0.0f, 0.0f), Cup));
    TestTrue(TEXT("A stopped ball over the cup is holed"), CapturesRolling(Rolling(0.03f, 0.0f), FVector3f::ZeroVector, Cup));
    TestTrue(TEXT("A ball leaning on the pin is holed"), CapturesRolling(Rolling(-(PoleRadius + GolfPhysics::Radius), 0.0f), FVector3f::ZeroVector, Cup));
    TestTrue(TEXT("Vertical velocity does not count"), CapturesRolling(Rolling(-0.02f, 0.0f), FVector3f(1.0f, -3.0f, 0.0f), Cup));
    TestFalse(TEXT("A ball perched on the pin is not holed"),
        CapturesRolling(Cup + FVector3f(0.01f, PoleHeight + GolfPhysics::Radius, 0.0f), FVector3f::ZeroVector, Cup));

    {
        float Speed = 0.0f, MaxSpeed = 0.0f;
        TestTrue(TEXT("A firm putt through the centre crosses too fast"), CrossesTooFast(Rolling(-0.02f, 0.0f), FVector3f(2.7f, 0.0f, 0.0f), Cup, Speed, MaxSpeed));
        TestEqual(TEXT("It reports its speed"), Speed, 2.7f, 1e-4f);
        TestTrue(TEXT("It reports the speed that drops"), MaxSpeed > 1.5f && MaxSpeed < Speed);
        TestFalse(TEXT("A dying putt does not cross too fast"), CrossesTooFast(Rolling(-0.02f, 0.0f), FVector3f(1.0f, 0.0f, 0.0f), Cup, Speed, MaxSpeed));
        TestFalse(TEXT("Beside the cup is not over it"), CrossesTooFast(Rolling(0.0f, CupRadiusM + 0.01f), FVector3f(2.7f, 0.0f, 0.0f), Cup, Speed, MaxSpeed));
    }
    TestTrue(TEXT("A landing inside the cup dunks"), CapturesLanding(Cup + FVector3f(0.03f, 0.0f, 0.02f), Cup));
    TestFalse(TEXT("A landing just outside bounces"), CapturesLanding(Cup + FVector3f(CupRadiusM + 0.005f, 0.0f, 0.0f), Cup));
    TestFalse(TEXT("Clipping the top of the pin is not a dunk"), CapturesLanding(Cup + FVector3f(0.0f, PoleHeight, 0.0f), Cup));

    for (const FVector2f Offset : {FVector2f(-0.04f, 0.0f), FVector2f(0.0f, 0.0f), FVector2f(0.01f, 0.03f)})
    {
        const FVector3f Drop = DropPoint(Rolling(Offset.X, Offset.Y), Cup, PoleRadius);
        const float Radial = FVector2f(Drop.X - Cup.X, Drop.Z - Cup.Z).Size();
        const FString Label = Offset.ToString();
        TestTrue(Label + TEXT(" drop clears the pin"), Radial >= PoleRadius + GolfPhysics::Radius - 1e-4f);
        TestTrue(Label + TEXT(" drop stays over the cup"), Radial <= CupRadiusM);
        TestTrue(Label + TEXT(" drop sits below the surface"), Drop.Y + GolfPhysics::Radius < Cup.Y);
        TestTrue(Label + TEXT(" drop stays inside the cup's depth"), Drop.Y - GolfPhysics::Radius > Cup.Y - CupDepthM);
    }
    TestTrue(TEXT("Drop keeps the entry side"), DropPoint(Rolling(-0.04f, 0.0f), Cup, PoleRadius).X < Cup.X);

    TestEqual(TEXT("A crossing starts where its line met the rim"),
        TimeSinceRimEntry(Rolling(-(CupRadiusM - 0.02f), 0.0f), FVector3f(2.0f, 0.0f, 0.0f), Cup), 0.01f, 1e-4f);
    TestEqual(TEXT("No crossing beside the cup"), TimeSinceRimEntry(Rolling(0.0f, CupRadiusM + 0.01f), FVector3f(2.0f, 0.0f, 0.0f), Cup), 0.0f);

    // Crossings from the rim, rolling along +X OffsetZ from the centre, stepped at the ball's 120 Hz until they end.
    struct FCrossing
    {
        ECupCrossing End = ECupCrossing::OverCup;
        FBallState State;
        float LowestY = 0.0f;
    };
    auto Cross = [&Cup](float Speed, float OffsetZ)
    {
        FCrossing Crossing;
        Crossing.State.Position = Cup + FVector3f(-FMath::Sqrt(CupRadiusM * CupRadiusM - OffsetZ * OffsetZ), GolfPhysics::Radius, OffsetZ);
        Crossing.State.Velocity = FVector3f(Speed, 0.0f, 0.0f);
        // Rolling, its contact point at rest.
        Crossing.State.Spin = FVector3f(0.0f, 0.0f, -Speed / GolfPhysics::Radius);
        Crossing.LowestY = Crossing.State.Position.Y;
        for (float Elapsed = 0.0f; Crossing.End == ECupCrossing::OverCup && Elapsed < 1.0f; Elapsed += GolfPhysics::SimulationDt)
        {
            Crossing.End = StepOverCup(Crossing.State, Cup, GolfPhysics::SimulationDt);
            Crossing.LowestY = FMath::Min(Crossing.LowestY, Crossing.State.Position.Y);
        }
        return Crossing;
    };
    const float RollingY = Cup.Y + GolfPhysics::Radius;
    auto Speed = [](const FCrossing& Crossing) { return FVector2f(Crossing.State.Velocity.X, Crossing.State.Velocity.Z).Size(); };
    auto TurnDeg = [](const FCrossing& Crossing) { return FMath::RadiansToDegrees(FMath::Atan2(Crossing.State.Velocity.Z, Crossing.State.Velocity.X)); };
    // Height above rolling, with the climb still to come once it has left the cup.
    auto Hop = [RollingY](const FCrossing& Crossing)
    {
        return Crossing.State.Position.Y - RollingY + FMath::Square(FMath::Max(Crossing.State.Velocity.Y, 0.0f)) / (2.0f * GolfPhysics::Gravity);
    };

    {
        // 21 ft at 8.6 mph on a stimp-10 green reaches the cup at about 2.72 m/s, well over the 1.64 m/s that drops.
        const FCrossing Firm = Cross(2.72f, 0.0f);
        TestTrue(TEXT("A firm putt through the centre lips out"), Firm.End == ECupCrossing::Out);
        TestTrue(TEXT("It dips into the cup"), Firm.LowestY < RollingY - 0.002f);
        TestTrue(TEXT("The lip takes much of its speed"), Speed(Firm) < 0.6f * 2.72f);
        TestTrue(TEXT("It carries on forward"), Firm.State.Velocity.X > 0.5f);
        TestTrue(TEXT("The departure height is finite"), FMath::IsFinite(Hop(Firm)));
        TestTrue(TEXT("It leaves on the green, not inside it"), Firm.State.Position.Y >= RollingY - 1e-4f);
        AddInfo(FString::Printf(TEXT("2.72 m/s through the centre: dips %.1f mm, leaves at %.2f m/s, hops %.0f mm."), (RollingY - Firm.LowestY) * 1000.0f,
            Speed(Firm), Hop(Firm) * 1000.0f));
    }
    {
        const FCrossing OffCentre = Cross(2.72f, 0.02f);
        TestTrue(TEXT("Off centre it lips out"), OffCentre.End == ECupCrossing::Out);
        TestTrue(TEXT("Off centre the lip slows it"), Speed(OffCentre) < 2.72f);
        TestTrue(TEXT("The rim turns it across the cup"), TurnDeg(OffCentre) < -5.0f);
        AddInfo(FString::Printf(TEXT("2.72 m/s, 2 cm off centre: leaves at %.2f m/s, turned %.1f deg."), Speed(OffCentre), TurnDeg(OffCentre)));
    }
    {
        const FCrossing Edge = Cross(2.7f, 0.048f);
        TestTrue(TEXT("Catching the edge lips out"), Edge.End == ECupCrossing::Out);
        TestTrue(TEXT("Catching the edge barely slows it"), Speed(Edge) > 0.95f * 2.7f);
        TestTrue(TEXT("Catching the edge barely turns it"), FMath::Abs(TurnDeg(Edge)) < 3.0f);
    }
    TestTrue(TEXT("The rattle-in speed is over the drop limit"), 1.68f > MaxCaptureSpeed(0.0f));
    TestTrue(TEXT("Just too fast through the centre, it hits the back of the cup and drops"), Cross(1.68f, 0.0f).End == ECupCrossing::Holed);
    {
        const FCrossing Hard = Cross(4.0f, 0.0f);
        TestTrue(TEXT("A hard putt lips out"), Hard.End == ECupCrossing::Out);
        TestTrue(TEXT("A hard putt keeps most of its speed"), Speed(Hard) > 0.6f * 4.0f);
    }
    return true;
}

#endif
