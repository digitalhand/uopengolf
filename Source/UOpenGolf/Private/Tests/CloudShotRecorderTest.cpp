// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Online/CloudShotRecorder.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCloudShotRecorderTest,"UOpenGolf.Online.CloudShots",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FCloudShotRecorderTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    TArray<TSharedPtr<FJsonObject>> Saved;
    bool Accept=true;
    FCloudShotRecorder Recorder([&](const auto& Shot,const FString& Owner)
    {
        TestEqual(TEXT("Account frozen at strike"),Owner,FString(TEXT("owner")));
        if(Accept)Saved.Add(Shot);
        return Accept;
    });
    FLaunchMonitorShot Shot;Shot.ShotNumber=1;Shot.ReceivedUtc=FDateTime::UtcNow();
    Shot.BallSpeedMps=50;Shot.BallSpeedSource=ELaunchMonitorValueSource::Measured;
    FOpenFairwayShotResult Result;Result.ShotNumber=10;Result.bLanded=true;Result.bAtRest=true;Result.CarryM=100;Result.RollM=-2;
    Recorder.Begin(Shot,ELaunchMonitorClub::Iron7,{TEXT("owner"),true},true,true);
    Recorder.BindSimulation(1,10);Recorder.OnRest(Result);
    TestEqual(TEXT("Waits for late club reading"),Saved.Num(),0);
    FLaunchMonitorClubData Club;Club.ShotNumber=1;Club.bHasPath=true;Club.PathDeg=2.5;
    Recorder.OnClubData(Club);
    TestEqual(TEXT("Late club reading completes exactly one shot"),Saved.Num(),1);
    Recorder.OnRest(Result);Recorder.Release();
    TestEqual(TEXT("Repeated rest does not duplicate upload"),Saved.Num(),1);
    TestEqual(TEXT("Selected club captured at strike"),Saved[0]->GetStringField(TEXT("club")),FString(TEXT("7I")));
    TestEqual(TEXT("Measured club reading retained"),Saved[0]->GetObjectField(TEXT("details"))->GetNumberField(TEXT("club_path_deg")),2.5);
    const FString Session=Saved[0]->GetStringField(TEXT("session_id"));
    for(int32 Kind=0;Kind<3;++Kind)
    {
        Recorder.Begin(Shot,ELaunchMonitorClub::Driver,{TEXT("owner"),Kind!=0},Kind!=1,Kind!=2);
        Recorder.BindSimulation(1,10);Recorder.OnRest(Result);Recorder.Release();
    }
    TestEqual(TEXT("Toggle off, hole mode and replay are excluded"),Saved.Num(),1);
    Recorder.Begin(Shot,ELaunchMonitorClub::Driver,{TEXT("owner"),true},true,true);
    Recorder.BindSimulation(1,11);Recorder.OnRest(Result);Recorder.Cancel();
    TestEqual(TEXT("Interrupted simulation cannot upload unrelated result"),Saved.Num(),1);
    Accept=false;
    Recorder.Begin(Shot,ELaunchMonitorClub::Driver,{TEXT("owner"),true},true,true);
    Recorder.BindSimulation(1,10);Recorder.OnRest(Result);Recorder.Release();
    TestEqual(TEXT("Refused queue retains completed shot"),Recorder.GetUnsavedCount(),1);
    Accept=true;Recorder.Retry();
    TestEqual(TEXT("Retained shot retries once"),Saved.Num(),2);
    TestEqual(TEXT("Range session identity retained"),Saved[1]->GetStringField(TEXT("session_id")),Session);
    Recorder.Retry();TestEqual(TEXT("Successful retry removed"),Saved.Num(),2);
    return true;
}

#endif
