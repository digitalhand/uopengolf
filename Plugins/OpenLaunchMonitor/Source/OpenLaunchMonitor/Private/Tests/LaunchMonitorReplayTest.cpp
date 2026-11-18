// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UObject/Package.h"

#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchMonitorReplayTest, "UOpenGolf.LaunchMonitor.Replay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaunchMonitorReplayTest::RunTest(const FString& Parameters)
{
    // Replay needs no world or connection; an uninitialized game instance is a valid outer.
    UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
    UOpenLaunchMonitorSubsystem* Subsystem = NewObject<UOpenLaunchMonitorSubsystem>(GameInstance);

    TArray<FLaunchMonitorShot> Shots;
    TArray<FLaunchMonitorClubData> ClubData;
    Subsystem->OnShotReceivedNative.AddLambda([&Shots](const FLaunchMonitorShot& Shot)
        { Shots.Add(Shot); });
    Subsystem->OnClubDataReceivedNative.AddLambda([&ClubData](const FLaunchMonitorClubData& Club)
        { ClubData.Add(Club); });

    FLaunchMonitorShot LastShot;
    TestFalse(TEXT("No shot before replay"), Subsystem->GetLastShot(LastShot));

    // The first built-in replay is the reference wood shot.
    TestTrue(TEXT("Replay shot delivered"), Subsystem->InjectReplayShot());
    if (TestEqual(TEXT("One shot event"), Shots.Num(), 1))
    {
        const FLaunchMonitorShot& Shot = Shots[0];
        TestEqual(TEXT("Shot number"), Shot.ShotNumber, 1);
        TestEqual(TEXT("Source"), Shot.Source, FString(TEXT("Replay")));
        TestEqual(TEXT("Speed mph"), Shot.GetBallSpeedMph(), 114.46f, 0.01f);
        TestEqual(TEXT("VLA"), Shot.VerticalLaunchDeg, 6.95f, 0.001f);
        TestEqual(TEXT("HLA"), Shot.HorizontalLaunchDeg, -0.63f, 0.001f);
        TestEqual(TEXT("Total spin"), Shot.TotalSpinRpm, 1933);
        TestEqual(TEXT("Spin axis"), Shot.SpinAxisDeg, -1.42f, 0.001f);
        TestTrue(TEXT("Measured values"), Shot.BallSpeedSource == ELaunchMonitorValueSource::Measured &&
                                              Shot.SideSpinSource == ELaunchMonitorValueSource::Measured);
    }
    if (TestEqual(TEXT("One club event"), ClubData.Num(), 1))
    {
        TestEqual(TEXT("Club data shot number"), ClubData[0].ShotNumber, 1);
        TestTrue(TEXT("Club path measured"), ClubData[0].bHasPath);
        TestEqual(TEXT("Club path"), ClubData[0].PathDeg, 1.6f, 0.001f);
        TestFalse(TEXT("No impact location"), ClubData[0].bHasImpactHorizontal);
    }
    TestTrue(TEXT("Last shot available"), Subsystem->GetLastShot(LastShot) && LastShot.ShotNumber == 1);
    FLaunchMonitorClubData LastClub;
    TestTrue(TEXT("Last club data available"), Subsystem->GetLastClubData(LastClub) && LastClub.ShotNumber == 1);

    // Every built-in replay decodes; the putt capture carries untracked club data.
    TestTrue(TEXT("Second replay"), Subsystem->InjectReplayShot());
    TestTrue(TEXT("Third replay"), Subsystem->InjectReplayShot());
    TestEqual(TEXT("Three shots"), Shots.Num(), 3);
    TestTrue(TEXT("Putt type"), Shots.Last().ShotType == ELaunchMonitorShotType::Putt);
    TestTrue(TEXT("Putt club not tracked"), Subsystem->GetLastClubData(LastClub) && !LastClub.HasReading());

    // Malformed input is reported, not delivered.
    TestFalse(TEXT("Malformed replay rejected"), Subsystem->InjectReplayFrames(TEXT("11023"), FString()));
    TestEqual(TEXT("No shot from malformed input"), Shots.Num(), 3);
    TestTrue(TEXT("Error recorded"), Subsystem->GetLastError().Contains(TEXT("not a plausible")));

    const TArray<FString> Names = Subsystem->GetReplayShotNames();
    TestEqual(TEXT("Replay catalog has ten presets"), Names.Num(), 10);
    TestFalse(TEXT("Negative preset rejected"), Subsystem->InjectReplayShotAt(-1));
    TestFalse(TEXT("Past-end preset rejected"), Subsystem->InjectReplayShotAt(Names.Num()));
    TestEqual(TEXT("Browsing and invalid selection do not fire shots"), Shots.Num(), 3);
    for (int32 Index = 0; Index < Names.Num(); ++Index)
    {
        TestFalse(TEXT("Preset name present"), Names[Index].IsEmpty());
        TestFalse(TEXT("Preset decodes for description"), Subsystem->GetReplayShotDescription(Index).IsEmpty());
        TestTrue(TEXT("Each preset delivers"), Subsystem->InjectReplayShotAt(Index));
    }
    TestTrue(TEXT("Straight putt type"), Shots.Last().ShotType == ELaunchMonitorShotType::Putt);
    TestEqual(TEXT("Straight putt speed at packet precision"), Shots.Last().GetBallSpeedMph(), 5.0f, 0.02f);
    TestEqual(TEXT("Straight putt horizontal launch"), Shots.Last().HorizontalLaunchDeg, 0.0f);
    TestEqual(TEXT("Straight putt vertical launch"), Shots.Last().VerticalLaunchDeg, 0.0f);
    TestEqual(TEXT("Straight putt spin"), Shots.Last().TotalSpinRpm, 0);
    TestTrue(TEXT("Mid iron preset selected"), Subsystem->InjectReplayShotAt(3));
    const FLaunchMonitorShot Previous = Shots.Last();
    TestEqual(TEXT("Earlier speed"), Previous.GetBallSpeedMph(), 75.7f, 0.02f);
    TestEqual(TEXT("Earlier HLA remains left"), Previous.HorizontalLaunchDeg, -0.97f, 0.001f);
    TestEqual(TEXT("Earlier spin"), Previous.TotalSpinRpm, 4463);
    TestEqual(TEXT("Earlier axis"), Previous.SpinAxisDeg, 20.62f, 0.001f);
    TestTrue(TEXT("Synthetic components derived"), Previous.BackSpinSource == ELaunchMonitorValueSource::Derived);
    TestFalse(TEXT("No stale club data on preset without club frame"), Subsystem->GetLastClubData(LastClub));
    TestTrue(TEXT("Preset can repeat"), Subsystem->InjectReplayShotAt(3));
    TestEqual(TEXT("Repeat keeps input"), Shots.Last().BallSpeedMps, Previous.BallSpeedMps);
    TestEqual(TEXT("Repeat gets new shot number"), Shots.Last().ShotNumber, Previous.ShotNumber + 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLaunchMonitorCustomShotTest, "UOpenGolf.LaunchMonitor.CustomShot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLaunchMonitorCustomShotTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
    UOpenLaunchMonitorSubsystem* Subsystem = NewObject<UOpenLaunchMonitorSubsystem>(GameInstance);
    TArray<FLaunchMonitorShot> Shots;
    Subsystem->OnShotReceivedNative.AddLambda([&Shots](const FLaunchMonitorShot& Shot) { Shots.Add(Shot); });

    // Typed values arrive exactly, not at Square packet precision.
    TestTrue(TEXT("Straight custom shot delivered"), Subsystem->InjectCustomShot(44.704f, 16.0f, 0.0f, 5000, 0.0f));
    if (TestEqual(TEXT("One custom shot"), Shots.Num(), 1))
    {
        const FLaunchMonitorShot& Shot = Shots[0];
        TestEqual(TEXT("Custom source"), Shot.Source, FString(LaunchMonitorCustomSource));
        TestTrue(TEXT("Custom shot type unknown"), Shot.ShotType == ELaunchMonitorShotType::Unknown);
        TestEqual(TEXT("Exact speed"), Shot.BallSpeedMps, 44.704f);
        TestEqual(TEXT("100 mph"), Shot.GetBallSpeedMph(), 100.0f, 0.001f);
        TestEqual(TEXT("Exact VLA"), Shot.VerticalLaunchDeg, 16.0f);
        TestEqual(TEXT("Exact HLA"), Shot.HorizontalLaunchDeg, 0.0f);
        TestEqual(TEXT("Exact spin"), Shot.TotalSpinRpm, 5000);
        TestTrue(TEXT("Typed values measured"), Shot.BallSpeedSource == ELaunchMonitorValueSource::Measured &&
            Shot.VerticalLaunchSource == ELaunchMonitorValueSource::Measured && Shot.HorizontalLaunchSource == ELaunchMonitorValueSource::Measured &&
            Shot.TotalSpinSource == ELaunchMonitorValueSource::Measured && Shot.SpinAxisSource == ELaunchMonitorValueSource::Measured);
        TestEqual(TEXT("Straight shot is all back spin"), Shot.BackSpinRpm, 5000);
        TestEqual(TEXT("Straight shot has no side spin"), Shot.SideSpinRpm, 0);
        TestTrue(TEXT("Spin components derived"), Shot.BackSpinSource == ELaunchMonitorValueSource::Derived &&
            Shot.SideSpinSource == ELaunchMonitorValueSource::Derived);
    }

    // A tilted axis splits the spin: 6000 rpm at -30 degrees is 5196 back and 3000 to the left.
    TestTrue(TEXT("Curving custom shot delivered"), Subsystem->InjectCustomShot(60.0f, 12.5f, -2.0f, 6000, -30.0f));
    if (TestEqual(TEXT("Two custom shots"), Shots.Num(), 2))
    {
        TestEqual(TEXT("Tilted back spin"), Shots[1].BackSpinRpm, 5196);
        TestEqual(TEXT("Tilted side spin"), Shots[1].SideSpinRpm, -3000);
        TestEqual(TEXT("Left HLA kept"), Shots[1].HorizontalLaunchDeg, -2.0f);
        TestEqual(TEXT("Numbered after the first"), Shots[1].ShotNumber, Shots[0].ShotNumber + 1);
    }
    FLaunchMonitorShot Last;
    TestTrue(TEXT("Custom shot is the last shot"), Subsystem->GetLastShot(Last) && Last.ShotNumber == Shots.Last().ShotNumber);

    // Values the decoders would drop are reported, never delivered.
    struct FBad { const TCHAR* Name; float Speed, Vla, Hla; int32 Spin; float Axis; };
    const FBad Bad[] = {
        {TEXT("Zero speed"), 0.0f, 10.0f, 0.0f, 3000, 0.0f},
        {TEXT("Speed at limit"), 250.0f, 10.0f, 0.0f, 3000, 0.0f},
        {TEXT("Vertical quarter turn"), 40.0f, 90.0f, 0.0f, 3000, 0.0f},
        {TEXT("Horizontal quarter turn"), 40.0f, 10.0f, -90.0f, 3000, 0.0f},
        {TEXT("Negative spin"), 40.0f, 10.0f, 0.0f, -1, 0.0f},
        {TEXT("Spin at limit"), 40.0f, 10.0f, 0.0f, 30000, 0.0f},
        {TEXT("Axis past half turn"), 40.0f, 10.0f, 0.0f, 3000, 180.5f},
        {TEXT("Not a number"), 40.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f, 3000, 0.0f},
    };
    for (const FBad& Case : Bad)
        TestFalse(Case.Name, Subsystem->InjectCustomShot(Case.Speed, Case.Vla, Case.Hla, Case.Spin, Case.Axis));
    TestEqual(TEXT("Rejected custom shots deliver nothing"), Shots.Num(), 2);
    TestTrue(TEXT("Rejection reported"), Subsystem->GetLastError().Contains(TEXT("Custom shot")));
    return true;
}

#endif
