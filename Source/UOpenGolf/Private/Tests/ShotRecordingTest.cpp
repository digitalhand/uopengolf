// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ShotDisplay.h"
#include "ShotRecording.h"

namespace
{
    FLaunchMonitorShot RecordingTestShot(int32 ShotNumber, const TCHAR* Source)
    {
        using ESource = ELaunchMonitorValueSource;
        FLaunchMonitorShot Shot;
        Shot.ShotNumber = ShotNumber;
        Shot.Source = Source;
        Shot.BallSpeedMps = 51.17f;
        Shot.VerticalLaunchDeg = 6.95f;
        Shot.HorizontalLaunchDeg = -0.63f;
        Shot.TotalSpinRpm = 1933;
        Shot.SpinAxisDeg = -1.42f;
        Shot.BackSpinRpm = 1932;
        Shot.SideSpinRpm = -48;
        Shot.BallSpeedSource = ESource::Measured;
        Shot.VerticalLaunchSource = ESource::Measured;
        Shot.HorizontalLaunchSource = ESource::Measured;
        Shot.TotalSpinSource = ESource::Measured;
        Shot.SpinAxisSource = ESource::Measured;
        Shot.BackSpinSource = ESource::Derived;
        // Side spin left unmeasured: it must be left out of the file.
        return Shot;
    }

    TSharedPtr<FJsonObject> ParseRecordingJson(const FString& Json)
    {
        TSharedPtr<FJsonObject> Root;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root);
        return Root;
    }

    TArray<FString> RecordedFiles(const FString& Dir)
    {
        TArray<FString> Files;
        IFileManager::Get().FindFiles(Files, *FPaths::Combine(Dir, TEXT("*.json")), true, false);
        Files.Sort();
        return Files;
    }
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotRecordingTest, "UOpenGolf.Range.ShotRecording",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShotRecordingTest::RunTest(const FString& Parameters)
{
    using namespace ShotRecording;
    using EState = ELaunchMonitorConnectionState;

    // Names follow the shot-library club codes; the index counts every shot in the session.
    TestEqual(TEXT("Driver code"), ClubCode(ELaunchMonitorClub::Driver), FString(TEXT("driver")));
    TestEqual(TEXT("3 wood code"), ClubCode(ELaunchMonitorClub::Wood3), FString(TEXT("3w")));
    TestEqual(TEXT("7 iron code"), ClubCode(ELaunchMonitorClub::Iron7), FString(TEXT("7i")));
    TestEqual(TEXT("Pitching wedge code"), ClubCode(ELaunchMonitorClub::PitchingWedge), FString(TEXT("pw")));
    TestEqual(TEXT("Lob wedge code"), ClubCode(ELaunchMonitorClub::LobWedge), FString(TEXT("lw")));
    TestEqual(TEXT("File name"), ShotFileName(ELaunchMonitorClub::Driver, 1), FString(TEXT("club_driver_shot_1.json")));
    TestEqual(TEXT("Session folder"), SessionFolderName(FDateTime(2026, 10, 15, 14, 3, 22)), FString(TEXT("session_20261015-140322")));
    const FString DefaultRoot = DefaultRootDir();
    TestTrue(TEXT("Default root is in the user's documents"), DefaultRoot.StartsWith(FPlatformProcess::UserDir()));
    TestTrue(TEXT("Default root folder"), DefaultRoot.EndsWith(TEXT("UOpenGolf/Saved/ShotRecordings")));
    TestEqual(TEXT("Recording status off"), ShotDisplay::RecordingStatus(false, 4), FString(TEXT("Off")));
    TestEqual(TEXT("Recording status on"), ShotDisplay::RecordingStatus(true, 3), FString(TEXT("On · 3")));

    // Only real device shots while connected.
    TestTrue(TEXT("Device shot when ready"), IsRecordable(RecordingTestShot(1, TEXT("Square Home")), EState::Ready));
    TestTrue(TEXT("Device shot when connected"), IsRecordable(RecordingTestShot(1, TEXT("Square Home")), EState::Connected));
    TestFalse(TEXT("Device shot when disconnected"), IsRecordable(RecordingTestShot(1, TEXT("Square Home")), EState::Disconnected));
    TestFalse(TEXT("Replay shot"), IsRecordable(RecordingTestShot(1, TEXT("Replay")), EState::Ready));

    // Shot JSON: mph, unmeasured values left out, no ClubData without a reading.
    FRecordedShot Recorded;
    Recorded.Shot = RecordingTestShot(5, TEXT("Square Home"));
    Recorded.SelectedClub = ELaunchMonitorClub::Wood3;
    Recorded.RecordedLocal = FDateTime(2026, 10, 15, 14, 3, 22);
    const FString PlainJson = BuildShotJson(Recorded, 2);
    // Decimal values are rounded to hundredths; whole numbers are written unchanged.
    TestTrue(TEXT("Speed rounded"), PlainJson.Contains(TEXT("\"Speed\": 114.46,")));
    TestTrue(TEXT("Angle rounded"), PlainJson.Contains(TEXT("\"HLA\": -0.63,")));
    TestTrue(TEXT("Whole-number spin unchanged"), PlainJson.Contains(TEXT("\"TotalSpin\": 1933,")));
    TestTrue(TEXT("Shot number unchanged"), PlainJson.Contains(TEXT("\"ShotNumber\": 2,")));
    const TSharedPtr<FJsonObject> Plain = ParseRecordingJson(PlainJson);
    if (!TestTrue(TEXT("JSON parses"), Plain.IsValid()))
    {
        return false;
    }
    TestEqual(TEXT("DeviceID"), Plain->GetStringField(TEXT("DeviceID")), FString(TEXT("Square Home")));
    TestEqual(TEXT("ShotNumber is the session index"), Plain->GetIntegerField(TEXT("ShotNumber")), 2);
    const TSharedPtr<FJsonObject> Ball = Plain->GetObjectField(TEXT("BallData"));
    TestEqual(TEXT("Speed in mph"), Ball->GetNumberField(TEXT("Speed")), 51.17 * 2.2369363, 0.01);
    TestEqual(TEXT("VLA"), Ball->GetNumberField(TEXT("VLA")), 6.95, 0.001);
    TestEqual(TEXT("BackSpin (derived) kept"), Ball->GetNumberField(TEXT("BackSpin")), 1932.0);
    TestFalse(TEXT("Unmeasured SideSpin left out"), Ball->HasField(TEXT("SideSpin")));
    TestFalse(TEXT("No ClubData without a reading"), Plain->HasField(TEXT("ClubData")));
    TestFalse(TEXT("ContainsClubData false"), Plain->GetObjectField(TEXT("ShotDataOptions"))->GetBoolField(TEXT("ContainsClubData")));
    const TSharedPtr<FJsonObject> Game = Plain->GetObjectField(TEXT("UOpenGolf"));
    TestEqual(TEXT("Club code"), Game->GetStringField(TEXT("Club")), FString(TEXT("3w")));
    TestEqual(TEXT("Club name"), Game->GetStringField(TEXT("ClubName")), FString(TEXT("3 Wood")));
    TestFalse(TEXT("No engine block before the ball stops"), Game->HasField(TEXT("Engine")));

    // Recorder: writes only while enabled, for device shots, once the ball is at rest.
    const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation"), TEXT("ShotRecordingTest"));
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    {
        FShotRecorder Recorder(Root);
        FOpenFairwayShotResult Rest;
        Rest.ShotNumber = 44;
        Rest.bLanded = true;
        Rest.bAtRest = true;
        Rest.CarryM = 100.0f * 0.9144f;
        Rest.TotalM = 120.0f * 0.9144f;

        Recorder.OnShot(RecordingTestShot(1, TEXT("Square Home")), EState::Ready, ELaunchMonitorClub::Driver, false, TEXT("1.0"));
        Recorder.OnBallAtRest(Rest);
        TestEqual(TEXT("Disabled records nothing"), Recorder.GetShotCount(), 0);

        Recorder.SetEnabled(true);
        Recorder.OnShot(RecordingTestShot(2, TEXT("Replay")), EState::Ready, ELaunchMonitorClub::Driver, false, TEXT("1.0"));
        Recorder.OnBallAtRest(Rest);
        Recorder.OnShot(RecordingTestShot(3, TEXT("Square Home")), EState::Disconnected, ELaunchMonitorClub::Driver, false, TEXT("1.0"));
        Recorder.OnBallAtRest(Rest);
        TestEqual(TEXT("Replays and disconnected shots are not recorded"), Recorder.GetShotCount(), 0);
        TestTrue(TEXT("No folder before the first recorded shot"), Recorder.GetSessionDir().IsEmpty());

        Recorder.OnShot(RecordingTestShot(4, TEXT("Square Home")), EState::Ready, ELaunchMonitorClub::Driver, false, TEXT("1.0"));
        Recorder.BindSimulation(4, Rest.ShotNumber);
        FLaunchMonitorClubData Club;
        Club.ShotNumber = 4;
        Club.bHasPath = true;
        Club.PathDeg = -0.18f;
        Recorder.OnClubData(Club);
        TestEqual(TEXT("Written only at rest"), Recorder.GetShotCount(), 0);
        Recorder.OnBallAtRest(Rest);
        Recorder.OnShot(RecordingTestShot(5, TEXT("Square Home")), EState::Ready, ELaunchMonitorClub::Iron7, false, TEXT("1.0"));
        Recorder.BindSimulation(5, Rest.ShotNumber);
        FLaunchMonitorClubData MissingClub;
        MissingClub.ShotNumber = 5;
        Recorder.OnClubData(MissingClub);
        Recorder.OnBallAtRest(Rest);
        TestEqual(TEXT("Two shots recorded"), Recorder.GetShotCount(), 2);

        const FString FirstSession = Recorder.GetSessionDir();
        const TArray<FString> Files = RecordedFiles(FirstSession);
        TestEqual(TEXT("Session folder name"), FPaths::GetCleanFilename(FirstSession).Left(8), FString(TEXT("session_")));
        if (TestEqual(TEXT("Two files"), Files.Num(), 2))
        {
            TestEqual(TEXT("First file"), Files[0], FString(TEXT("club_7i_shot_2.json")));
            TestEqual(TEXT("Second file"), Files[1], FString(TEXT("club_driver_shot_1.json")));
            FString Json;
            FFileHelper::LoadFileToString(Json, *FPaths::Combine(FirstSession, TEXT("club_driver_shot_1.json")));
            const TSharedPtr<FJsonObject> Saved = ParseRecordingJson(Json);
            if (TestTrue(TEXT("Saved JSON parses"), Saved.IsValid()))
            {
                TestEqual(TEXT("Club data attached by shot number"), Saved->GetObjectField(TEXT("ClubData"))->GetNumberField(TEXT("Path")), -0.18, 0.001);
                const TSharedPtr<FJsonObject> Engine = Saved->GetObjectField(TEXT("UOpenGolf"))->GetObjectField(TEXT("Engine"));
                TestEqual(TEXT("Engine carry in yards"), Engine->GetNumberField(TEXT("CarryYd")), 100.0, 0.01);
                TestEqual(TEXT("Engine total in yards"), Engine->GetNumberField(TEXT("TotalYd")), 120.0, 0.01);
            }
        }

        // Switching off writes a pending shot; switching on again starts a new session.
        Recorder.OnShot(RecordingTestShot(6, TEXT("Square Home")), EState::Ready, ELaunchMonitorClub::PitchingWedge, false, TEXT("1.0"));
        Recorder.SetEnabled(false);
        TestEqual(TEXT("Pending shot written when switched off"), RecordedFiles(FirstSession).Num(), 3);
        Recorder.SetEnabled(true);
        TestEqual(TEXT("Count resets for the new session"), Recorder.GetShotCount(), 0);
        Recorder.OnShot(RecordingTestShot(7, TEXT("Square Home")), EState::Ready, ELaunchMonitorClub::Driver, false, TEXT("1.0"));
        Recorder.Flush();
        TestNotEqual(TEXT("New session folder"), Recorder.GetSessionDir(), FirstSession);
        TestEqual(TEXT("New session starts at 1"), RecordedFiles(Recorder.GetSessionDir()), TArray<FString>{TEXT("club_driver_shot_1.json")});
    }
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotRecordingRecoveryTest, "UOpenGolf.Range.ShotRecordingRecovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FShotRecordingRecoveryTest::RunTest(const FString&)
{
    using namespace ShotRecording;
    double Now = 0;
    int32 Calls = 0;
    FString Failure;
    TMap<FString, FString> Saved;
    FShotRecorder Recorder(TEXT("UnusedTestRoot"), [&] { return Now; },
        [&](const FString& Path, const FString& Json, FString& Error)
        {
            ++Calls;
            Error = Failure;
            if (!Failure.IsEmpty()) return false;
            TestFalse(TEXT("Never overwrite a saved record"), Saved.Contains(Path));
            Saved.Add(Path, Json);
            return true;
        });
    auto Shot = [&](int32 Id)
    {
        Recorder.OnShot(RecordingTestShot(Id, TEXT("Square Home")), ELaunchMonitorConnectionState::Ready,
            ELaunchMonitorClub::Driver, false, TEXT("test"));
        Recorder.BindSimulation(Id, Id + 100);
    };
    auto Rest = [&](int32 Id)
    {
        FOpenFairwayShotResult Result;
        Result.ShotNumber = Id + 100;
        Result.bAtRest = true;
        Recorder.OnBallAtRest(Result);
    };
    auto Club = [&](int32 Id)
    {
        FLaunchMonitorClubData Data;
        Data.ShotNumber = Id;
        Data.bHasPath = true;
        Data.PathDeg = 2;
        Recorder.OnClubData(Data);
    };
    Recorder.SetEnabled(true);
    Shot(1); Rest(99); Club(1);
    TestEqual(TEXT("Unrelated result ignored"), Calls, 0);
    Rest(1); Rest(1);
    TestEqual(TEXT("Club before rest saved exactly once"), Saved.Num(), 1);
    Shot(2); Rest(2);
    TestEqual(TEXT("Rest waits for club"), Saved.Num(), 1);
    Shot(3); Club(2);
    TestEqual(TEXT("Earlier completed shot accepts late club"), Saved.Num(), 2);
    Rest(3); Now = 2.99; Recorder.Tick();
    TestEqual(TEXT("Wait through club window"), Saved.Num(), 2);
    Now = 3; Recorder.Tick();
    TestEqual(TEXT("Missing club times out"), Saved.Num(), 3);
    Club(3); TestEqual(TEXT("Late club cannot rewrite saved file"), Calls, 3);
    Shot(4); Recorder.CancelSimulation(); Club(4); Rest(4);
    TestEqual(TEXT("Reset shot saved once"), Saved.Num(), 4);
    for (const auto& Pair : Saved)
        if (Pair.Key.EndsWith(TEXT("shot_4.json")))
            TestFalse(TEXT("Reset has no unrelated engine result"), ParseRecordingJson(Pair.Value)->GetObjectField(TEXT("UOpenGolf"))->HasField(TEXT("Engine")));

    // The storage seam models failures at each transaction stage; none may discard records.
    for (const TCHAR* Stage : {TEXT("directory"), TEXT("write"), TEXT("rename")})
    {
        Failure = Stage;
        const int32 Id = Calls + 20;
        Shot(Id); Club(Id); Rest(Id);
        const int32 AtFailure = Calls;
        TestEqual(TEXT("Failed record retained"), Recorder.GetFailedSaveCount(), 1);
        Recorder.Tick();
        TestEqual(TEXT("No busy retry"), Calls, AtFailure);
        Failure.Reset(); Now += 1; Recorder.Tick();
        TestEqual(TEXT("Retry recovers record"), Recorder.GetFailedSaveCount(), 0);
        TestTrue(TEXT("Error cleared"), Recorder.GetLastError().IsEmpty());
    }
    Failure = TEXT("offline");
    Shot(50); Club(50); Rest(50);
    Shot(51); Club(51); Rest(51);
    Recorder.SetEnabled(false);
    Recorder.SetEnabled(true);
    Shot(52); Club(52); Rest(52);
    TestEqual(TEXT("Three records retained across sessions"), Recorder.GetFailedSaveCount(), 3);
    const int32 BeforeRecovery = Saved.Num();
    Recorder.SetEnabled(false);
    Failure.Reset(); Now += 1; Recorder.Tick();
    TestEqual(TEXT("All queued records saved while disabled"), Saved.Num(), BeforeRecovery + 3);
    TestEqual(TEXT("Old session saves do not increment new session count"), Recorder.GetShotCount(), 1);
    Recorder.SetEnabled(true);
    Shot(60); Failure = TEXT("offline");
    AddExpectedError(TEXT("Play ended with 1 unsaved recordings"), EAutomationExpectedErrorFlags::Contains, 1);
    Recorder.Shutdown();
    TestEqual(TEXT("Shutdown failure still retains data until destruction"), Recorder.GetFailedSaveCount(), 1);
    Failure.Reset(); Recorder.Flush();
    TestEqual(TEXT("Explicit final retry can recover"), Recorder.GetFailedSaveCount(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShotRecordingStorageTest, "UOpenGolf.Range.ShotRecordingStorage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShotRecordingStorageTest::RunTest(const FString&)
{
    const FString Root = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Automation/RecordingStorage"), FGuid::NewGuid().ToString());
    IFileManager& Files = IFileManager::Get();
    Files.MakeDirectory(*Root, true);
    const FString Folder = Root / TEXT("session");
    const FString Path = Folder / TEXT("shot.json");
    FString Error, Read;
    FFileHelper::SaveStringToFile(TEXT("obstruction"), *Folder);
    TestFalse(TEXT("Directory failure reported"), ShotRecording::WriteRecordingFile(Path, TEXT("{}"), Error));
    Files.Delete(*Folder);
    Files.MakeDirectory(*(Path + TEXT(".tmp")), true);
    TestFalse(TEXT("Temp write failure reported"), ShotRecording::WriteRecordingFile(Path, TEXT("{}"), Error));
    Files.DeleteDirectory(*(Path + TEXT(".tmp")), false, false);
    Files.MakeDirectory(*Path, true);
    TestFalse(TEXT("Rename failure reported"), ShotRecording::WriteRecordingFile(Path, TEXT("{}"), Error));
    Files.DeleteDirectory(*Path, false, false);
    TestTrue(TEXT("Retry succeeds"), ShotRecording::WriteRecordingFile(Path, TEXT("{}"), Error));
    TestFalse(TEXT("Successful save consumes temp"), Files.FileExists(*(Path + TEXT(".tmp"))));
    TestFalse(TEXT("Existing final file is never replaced"), ShotRecording::WriteRecordingFile(Path, TEXT("replacement"), Error));
    FFileHelper::LoadFileToString(Read, *Path);
    TestEqual(TEXT("Original file preserved"), Read, FString(TEXT("{}")));
    Files.DeleteDirectory(*Root, false, true);
    return true;
}
#endif
