// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenGolfFlagCloth.h"
#include "OpenGolfFlagstickComponent.h"

#include <limits>

namespace
{
    constexpr float MpsPerMph = 0.44704f;
    const FVector PoleTop(0.0, 0.0, UOpenGolfFlagstickComponent::PoleHeightCm);

    struct FFlagRun
    {
        /** Mean angle of the middle row below horizontal over the last three seconds. */
        float MeanDroopDeg = 0.0f;
        /** Unit horizontal direction from the hoist to the free edge at the end. */
        FVector2D Heading = FVector2D::ZeroVector;
        /** How far the free edge moves across the flag over the last three seconds (standard deviation). */
        float FlapCm = 0.0f;
        float MaxStretch = 0.0f;
        FVector TopFlyCorner = FVector::ZeroVector;
        bool bFinite = true;
    };

    /** Flies a flag that starts along +Y in a wind from WindFromDeg (0 is from +X, 90 from +Y). */
    FFlagRun Fly(float Mph, float WindFromDeg, float Seconds, float FrameSeconds)
    {
        FOpenGolfFlagCloth Cloth;
        Cloth.Reset(FOpenGolfFlagCloth::FSettings(), PoleTop, 90.0f);
        const float From = FMath::DegreesToRadians(WindFromDeg);
        Cloth.SetWind(-Mph * MpsPerMph * FVector(FMath::Cos(From), FMath::Sin(From), 0.0), 1.2f);

        FFlagRun Run;
        TArray<FVector> FreeEdge;
        float DroopSum = 0.0f;
        int32 Samples = 0;
        for (float Time = 0.0f; Time < Seconds; Time += FrameSeconds)
        {
            Cloth.Advance(FrameSeconds, PoleTop);
            if (Time > 2.0f)
                Run.MaxStretch = FMath::Max(Run.MaxStretch, Cloth.GetMaxStretch());
            if (Time > Seconds - 3.0f)
            {
                const FVector Span = Cloth.GetPosition(FOpenGolfFlagCloth::Columns - 1, FOpenGolfFlagCloth::Rows / 2)
                    - Cloth.GetPosition(0, FOpenGolfFlagCloth::Rows / 2);
                DroopSum += FMath::RadiansToDegrees(FMath::Atan2(-Span.Z, Span.Size2D()));
                ++Samples;
                FreeEdge.Add(Cloth.GetPosition(FOpenGolfFlagCloth::Columns - 1, FOpenGolfFlagCloth::Rows / 2));
            }
        }
        Run.MeanDroopDeg = DroopSum / FMath::Max(Samples, 1);
        const FVector Span = Cloth.GetPosition(FOpenGolfFlagCloth::Columns - 1, FOpenGolfFlagCloth::Rows / 2) - Cloth.GetPosition(0, FOpenGolfFlagCloth::Rows / 2);
        Run.Heading = FVector2D(Span.X, Span.Y).GetSafeNormal();
        FVector Mean = FVector::ZeroVector;
        for (const FVector& Point : FreeEdge)
            Mean += Point / FreeEdge.Num();
        double Variance = 0.0;
        for (const FVector& Point : FreeEdge)
            Variance += FVector::DistSquared(Point, Mean) / FreeEdge.Num();
        Run.FlapCm = static_cast<float>(FMath::Sqrt(Variance));
        Run.TopFlyCorner = Cloth.GetPosition(FOpenGolfFlagCloth::Columns - 1, 0);
        for (const FVector& Point : Cloth.GetPositions())
            Run.bFinite &= !Point.ContainsNaN();
        return Run;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfFlagClothTest, "UOpenGolf.Course.FlagCloth",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfFlagClothTest::RunTest(const FString& Parameters)
{
    constexpr float Frame = 1.0f / 60.0f;

    // Still air: gravity drops the flag, which hangs from the pole without stretching and comes to rest.
    const FFlagRun Calm = Fly(0.0f, 0.0f, 8.0f, Frame);
    TestTrue(FString::Printf(TEXT("Calm flag hangs (%.1f deg below horizontal)"), Calm.MeanDroopDeg), Calm.MeanDroopDeg > 60.0f);
    TestTrue(FString::Printf(TEXT("Calm free corner falls well below the pole top (%.1f cm)"), Calm.TopFlyCorner.Z - PoleTop.Z),
        Calm.TopFlyCorner.Z < PoleTop.Z - 30.0);
    TestTrue(TEXT("Calm flag stays by the pole"), Calm.TopFlyCorner.Size2D() < 20.0);
    TestTrue(FString::Printf(TEXT("Calm flag comes to rest (%.2f cm)"), Calm.FlapCm), Calm.FlapCm < 1.0f);
    TestTrue(FString::Printf(TEXT("Calm cloth does not stretch (%.1f%%)"), Calm.MaxStretch * 100.0f), Calm.MaxStretch < 0.03f);

    // 10 mph from the right (+Y): the sleeve turns, and the flag flies out toward -Y and flaps.
    const FFlagRun Breeze = Fly(10.0f, 90.0f, 12.0f, Frame);
    TestTrue(TEXT("10 mph stays finite"), Breeze.bFinite);
    TestTrue(FString::Printf(TEXT("10 mph flies downwind (heading %.2f, %.2f)"), Breeze.Heading.X, Breeze.Heading.Y), Breeze.Heading.Y < -0.7);
    TestTrue(FString::Printf(TEXT("10 mph flies out (%.1f deg below horizontal)"), Breeze.MeanDroopDeg),
        Breeze.MeanDroopDeg > 3.0f && Breeze.MeanDroopDeg < 30.0f);
    TestTrue(FString::Printf(TEXT("10 mph flaps (%.2f cm)"), Breeze.FlapCm), Breeze.FlapCm > 2.0f);
    TestTrue(FString::Printf(TEXT("10 mph cloth does not stretch (%.1f%%)"), Breeze.MaxStretch * 100.0f), Breeze.MaxStretch < 0.05f);

    // Lighter air lets it sag more; stronger air holds it straighter.
    const FFlagRun Light = Fly(5.0f, 90.0f, 12.0f, Frame);
    const FFlagRun Strong = Fly(20.0f, 90.0f, 12.0f, Frame);
    TestTrue(FString::Printf(TEXT("5 mph sags more than 10 mph (%.1f vs %.1f deg)"), Light.MeanDroopDeg, Breeze.MeanDroopDeg),
        Light.MeanDroopDeg > Breeze.MeanDroopDeg);
    TestTrue(FString::Printf(TEXT("20 mph flies higher than 10 mph (%.1f vs %.1f deg)"), Strong.MeanDroopDeg, Breeze.MeanDroopDeg),
        Strong.MeanDroopDeg < Breeze.MeanDroopDeg);
    TestTrue(FString::Printf(TEXT("20 mph cloth does not stretch (%.1f%%)"), Strong.MaxStretch * 100.0f), Strong.MaxStretch < 0.06f);

    // Fixed substeps: the pose does not depend on the frame rate.
    const FFlagRun Slow = Fly(10.0f, 90.0f, 12.0f, 1.0f / 30.0f);
    const FFlagRun Fast = Fly(10.0f, 90.0f, 12.0f, 1.0f / 144.0f);
    TestEqual(TEXT("30 and 144 fps fly at the same angle"), Slow.MeanDroopDeg, Fast.MeanDroopDeg, 2.0f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfFlagClothInputsTest, "UOpenGolf.Course.FlagClothInputs",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfFlagClothInputsTest::RunTest(const FString& Parameters)
{
    constexpr float Frame = 1.0f / 60.0f;
    auto AllFinite = [](const FOpenGolfFlagCloth& Cloth)
    {
        for (const FVector& Point : Cloth.GetPositions())
            if (Point.ContainsNaN())
                return false;
        return true;
    };

    // A non-finite wind or air density from a bad setting reads as calm air instead of poisoning the cloth.
    FOpenGolfFlagCloth Cloth;
    Cloth.Reset(FOpenGolfFlagCloth::FSettings(), PoleTop, 90.0f);
    constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
    Cloth.SetWind(FVector(NaN, 0.0, 0.0), NaN);
    for (int32 Step = 0; Step < 60; ++Step)
        Cloth.Advance(Frame, PoleTop);
    TestTrue(TEXT("Non-finite wind keeps the cloth finite"), AllFinite(Cloth));
    TestTrue(TEXT("Non-finite wind is calm"), Cloth.GetWindMps().IsZero());
    Cloth.SetWind(FVector(0.0, -4.47, 0.0), 1.2f);
    for (int32 Step = 0; Step < 60; ++Step)
        Cloth.Advance(Frame, PoleTop);
    TestTrue(TEXT("Cloth flies again after a valid wind"), AllFinite(Cloth));

    // A pole that jumps (placed or aligned to the ground) carries its flag instead of whipping it across the gap.
    // A second flag that never moves is the reference: relative to its pole, the jumped flag must match it.
    constexpr int32 FreeColumn = FOpenGolfFlagCloth::Columns - 1;
    FOpenGolfFlagCloth Still;
    Still.Reset(FOpenGolfFlagCloth::FSettings(), PoleTop, 90.0f);
    Still.Settle(2.0f, PoleTop);
    Cloth.Reset(FOpenGolfFlagCloth::FSettings(), PoleTop, 90.0f);
    Cloth.SetWind(FVector::ZeroVector, 1.225f); // Reset keeps the wind set above.
    Cloth.Settle(2.0f, PoleTop);
    const FVector Before = Cloth.GetPosition(FreeColumn, 0) - PoleTop;
    const FVector Jumped = PoleTop + FVector(1000.0, 0.0, 0.0);
    Cloth.Advance(Frame, Jumped);
    Still.Advance(Frame, PoleTop);
    const FVector After = Cloth.GetPosition(FreeColumn, 0) - Jumped;
    TestTrue(FString::Printf(TEXT("Free corner keeps its place on the pole (moved %.1f cm)"), FVector::Dist(Before, After)), FVector::Dist(Before, After) < 5.0);
    for (int32 Step = 0; Step < 60; ++Step)
    {
        Cloth.Advance(Frame, Jumped);
        Still.Advance(Frame, PoleTop);
    }
    TestTrue(TEXT("Cloth stays finite after the jump"), AllFinite(Cloth));
    const FVector Moved = Cloth.GetPosition(FreeColumn, 0) - Jumped;
    const FVector Reference = Still.GetPosition(FreeColumn, 0) - PoleTop;
    TestTrue(FString::Printf(TEXT("Moved flag hangs like an unmoved one (%.1f cm apart; corner %.1f cm vs %.1f cm below the top)"),
                 FVector::Dist(Moved, Reference), -Moved.Z, -Reference.Z),
        FVector::Dist(Moved, Reference) < 2.0);
    return true;
}

#endif
