// SPDX-License-Identifier: MIT
// Space while the ball rolls: holding it plays the roll fast while held, a tap skips to where the ball stops.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Play/OpenGolfFastForward.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfFastForwardTest, "UOpenGolf.Course.Play.FastForward",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfFastForwardTest::RunTest(const FString& Parameters)
{
    using namespace OpenGolfPlay;
    FOpenGolfFastForward Input;
    TestEqual(TEXT("Normal speed before Space"), Input.Rate(true), 1.0f);

    // Hold: fast from the press, while the ball rolls, back to normal on release.
    Input.Press(10.0, true);
    TestEqual(TEXT("Holding Space plays the roll fast"), Input.Rate(true), FastForwardHoldRate);
    TestEqual(TEXT("Nothing speeds up while the ball flies"), Input.Rate(false), 1.0f);
    Input.Release(11.0);
    TestEqual(TEXT("Releasing a hold returns to normal speed"), Input.Rate(true), 1.0f);
    TestFalse(TEXT("A long hold is not a tap"), Input.IsSkipping());

    // Tap: skips to rest, and stays skipping until the shot ends.
    Input.Press(20.0, true);
    Input.Release(20.0 + FastForwardTapSeconds);
    TestTrue(TEXT("A quick press and release is a tap"), Input.IsSkipping());
    TestEqual(TEXT("A tap skips at the skip rate"), Input.Rate(true), FastForwardSkipRate);
    Input.Press(20.5, true);
    Input.Release(21.5);
    TestEqual(TEXT("Pressing again while skipping keeps skipping"), Input.Rate(true), FastForwardSkipRate);
    Input.Reset();
    TestEqual(TEXT("The shot's end returns to normal speed"), Input.Rate(true), 1.0f);
    TestFalse(TEXT("The shot's end clears the skip"), Input.IsSkipping());

    // Presses during the flight.
    Input.Press(30.0, false);
    Input.Release(30.1);
    TestEqual(TEXT("A tap in the air does not skip the roll"), Input.Rate(true), 1.0f);
    Input.Press(31.0, false);
    TestEqual(TEXT("Held from the air, nothing happens in flight"), Input.Rate(false), 1.0f);
    TestEqual(TEXT("Held through the landing, the roll plays fast"), Input.Rate(true), FastForwardHoldRate);
    Input.Release(31.1);
    TestEqual(TEXT("A quick press from the air is still no tap"), Input.Rate(true), 1.0f);

    // A reset while Space is down needs a new press.
    Input.Press(40.0, true);
    Input.Reset();
    TestEqual(TEXT("A reset ends the hold"), Input.Rate(true), 1.0f);
    Input.Release(40.1);
    TestFalse(TEXT("The release after a reset is not a tap"), Input.IsSkipping());

    FOpenGolfFastForward Fresh;
    Fresh.Release(1.0);
    TestEqual(TEXT("A release without a press does nothing"), Fresh.Rate(true), 1.0f);
    TestTrue(TEXT("The skip rate is the fastest"), FastForwardSkipRate > FastForwardHoldRate && FastForwardHoldRate > 1.0f);
    return true;
}

#endif
