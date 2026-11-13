// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "OpenGolfOnlineSubsystem.h"
#include "OnlineJson.h"
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
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
    FString PermissionPayload = FBase64::Encode(FString(TEXT("{\"permissions\":[\"ogolf:admin\"]}")));
    PermissionPayload.ReplaceInline(TEXT("+"),TEXT("-")); PermissionPayload.ReplaceInline(TEXT("/"),TEXT("_")); PermissionPayload.ReplaceInline(TEXT("="),TEXT(""));
    const FString AdminToken = TEXT("header.") + PermissionPayload + TEXT(".signature");
    const FString DeviceReply=TEXT("{\"device_code\":\"private-code\",\"user_code\":\"USER-CODE\",\"verification_uri\":\"https://login.example.test/activate\",\"verification_uri_complete\":\"https://login.example.test/activate?user_code=USER-CODE\",\"expires_in\":600,\"interval\":5}");
    auto Login=[&](const FString& Subject)
    {
        Session->KeepQrSignInReady();
        Respond(200,DeviceReply);
        Session->PollAt=0;Session->Tick(0);
        Respond(200,TEXT("{\"access_token\":\"")+AdminToken+TEXT("\",\"refresh_token\":\"refresh-a\",\"token_type\":\"Bearer\",\"expires_in\":3600}"));
        Respond(200,TEXT("{\"sub\":\"")+Subject+TEXT("\"}"));
        Respond(200,TEXT("{\"id\":\"player\",\"display_name\":\"Golfer\"}"));
    };
    Login(TEXT("auth0|one"));
    TestTrue(TEXT("Accepted admin claim enables manual shots"),Session->HasPermission(TEXT("ogolf:admin")));
    TestFalse(TEXT("Permission match is exact"),Session->HasPermission(TEXT("ogolf:Admin")));
    const double ValidExpiry = Session->TokenExpiresAt;
    Session->TokenExpiresAt = 0;
    TestFalse(TEXT("Expired token denies permission"),Session->HasPermission(TEXT("ogolf:admin")));
    Session->TokenExpiresAt = ValidExpiry;
    TestTrue(TEXT("QR login established a session"),Session->HasSession());
    TestFalse(TEXT("Sign-in finished"),Session->IsSigningIn());
    TestTrue(TEXT("Private device code removed after success"),Session->DeviceCode.IsEmpty()&&Session->GetQrUrl().IsEmpty());
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
    TestFalse(TEXT("Refresh without permissions revokes admin"),Session->HasPermission(TEXT("ogolf:admin")));
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
    TestFalse(TEXT("Logout clears permissions"),Session->HasPermission(TEXT("ogolf:admin")));
    TestTrue(TEXT("Logout clears local identity"),Session->GetAccountKey().IsEmpty());
    TestTrue(TEXT("Logout blocks automatic restore on disk"), IFileManager::Get().FileExists(*(Root / TEXT("signed-out"))));
    Pending.Empty();
    Login(TEXT("auth0|two"));
    TestFalse(TEXT("Explicit successful sign-in permits restore"), IFileManager::Get().FileExists(*(Root / TEXT("signed-out"))));
    Session->SignOut();
    Pending.Empty();
    Session->KeepQrSignInReady();
    Respond(200,TEXT("{\"device_code\":\"code\",\"user_code\":\"PUBLIC\",\"verification_uri\":\"https://evil.test/\",\"verification_uri_complete\":\"https://evil.test/\",\"expires_in\":600,\"interval\":5}"));
    TestTrue(TEXT("Foreign verification link rejected"),Session->IsQrSignInFailing()&&Session->DeviceCode.IsEmpty());
    TestTrue(TEXT("Foreign link never exposed"),Session->GetQrUrl().IsEmpty());
    Session->KeepQrSignInReady();
    TestEqual(TEXT("A failed QR request waits before retrying"),Pending.Num(),0);
    // The standing QR code: held while signed out, kept through a cancelled browser sign-in, replaced when it expires
    // or is declined, and dropped on sign-out.
    Session->QrRetryAt=0;Session->KeepQrSignInReady();
    Respond(200,DeviceReply);
    TestFalse(TEXT("A QR code is ready"),Session->GetQrUrl().IsEmpty());
    TestEqual(TEXT("The player's code is exposed"),Session->GetUserCode(),FString(TEXT("USER-CODE")));
    TestFalse(TEXT("A ready code clears the failure"),Session->IsQrSignInFailing());
    TestTrue(TEXT("Holding a QR code is not signing in"),Session->GetAccountState()==EOpenGolfAccountState::SignedOut);
    Session->CancelSignIn();
    TestFalse(TEXT("Cancelling a browser sign-in keeps the QR code"),Session->GetQrUrl().IsEmpty());
    Session->KeepQrSignInReady();
    TestEqual(TEXT("A valid code is not requested again"),Pending.Num(),0);
    Session->QrWantedUntil=0;Session->PollAt=0;Session->Tick(0);
    TestEqual(TEXT("A code nobody is shown is not polled"),Pending.Num(),0);
    Session->QrExpiresAt=0;Session->KeepQrSignInReady();
    TestTrue(TEXT("An expired code is dropped"),Session->GetQrUrl().IsEmpty());
    TestEqual(TEXT("An expired code is replaced"),Pending.Num(),1);
    Respond(200,DeviceReply);
    Session->PollAt=0;Session->Tick(0);
    Respond(403,TEXT("{\"error\":\"access_denied\"}"));
    TestTrue(TEXT("A declined code is dropped"),Session->GetQrUrl().IsEmpty());
    Session->QrRetryAt=0;Session->KeepQrSignInReady();
    Respond(200,DeviceReply);
    Session->SignOut();
    TestTrue(TEXT("Signing out drops the QR code"),Session->GetQrUrl().IsEmpty());
    Pending.Empty();
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
