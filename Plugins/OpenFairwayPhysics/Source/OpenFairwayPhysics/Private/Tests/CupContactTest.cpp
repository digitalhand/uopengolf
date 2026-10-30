// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "OpenFairway/CupCapture.h"
#include "OpenFairway/GolfPhysics.h"

using namespace OpenFairway;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCupContactTest, "UOpenGolf.Physics.CupContact",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCupContactTest::RunTest(const FString& Parameters)
{
    using namespace CupCapture;
    struct FRun
    {
        FBallState State;
        ECupCrossing Outcome = ECupCrossing::OverCup;
        float MaxEnergyGain = 0;
        float Time = 0;
        float Unused = 0;
    };
    auto Run = [this](float Speed, float Offset, const FVector3f& Gravity, float Frame, float Substep)
    {
        FRun Run;
        Run.State = {FVector3f(-FMath::Sqrt(FMath::Square(CupRadiusM) - Offset * Offset), GolfPhysics::Radius, Offset),
            FVector3f(Speed, 0, 0), FVector3f(0, 0, -Speed / GolfPhysics::Radius)};
        auto Energy = [&Gravity](const FBallState& State)
        {
            return 0.5f * GolfPhysics::Mass * State.Velocity.SizeSquared() + 0.5f * GolfPhysics::Inertia * State.Spin.SizeSquared()
                - GolfPhysics::Mass * FVector3f::DotProduct(Gravity, State.Position);
        };
        const float Initial = Energy(Run.State);
        for (int32 Index = 0; Index < 2400 && Run.Outcome == ECupCrossing::OverCup; ++Index)
        {
            const auto Step = AdvanceCrossing(Run.State, FVector3f::ZeroVector, Gravity, Frame, Substep);
            TestTrue(TEXT("The solver accounts for a positive, bounded duration"), Step.ConsumedSeconds > 0 && Step.ConsumedSeconds <= Frame);
            Run.Time += Step.ConsumedSeconds;
            Run.Unused = Frame - Step.ConsumedSeconds;
            Run.Outcome = Step.Outcome;
            Run.MaxEnergyGain = FMath::Max(Run.MaxEnergyGain, Energy(Run.State) - Initial);
        }
        return Run;
    };
    for (const FVector3f Gravity : {FVector3f(0, -9.81f, 0), FVector3f(0.3f, -9.8f, 0.2f)})
    {
        for (float Speed : {1.68f, 2.49f, 2.72f, 3.11f, 4.0f})
        {
            for (float Offset : {0.0f, 0.02f, 0.048f})
            {
                const FRun Normal = Run(Speed, Offset, Gravity, 1.0f / 120, CrossingSubstepS);
                const FRun Fine = Run(Speed, Offset, Gravity, 1.0f / 240, CrossingSubstepS / 2);
                const FString Label = FString::Printf(TEXT("%.2f m/s, %.3f m offset, gravity %s"), Speed, Offset, *Gravity.ToString());
                TestTrue(Label + TEXT(" ends"), Normal.Outcome != ECupCrossing::OverCup);
                TestTrue(Label + TEXT(" conserves or dissipates energy"), Normal.MaxEnergyGain < 1e-5f);
                TestTrue(Label + TEXT(" outcome converges"), Normal.Outcome == Fine.Outcome);
                TestTrue(Label + TEXT(" exit velocity converges"), Normal.State.Velocity.Equals(Fine.State.Velocity, 0.015f));
                TestTrue(Label + TEXT(" elapsed time converges"), FMath::Abs(Normal.Time - Fine.Time) < 0.001f);
                if (Normal.Outcome == ECupCrossing::Out)
                {
                    TestTrue(Label + TEXT(" exits above the surface"), Normal.State.Position.Y >= GolfPhysics::Radius - 1e-6f);
                    TestTrue(Label + TEXT(" returns unconsumed time"), Normal.Unused >= 0);
                }
            }
        }
    }
    const FRun Center = Run(2.49f, 0, FVector3f(0, -9.81f, 0), 1.0f / 120, CrossingSubstepS);
    const float Rise = Center.State.Position.Y - GolfPhysics::Radius + FMath::Square(FMath::Max(Center.State.Velocity.Y, 0.0f)) / (2 * GolfPhysics::Gravity);
    AddInfo(FString::Printf(TEXT("Centered 2.49 m/s crossing: exit %s m/s, predicted rise %.2f cm, maximum energy gain %.8f J."),
        *Center.State.Velocity.ToString(), Rise * 100, Center.MaxEnergyGain));
    return true;
}
#endif
