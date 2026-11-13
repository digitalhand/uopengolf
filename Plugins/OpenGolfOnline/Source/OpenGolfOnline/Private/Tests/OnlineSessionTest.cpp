// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "OpenGolfOnlineSubsystem.h"
#include "OnlineJson.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfOnlineSessionTest,"OpenGolf.Online.Session",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FOpenGolfOnlineSessionTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    FConfig Config;
    Config.Authority=TEXT("https://login.example.test/"); Config.ClientId=TEXT("test-client");
    Config.Audience=TEXT("https://api.example.test");Config.ApiOrigin=TEXT("https://api.example.test");
    Config.ApplicationName=TEXT("Test golf");Config.StorageName=TEXT("OpenGolfOnlineTests");Config.CallbackPorts={47801};
    TestTrue(TEXT("Plugin accepts a different host project"),Config.IsValid());
    struct FPending { FString Url, Body, Token; FHttpCallback Done; };
    TArray<FPending> Pending;
    auto* Session=NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    const FString Root=FPaths::ProjectSavedDir()/TEXT("Automation")/FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Session->SetupForTests(Config,[&Pending](const FString& Url,const FString&,const FString& Body,const FString& Token,FHttpCallback Done)
    {
        Pending.Add({Url,Body,Token,MoveTemp(Done)});
        auto Cancel=MakeShared<FRequestCancellation,ESPMode::ThreadSafe>(); Cancel->Finished.store(true); return Cancel;
    },Root);
    auto Respond=[&](int32 Status,const FString& Body)
    {
        if(Pending.IsEmpty()){AddError(TEXT("Expected a queued request"));return;}
        auto Request=MoveTemp(Pending[0]);Pending.RemoveAt(0);Request.Done({Status,Body,true});
    };
    auto Login=[&](const FString& Subject)
    {
        Session->SignInWithQr();
        Respond(200,TEXT("{\"device_code\":\"private-code\",\"user_code\":\"USER-CODE\",\"verification_uri\":\"https://login.example.test/activate\",\"verification_uri_complete\":\"https://login.example.test/activate?user_code=USER-CODE\",\"expires_in\":600,\"interval\":5}"));
        Session->PollAt=0;Session->Tick(0);
        Respond(200,TEXT("{\"access_token\":\"access-a\",\"refresh_token\":\"refresh-a\",\"token_type\":\"Bearer\",\"expires_in\":3600}"));
        Respond(200,TEXT("{\"sub\":\"")+Subject+TEXT("\"}"));
        Respond(200,TEXT("{\"id\":\"player\",\"display_name\":\"Golfer\"}"));
    };
    Login(TEXT("auth0|one"));
    TestTrue(TEXT("QR login established a session"),Session->HasSession());
    TestFalse(TEXT("Private device code removed after success"),Session->IsSigningIn());
    const FString FirstOwner=Session->GetAccountKey();
    TestEqual(TEXT("Account key is an opaque hash"),FirstOwner.Len(),64);
    auto Shot=MakeShared<FJsonObject>();Shot->SetStringField(TEXT("id"),FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
    Shot->SetField(TEXT("carry_yd"),MakeShared<FJsonValueNumberString>(TEXT("150.3")));
    TestTrue(TEXT("Shot belongs to verified account"),Session->QueueShot(Shot,FirstOwner));
    TestFalse(TEXT("Foreign account cannot queue shots"),Session->QueueShot(Shot,TEXT("other")));
    auto* Reload=NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    Reload->SetupForTests(Config,[](const FString&,const FString&,const FString&,const FString&,FHttpCallback)
    {return MakeShared<FRequestCancellation,ESPMode::ThreadSafe>();},Root);
    Reload->LoadLocal();
    TestEqual(TEXT("Restart restores the pending queue"),Reload->GetPendingCount(),1);
    TestFalse(TEXT("Cached profile cannot grant a session"),Reload->HasSession());
    const FString RestoredJson=Json(Reload->Outbox[0]->AsObject().ToSharedRef());
    TestFalse(TEXT("Restart does not expand decimal fields into binary floating point noise"),RestoredJson.Contains(TEXT("150.300")));
    Session->CancelSignIn();
    TestTrue(TEXT("Cancelling another login keeps session"),Session->HasSession());
    Login(TEXT("auth0|one"));
    TestEqual(TEXT("Same-account login keeps pending shot"),Session->GetPendingCount(),1);
    // The successful profile sync starts an upload. Finish it before switching accounts.
    if(!Pending.IsEmpty())Respond(200,TEXT("{\"accepted\":1,\"duplicates\":0}"));
    Session->QueueShot(Shot,FirstOwner);
    Login(TEXT("auth0|two"));
    TestTrue(TEXT("Different account has different key"),Session->GetAccountKey()!=FirstOwner);
    TestEqual(TEXT("Different account cannot inherit queued shots"),Session->GetPendingCount(),0);
    int32 RefreshCompletions=0;
    Session->Refresh([&](bool Ok){if(Ok)++RefreshCompletions;});
    Session->Refresh([&](bool Ok){if(Ok)++RefreshCompletions;});
    TestEqual(TEXT("Concurrent callers share one refresh"),Pending.Num(),1);
    Respond(200,TEXT("{\"access_token\":\"access-b\",\"refresh_token\":\"refresh-b\",\"token_type\":\"Bearer\",\"expires_in\":3600}"));
    Respond(200,TEXT("{\"sub\":\"auth0|two\"}"));
    TestEqual(TEXT("Both refresh callers complete"),RefreshCompletions,2);
    TestEqual(TEXT("Rotated refresh token replaces the previous one"),Session->RefreshToken,FString(TEXT("refresh-b")));
    Session->QueueShot(Shot, Session->GetAccountKey());
    const FString RecoverOwner = Session->GetAccountKey();
    Session->Refresh([](bool){});
    Respond(400, TEXT("{\"error\":\"invalid_grant\"}"));
    TestFalse(TEXT("Rejected refresh ends authentication"), Session->HasSession());
    TestEqual(TEXT("Rejected refresh preserves pending shots"), Session->GetPendingCount(), 1);
    Reload->LoadLocal();
    TestEqual(TEXT("Recovery queue survives restart"), Reload->GetPendingCount(), 1);
    Login(TEXT("auth0|two"));
    TestEqual(TEXT("Same account recovers queue"), Session->GetAccountKey(), RecoverOwner);
    TestEqual(TEXT("Recovered shot remains until acknowledged"), Session->GetPendingCount(), 1);
    Respond(200, TEXT("{\"accepted\":1,\"duplicates\":0}"));
    bool Refreshed=false;
    Session->Refresh([&](bool Ok){Refreshed=Ok;});
    TestTrue(TEXT("Refresh request queued"),Pending.Num()==1);
    FHttpCallback OldReply=MoveTemp(Pending[0].Done);Pending.RemoveAt(0);
    Session->SignOut();
    OldReply({200,TEXT("{\"access_token\":\"late\",\"refresh_token\":\"late\",\"token_type\":\"Bearer\",\"expires_in\":3600}"),true});
    TestFalse(TEXT("Late refresh cannot restore signed-out session"),Session->HasSession());
    TestFalse(TEXT("Late refresh waiter cannot report success"),Refreshed);
    TestTrue(TEXT("Logout clears local identity"),Session->GetAccountKey().IsEmpty());
    TestTrue(TEXT("Logout blocks automatic restore on disk"), IFileManager::Get().FileExists(*(Root / TEXT("signed-out"))));
    Pending.Empty();
    Login(TEXT("auth0|two"));
    TestFalse(TEXT("Explicit successful sign-in permits restore"), IFileManager::Get().FileExists(*(Root / TEXT("signed-out"))));
    Session->SignOut();
    Pending.Empty();
    Session->SignInWithQr();
    Respond(200,TEXT("{\"device_code\":\"code\",\"user_code\":\"PUBLIC\",\"verification_uri\":\"https://evil.test/\",\"verification_uri_complete\":\"https://evil.test/\",\"expires_in\":600,\"interval\":5}"));
    TestFalse(TEXT("Foreign verification link rejected"),Session->IsSigningIn());
    TestTrue(TEXT("Foreign link never exposed"),Session->GetQrUrl().IsEmpty());
    auto* Shutdown=NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    Shutdown->SetupForTests(Config,[](const FString&,const FString&,const FString&,const FString&,FHttpCallback)
    {auto H=MakeShared<FRequestCancellation,ESPMode::ThreadSafe>();H->Finished.store(true);return H;},Root/TEXT("shutdown"));
    TArray<TFunction<void()>> CredentialTasks;
    int32 Deletes=0;
    Shutdown->TestCredentialDispatch=[&](TFunction<void()> Task){CredentialTasks.Add(MoveTemp(Task));};
    Shutdown->TestCredentialWriter=[&](const FString&,const FString& Token){if(Token.IsEmpty())++Deletes;return EStoreResult::Ok;};
    Shutdown->RefreshToken=TEXT("test-only");
    Shutdown->SignOut();
    TestTrue(TEXT("Logout persisted before credential worker starts"),Shutdown->IsSessionRestoreBlocked());
    Shutdown->Deinitialize();
    for(auto& Task:CredentialTasks)Task();
    TestEqual(TEXT("Shutdown does not discard queued credential deletion"),Deletes,1);
    TestTrue(TEXT("Shutdown leaves automatic restore blocked"),Shutdown->IsSessionRestoreBlocked());
    Session->bStopped=true;
    IFileManager::Get().DeleteDirectory(*Root,false,true);
    return true;
}

#endif
