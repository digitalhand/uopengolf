// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "OpenGolfEnvironment.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentClockTest, "UOpenGolf.Environment.Clock", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnvironmentClockTest::RunTest(const FString& Parameters)
{
    FOpenGolfEnvironmentSettings State;
    State.Daylight = 0;
    State.Advance(1800);
    TestEqual(TEXT("Manual daylight holds"), State.Daylight, 0.f);
    State.bAutomaticTime = true;
    State.Advance(1800);
    TestEqual(TEXT("Half of a 60 minute day"), State.Daylight, .5f);
    State.Advance(7200);
    TestEqual(TEXT("Stops at sunset"), State.Daylight, 1.f);
    State.Advance(-10);
    TestEqual(TEXT("Negative elapsed time is ignored"), State.Daylight, 1.f);
    State.CloudCoverage = 9;
    State.CloudSpeed = -5;
    State.DurationMinutes = 0;
    State.Sanitize();
    TestEqual(TEXT("Coverage is bounded"), State.CloudCoverage, 1.f);
    TestEqual(TEXT("Speed cannot reverse"), State.CloudSpeed, 0.f);
    TestEqual(TEXT("Duration is positive"), State.DurationMinutes, 1.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEnvironmentActorTest, "UOpenGolf.Environment.Actor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEnvironmentActorTest::RunTest(const FString& Parameters)
{
    FTestWorldWrapper Wrapper;
    if (!Wrapper.CreateTestWorld(EWorldType::Game))
    {
        Wrapper.ForwardErrorMessages(this);
        return false;
    }
    UWorld* World = Wrapper.GetTestWorld();
    auto* Environment = World->SpawnActor<AOpenGolfEnvironment>();
    if (!TestNotNull(TEXT("Environment spawned"), Environment))
        return false;
    TestEqual(TEXT("Found without a range"), AOpenGolfEnvironment::Find(World), Environment);
    const auto* Clouds = Environment->FindComponentByClass<UVolumetricCloudComponent>();
    if (!TestNotNull(TEXT("Environment owns the cloud layer"), Clouds))
        return false;
    const auto* Material = Clouds->GetMaterial();
    if (TestNotNull(TEXT("The cloud material loads from its asset path"), Material))
        TestTrue(TEXT("The material is compiled for volumetric clouds"), Material->GetUsageByFlag(MATUSAGE_VolumetricCloud));
    // Unreal's default 156 m ray-march step makes the cumulus material shimmer (Docs/PhasesStatus.md).
    TestTrue(TEXT("Clouds ray-march at least four times the engine's default samples"), Clouds->ViewSampleCountScale >= 4.f);
    auto State = Environment->GetSettings();
    const float StartingCoverage = State.CloudCoverage;
    State.CloudCoverage = 0;
    Environment->ApplySettings(State);
    TestFalse(TEXT("Zero coverage clears the cloud layer"), Clouds->IsVisible());
    State.CloudCoverage = StartingCoverage;
    State.Daylight = .1f;
    State.CloudSpeed = 0;
    Environment->ApplySettings(State);
    TestTrue(TEXT("Restoring coverage shows the cloud layer"), Clouds->IsVisible());
    Environment->Tick(10);
    TestEqual(TEXT("Stationary clouds"), Environment->GetCloudOffset(), 0.f);
    State.CloudSpeed = 1;
    Environment->ApplySettings(State);
    Environment->Tick(10);
    TestTrue(TEXT("Clouds drift with a fixed sun"), Environment->GetCloudOffset() > 0);
    TestEqual(TEXT("Cloud motion does not advance sun"), Environment->GetSettings().Daylight, .1f);
    Environment->SelectPreset(EOpenGolfDaylightPreset::Sunset);
    TestEqual(TEXT("Sunset preset"), Environment->GetSettings().Daylight, 1.f);
    Environment->ResetToCourseDefaults();
    TestEqual(TEXT("Reset restores authored starting time"), Environment->GetSettings().Daylight, .5f);
    auto* Profile = NewObject<UOpenGolfEnvironmentProfile>(Environment);
    TestTrue(TEXT("Morning mist clears by midday"), Profile->FogDensity.GetRichCurveConst()->Eval(0) > Profile->FogDensity.GetRichCurveConst()->Eval(.5f));
    int32 Notifications = 0;
    Environment->OnSettingsChanged.AddLambda([&Notifications]
        { ++Notifications; });
    Environment->ApplySettings(State, false);
    TestEqual(TEXT("Dragging waits to notify expensive listeners"), Notifications, 0);
    Environment->ApplySettings(State);
    TestEqual(TEXT("Committing notifies listeners once"), Notifications, 1);
    Environment->Tick(1);
    TestEqual(TEXT("Animation does not request minimap captures"), Notifications, 1);
    Environment->Profile = Profile;
    Profile->StartingSettings.Daylight = .2f;
    Environment->ResetToCourseDefaults();
    TestEqual(TEXT("Course profile supplies defaults"), Environment->GetSettings().Daylight, .2f);
    Environment->bOverrideStartingSettings = true;
    Environment->StartingSettings.Daylight = .8f;
    Environment->ResetToCourseDefaults();
    TestEqual(TEXT("Map override supplies defaults"), Environment->GetSettings().Daylight, .8f);
    Environment->SelectPreset(EOpenGolfDaylightPreset::Morning);
    TestEqual(TEXT("Runtime changes leave map defaults alone"), Environment->StartingSettings.Daylight, .8f);
    TestEqual(TEXT("Runtime changes leave shared profile alone"), Profile->StartingSettings.Daylight, .2f);
    Environment->bOverrideStartingSettings = false;
    Environment->ResetToCourseDefaults();
    State = Environment->GetSettings();
    State.bAutomaticTime = true;
    State.DurationMinutes = 1;
    Environment->ApplySettings(State);
    Environment->Tick(60);
    TestEqual(TEXT("Actor reaches sunset"), Environment->GetSettings().Daylight, 1.f);
    Environment->Tick(60);
    TestEqual(TEXT("Actor holds sunset"), Environment->GetSettings().Daylight, 1.f);
    return true;
}
#endif
