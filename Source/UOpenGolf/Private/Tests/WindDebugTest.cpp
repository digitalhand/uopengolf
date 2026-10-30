// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenFairway/ShotSimulator.h"
#include "UI/WindDebug.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWindDebugTest, "UOpenGolf.Range.WindDebug",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWindDebugTest::RunTest(const FString& Parameters)
{
    WindDebug::FInput Input;
    Input.WindMps = FVector(0.0, -10.0 * OpenFairway::MpsPerMph, 0.0);
    Input.bHasFlag = true;
    Input.FlagDroopDeg = 20.0f;
    Input.FlagFlapHz = 3.4f;
    const TArray<FString> Breeze = WindDebug::Lines(Input);
    if (!TestEqual(TEXT("Wind, land, heights and flag lines"), Breeze.Num(), 4))
        return false;
    TestTrue(TEXT("10 mph is a gentle breeze"), Breeze[0].Contains(TEXT("10.0 mph")) && Breeze[0].Contains(TEXT("Beaufort 3 Gentle breeze")));
    TestTrue(TEXT("The flag height gets about 72% of the wind"), Breeze[2].Contains(TEXT("flag 2.0 m 7.2 mph")));
    TestTrue(TEXT("A flag flying 20 degrees down at force 3 is in band"), Breeze[3].Contains(TEXT("Extended")) && Breeze[3].Contains(TEXT("in band")));

    Input.FlagDroopDeg = 80.0f;
    TestTrue(TEXT("A hanging flag in a gentle breeze is out of band"), WindDebug::Lines(Input)[3].Contains(TEXT("OUT OF BAND")));

    // The scale reads the 10 m wind: 10 mph measured at 2 m is about 13.8 mph at 10 m, a moderate breeze.
    Input.ReferenceHeightM = 2.0f;
    const TArray<FString> LowReading = WindDebug::Lines(Input);
    TestTrue(TEXT("A low reading is moved to 10 m for the scale"), LowReading[0].Contains(TEXT("13.8 mph")) && LowReading[0].Contains(TEXT("Beaufort 4")));
    TestTrue(TEXT("The flag height is read through the same profile"), LowReading[2].Contains(TEXT("flag 2.0 m 9.9 mph")));
    Input.ReferenceHeightM = OpenFairway::WindProfile::StandardReferenceHeightM;

    Input.bHasFlag = false;
    Input.WindMps = FVector::ZeroVector;
    const TArray<FString> Calm = WindDebug::Lines(Input);
    TestEqual(TEXT("No flag line without a flag"), Calm.Num(), 3);
    TestTrue(TEXT("Zero wind is calm"), Calm[0].Contains(TEXT("Beaufort 0 Calm")));
    return true;
}

#endif
