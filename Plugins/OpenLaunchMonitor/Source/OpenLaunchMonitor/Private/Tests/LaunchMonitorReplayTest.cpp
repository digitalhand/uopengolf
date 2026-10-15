// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "UObject/Package.h"

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
    return true;
}

#endif
