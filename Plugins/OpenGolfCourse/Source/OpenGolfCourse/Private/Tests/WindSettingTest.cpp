// SPDX-License-Identifier: MIT
// The wind the player sets on the HUD: on and off, speed and direction steps, wind taken from the map or console, and labels.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Play/OpenGolfWindSetting.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfWindSettingTest, "UOpenGolf.Course.Play.Wind",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfWindSettingTest::RunTest(const FString& Parameters)
{
    using namespace OpenGolfPlay;
    FOpenGolfWindSetting Wind;
    TestFalse(TEXT("The wind starts off"), Wind.bOn);
    TestEqual(TEXT("Off is calm"), Wind.AppliedMph(), 0.0f);
    Wind.bOn = true;
    TestEqual(TEXT("On blows at the default speed"), Wind.AppliedMph(), DefaultWindSettingMph);

    Wind.SetSpeedMph(-5.0f);
    TestEqual(TEXT("Speed stops at calm"), Wind.SpeedMph, 0.0f);
    Wind.SetSpeedMph(45.0f);
    TestEqual(TEXT("Speed stops at the HUD's maximum"), Wind.SpeedMph, MaxWindSettingMph);
    Wind.SetSpeedMph(12.4f);
    TestEqual(TEXT("Speed is whole mph"), Wind.SpeedMph, 12.0f);

    Wind.SetFromDeg(93.0f);
    TestEqual(TEXT("Direction snaps to its step"), Wind.FromDeg, 95.0f);
    Wind.SetFromDeg(-90.0f);
    TestEqual(TEXT("Direction wraps below zero"), Wind.FromDeg, 270.0f);
    Wind.SetFromDeg(359.0f);
    TestEqual(TEXT("Direction wraps past a full turn"), Wind.FromDeg, 0.0f);

    Wind.Adopt(0.0f, 90.0f);
    TestFalse(TEXT("Calm wind from the map is off"), Wind.bOn);
    TestEqual(TEXT("Off keeps the last speed"), Wind.SpeedMph, 12.0f);
    TestEqual(TEXT("The direction is taken while calm"), Wind.FromDeg, 90.0f);
    Wind.Adopt(15.0f, 180.0f);
    TestTrue(TEXT("Blowing wind from the console is on"), Wind.bOn && Wind.AppliedMph() == 15.0f && Wind.FromDeg == 180.0f);
    Wind.Adopt(35.0f, -45.0f);
    TestEqual(TEXT("A console speed over the HUD's maximum is kept"), Wind.AppliedMph(), 35.0f);
    TestEqual(TEXT("An adopted direction wraps"), Wind.FromDeg, 315.0f);

    const TPair<float, const TCHAR*> Labels[] = {{0.0f, TEXT("Headwind")}, {22.4f, TEXT("Headwind")}, {22.5f, TEXT("Into · right to left")},
        {45.0f, TEXT("Into · right to left")}, {90.0f, TEXT("Right to left")}, {135.0f, TEXT("Helping · right to left")}, {180.0f, TEXT("Tailwind")},
        {225.0f, TEXT("Helping · left to right")}, {270.0f, TEXT("Left to right")}, {315.0f, TEXT("Into · left to right")}, {340.0f, TEXT("Headwind")},
        {-90.0f, TEXT("Left to right")}};
    for (const auto& [FromDeg, Label] : Labels)
        TestEqual(FString::Printf(TEXT("Wind from %.1f°"), FromDeg), WindDirectionLabel(FromDeg), FString(Label));
    return true;
}

#endif
