// SPDX-License-Identifier: MIT
// Hole scoring after ogolf tests/OGolf.Tests/Scoring, and the shared play rules.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Play/OpenGolfHoleRound.h"
#include "Play/OpenGolfPlayRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfHoleRoundTest, "UOpenGolf.Course.Play.HoleRound",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfHoleRoundTest::RunTest(const FString& Parameters)
{
    FOpenGolfHoleRound Round;
    Round.Start(4);
    TestEqual(TEXT("A new hole has no strokes"), Round.Strokes, 0);
    TestTrue(TEXT("A new hole has no result"), Round.ResultName().IsEmpty());
    Round.RecordStroke(false, false);
    Round.RecordStroke(false, false);
    Round.RecordStroke(true, false);
    TestEqual(TEXT("Every stopped shot is a stroke"), Round.Strokes, 3);
    TestEqual(TEXT("Shots from the green are putts"), Round.Putts, 1);
    Round.RecordStroke(true, true);
    TestTrue(TEXT("The hole ends when the ball drops"), Round.bHoled);
    TestEqual(TEXT("Par"), Round.ResultName(), FString(TEXT("Par")));
    Round.RecordStroke(true, false);
    TestEqual(TEXT("No strokes after holing out"), Round.Strokes, 4);

    Round.Start(4);
    Round.bPractice = true;
    Round.RecordStroke(false, true);
    TestEqual(TEXT("Practice reports the strokes, not a score"), Round.ResultName(), FString(TEXT("Holed in 1")));

    const TPair<int32, const TCHAR*> Names[] = {{1, TEXT("Hole in one")}, {2, TEXT("Eagle")}, {3, TEXT("Birdie")}, {4, TEXT("Par")}, {5, TEXT("Bogey")},
        {6, TEXT("Double bogey")}, {7, TEXT("Triple bogey")}, {9, TEXT("+5")}};
    for (const auto& [Strokes, Name] : Names)
        TestEqual(FString::Printf(TEXT("%d on a par 4"), Strokes), FOpenGolfHoleRound::ScoreName(Strokes, 4), FString(Name));
    TestEqual(TEXT("Albatross"), FOpenGolfHoleRound::ScoreName(2, 5), FString(TEXT("Albatross")));
    TestEqual(TEXT("An ace on a par 3 is a hole in one"), FOpenGolfHoleRound::ScoreName(1, 3), FString(TEXT("Hole in one")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfPlayRulesTest, "UOpenGolf.Course.Play.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfPlayRulesTest::RunTest(const FString& Parameters)
{
    using namespace OpenGolfPlay;
    TestEqual(TEXT("Downrange bears 0"), BearingDeg(FVector::ZeroVector, FVector(1000, 0, 50)), 0.0f, 1e-3f);
    TestEqual(TEXT("Right bears +90"), BearingDeg(FVector::ZeroVector, FVector(0, 1000, 0)), 90.0f, 1e-3f);
    TestEqual(TEXT("Left bears -90"), BearingDeg(FVector::ZeroVector, FVector(0, -1000, 0)), -90.0f, 1e-3f);
    TestEqual(TEXT("The same spot keeps the fallback"), BearingDeg(FVector::ZeroVector, FVector(0.5, 0, 0), 12.0f), 12.0f);

    TestTrue(TEXT("On the green is short game"), IsShortGame(80.0f, true));
    TestTrue(TEXT("Inside 30 yd is short game"), IsShortGame(20.0f, false));
    TestFalse(TEXT("An approach is not"), IsShortGame(120.0f, false));

    TestEqual(TEXT("No cup zoom beyond 8 ft"), CupZoomAmount(3.0f), 0.0f);
    TestEqual(TEXT("The cup zoom starts at 8 ft"), CupZoomAmount(CupZoomStartM), 0.0f, 1e-4f);
    TestTrue(TEXT("The cup zoom has begun at 6 ft"), CupZoomAmount(1.8288f) > 0.0f);
    TestEqual(TEXT("Full cup zoom at the cup"), CupZoomAmount(0.0f), 1.0f, 1e-4f);
    TestTrue(TEXT("The cup zoom closes in as the ball nears"), CupZoomAmount(0.4f) > CupZoomAmount(1.0f) && CupZoomAmount(1.0f) > 0.0f);

    const FVector Cup(36576, 0, 120);
    TestFalse(TEXT("No ball on the cup"), CanPlaceBallAt(Cup + FVector(5, 0, 0), Cup));
    TestTrue(TEXT("A ball beside the cup"), CanPlaceBallAt(Cup + FVector(20, 0, 0), Cup));

    TestEqual(TEXT("Feet on the green"), DistanceToPinLabel(3.048f, true), FString(TEXT("10 ft")));
    TestEqual(TEXT("Yards off it"), DistanceToPinLabel(137.16f, false), FString(TEXT("150 yd")));

    TestEqual(TEXT("A tap turns only its step"), AimTurnDegPerSecond(0.2f, false), 0.0f);
    TestEqual(TEXT("A hold starts slow"), AimTurnDegPerSecond(0.3f, false), 2.0f, 1e-3f);
    TestTrue(TEXT("A hold speeds up"), AimTurnDegPerSecond(0.8f, false) > AimTurnDegPerSecond(0.5f, false));
    TestEqual(TEXT("A long hold turns fast"), AimTurnDegPerSecond(1.3f, false), 20.0f, 1e-3f);
    TestEqual(TEXT("Fine aim stays slow"), AimTurnDegPerSecond(3.0f, true), 2.0f, 1e-3f);

    TestEqual(TEXT("Aim straight"), AimOffsetLabel(0.0f), FString(TEXT("Straight")));
    TestEqual(TEXT("A tiny offset is straight"), AimOffsetLabel(0.04f), FString(TEXT("Straight")));
    TestEqual(TEXT("Two taps read in tenths"), AimOffsetLabel(0.5f), FString(TEXT("0.5° R")));
    TestEqual(TEXT("Aim left in tenths"), AimOffsetLabel(-1.5f), FString(TEXT("1.5° L")));
    TestEqual(TEXT("Wide aim in whole degrees"), AimOffsetLabel(12.6f), FString(TEXT("13° R")));
    TestEqual(TEXT("At the limit"), AimOffsetLabel(45.0f), FString(TEXT("45° R")));
    TestEqual(TEXT("Yards wide of the target"), AimOffsetLabel(2.0f, 137.16f), FString(TEXT("2.0° R · 5 yd")));
    TestEqual(TEXT("Feet wide in the short game"), AimOffsetLabel(-1.0f, 6.4f, true), FString(TEXT("1.0° L · 0.4 ft")));
    return true;
}

#endif
