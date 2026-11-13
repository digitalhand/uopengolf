// SPDX-License-Identifier: MIT
#include "OpenGolfOnlineSubsystem.h"
#include "OnlineJson.h"
#include "OnlineAtomicFile.h"
#include "TokenStore.h"
#include "BrowserLogin.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "openssl/sha.h"

using namespace OpenGolfOnline;

struct FOpenGolfCredentialWork
{
    FCriticalSection Mutex;
    std::atomic<uint64> Epoch{0};
    std::atomic<bool> Stopped{false};
};

namespace
{
    FString AccountHash(const FString& Subject)
    {
        const FTCHARToUTF8 Bytes(*Subject);
        uint8 Hash[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length(), Hash);
        return BytesToHex(Hash, sizeof(Hash)).ToLower();
    }
    bool Success(const FHttpResult& R) { return R.bConnected && R.Status >= 200 && R.Status < 300; }
    bool KeyValid(const FString& Key)
    {
        if (Key.Len() != 64) return false;
        for (TCHAR C : Key) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
        return true;
    }
}

EOpenGolfAccountState UOpenGolfOnlineSubsystem::GetAccountState() const
{
    if (bSigningOut) return EOpenGolfAccountState::SigningOut;
    if (bSigningIn) return EOpenGolfAccountState::SigningIn;
    if (bRestoring) return EOpenGolfAccountState::Restoring;
    if (!HasSession()) return EOpenGolfAccountState::SignedOut;
    if (AccessToken.IsEmpty() || RetryAt > 0) return EOpenGolfAccountState::Reconnecting;
    return Profile ? EOpenGolfAccountState::Ready : EOpenGolfAccountState::LoadingProfile;
}
void UOpenGolfOnlineSubsystem::Touch()
{
    ++Revision;
    const auto State = GetAccountState();
    if (LastAccountState != State) { LastAccountState = State; OnAccountStateChanged.Broadcast(); }
    if (LastProfile != Profile) { LastProfile = Profile; OnProfileChanged.Broadcast(); }
    if (LastStatus != Status || LastStorageWarning != StorageWarning)
    {
        LastStatus = Status; LastStorageWarning = StorageWarning; OnStatusChanged.Broadcast();
    }
}
bool UOpenGolfOnlineSubsystem::IsSessionRestoreBlocked() const
{
    return IFileManager::Get().FileExists(*(FPaths::GetPath(LocalPath()) / TEXT("signed-out")));
}
bool UOpenGolfOnlineSubsystem::BlockSessionRestore()
{
    const FString Directory = FPaths::GetPath(LocalPath());
    IFileManager::Get().MakeDirectory(*Directory, true);
    return WriteAtomically(Directory / TEXT("signed-out"), TEXT("Explicit sign-in required.\n"));
}
void UOpenGolfOnlineSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Config = FConfig::Load();
    CredentialWork = MakeShared<FOpenGolfCredentialWork, ESPMode::ThreadSafe>();
    if (!Config.IsValid()) { Status = TEXT("Online features are not configured."); return; }
    LoadLocal();
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &ThisClass::Tick), 0.1f);
    if (IsSessionRestoreBlocked()) { Status = TEXT("Signed out."); return; }
    bRestoring = true;
    Status = TEXT("Connecting...");
    auto Work = CredentialWork;
    const FString Storage = Config.StorageName;
    TWeakObjectPtr<ThisClass> Weak(this);
    Async(EAsyncExecution::ThreadPool, [Weak, Work, Storage]
    {
        FString Token; EStoreResult Result;
        { FScopeLock Lock(&Work->Mutex); Result = ReadRefreshToken(Storage, Token); }
        AsyncTask(ENamedThreads::GameThread, [Weak, Work, Result, Token = MoveTemp(Token)]() mutable
        {
            if (!Weak.IsValid() || Work->Epoch.load() != 0) return;
            auto* Self = Weak.Get();
            Self->bRestoring = false;
            if (Result == EStoreResult::Ok)
            {
                Self->RefreshToken = MoveTemp(Token);
                Self->Refresh([Weak](bool Ok) { if (Weak.IsValid() && Ok) Weak->SyncProfile(); });
            }
            else
            {
                Self->Status = TEXT("Signed out.");
                if (Result == EStoreResult::Unavailable) Self->StorageWarning = TEXT("Secure storage is unavailable. Sign-in will last for this game session only.");
                Self->Touch();
            }
        });
    });
}
void UOpenGolfOnlineSubsystem::Deinitialize()
{
    bStopped = true;
    ++Epoch; ++AuthAttempt;
    if (CredentialWork) { CredentialWork->Stopped.store(true); ++CredentialWork->Epoch; }
    CancelSignIn();
    for (auto& R : Requests) R->Cancel();
    for (auto& R : Requests) while (!R->Finished.load()) FPlatformProcess::Sleep(0.01f);
    FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
    OnBeforeReadProfile.Broadcast();
    if (Config.IsValid()) SaveLocal();
    AccessToken.Empty(); RefreshToken.Empty();
    Super::Deinitialize();
}
void UOpenGolfOnlineSubsystem::Send(const FString& Url, const FString& Verb, const FString& Body,
    const FString& Token, FCompletion Completion, bool bForm, bool bAuth)
{
    TWeakObjectPtr<ThisClass> Weak(this);
#if WITH_DEV_AUTOMATION_TESTS
    if (TestSender)
    {
        auto Handle = TestSender(Url,Verb,Body,Token,MoveTemp(Completion));
        (bAuth ? AuthRequests : Requests).Add(Handle);
        return;
    }
#endif
    auto Handle = Request(Config, Url, Verb, Body, Token,
        [Weak, Completion = MoveTemp(Completion)](FReply Reply) mutable
        {
            if (Weak.IsValid() && !Weak->bStopped) Completion(MoveTemp(Reply));
        }, bForm);
    (bAuth ? AuthRequests : Requests).Add(Handle);
}
void UOpenGolfOnlineSubsystem::SaveCredentials(const FString& Token, TFunction<void(bool)> Done)
{
#if WITH_DEV_AUTOMATION_TESTS
    if (TestSender && !TestCredentialDispatch) { Done(true); return; }
#endif
    auto Work = CredentialWork;
    const uint64 Version = Work->Epoch.load();
    const FString Storage = Config.StorageName;
    TWeakObjectPtr<ThisClass> Weak(this);
    TFunction<EStoreResult(const FString&,const FString&)> Writer=WriteRefreshToken;
#if WITH_DEV_AUTOMATION_TESTS
    if(TestCredentialWriter) Writer=TestCredentialWriter;
#endif
    auto Operation = [Weak, Work, Version, Storage, Token, Writer=MoveTemp(Writer), Done = MoveTemp(Done)]() mutable
    {
        EStoreResult Result = EStoreResult::Unavailable;
        {
            FScopeLock Lock(&Work->Mutex);
            if (Work->Epoch.load() != Version && !(Token.IsEmpty() && Work->Stopped.load())) return;
            Result = Writer(Storage, Token);
            if (Result != EStoreResult::Ok) ClearRefreshToken(Storage);
        }
        AsyncTask(ENamedThreads::GameThread, [Weak, Work, Version, Result, Done = MoveTemp(Done)]() mutable
        {
            if (!Weak.IsValid() || Work->Epoch.load() != Version || Weak->bStopped) return;
            Weak->StorageWarning = Result == EStoreResult::Ok ? FString() : TEXT("Secure storage is unavailable. Sign-in will last for this game session only.");
            Done(Result == EStoreResult::Ok);
        });
    };
#if WITH_DEV_AUTOMATION_TESTS
    if(TestCredentialDispatch) {TestCredentialDispatch(MoveTemp(Operation));return;}
#endif
    Async(EAsyncExecution::ThreadPool,MoveTemp(Operation));
}
void UOpenGolfOnlineSubsystem::CloseBrowser() { Browser.Reset(); }
void UOpenGolfOnlineSubsystem::CancelSignIn()
{
    ++AuthAttempt;
    for (auto& R : AuthRequests) R->Cancel();
    Requests.Append(AuthRequests); AuthRequests.Empty(); CloseBrowser();
    bSigningIn = false; DeviceCode.Empty(); UserCode.Empty(); VerificationUrl.Empty(); QrUrl.Empty();
    Status = HasSession() ? TEXT("Signed in.") : TEXT("Signed out.");
    Touch();
}
void UOpenGolfOnlineSubsystem::FinishSignInFailure(const FString& Message)
{
    CancelSignIn(); Status = Message; Touch();
}
void UOpenGolfOnlineSubsystem::SignInWithQr()
{
    if (!Config.IsValid()) return;
    CancelSignIn();
    bSigningIn = true; Status = TEXT("Getting a QR code..."); AuthDeadline = FPlatformTime::Seconds() + 30; Touch();
    const uint64 Attempt = AuthAttempt;
    const FString Payload = Form({{TEXT("client_id"),Config.ClientId},{TEXT("audience"),Config.Audience},{TEXT("scope"),TEXT("openid profile offline_access")}});
    Send(Config.Authority + TEXT("oauth/device/code"), TEXT("POST"), Payload, {}, [this, Attempt](FReply Reply)
    {
        if (Attempt != AuthAttempt) return;
        const auto Data = Object(Reply.Body);
        if (!Success(Reply) || !Data) { FinishSignInFailure(TEXT("Could not start QR sign-in. Try again or use this computer's browser.")); return; }
        DeviceCode = Text(Data,TEXT("device_code")); UserCode = Text(Data,TEXT("user_code"));
        VerificationUrl = Text(Data,TEXT("verification_uri")); QrUrl = Text(Data,TEXT("verification_uri_complete"));
        const double Seconds = Number(Data,TEXT("expires_in"));
        const double Interval = Number(Data,TEXT("interval"), 5);
        if (DeviceCode.IsEmpty() || DeviceCode.Len() > 4096 || UserCode.IsEmpty() || UserCode.Len() > 64
            || !VerificationUrl.StartsWith(Config.Authority, ESearchCase::CaseSensitive)
            || !QrUrl.StartsWith(Config.Authority, ESearchCase::CaseSensitive) || QrUrl.Len() > 2048
            || Seconds <= 0 || Seconds > 3600 || Interval < 1 || Interval > 300)
        { FinishSignInFailure(TEXT("The login service returned an invalid response.")); return; }
        PollInterval = FMath::CeilToInt(Interval); PollAt = FPlatformTime::Seconds() + PollInterval;
        AuthDeadline = FPlatformTime::Seconds() + Seconds;
        Status = TEXT("Scan the QR code and approve sign-in on your phone."); Touch();
    }, true, true);
}
void UOpenGolfOnlineSubsystem::SignInWithBrowser(bool bCreateAccount)
{
    if (!Config.IsValid()) return;
    CancelSignIn();
    Browser = MakeShared<FOpenGolfBrowserLogin>();
    if (!Browser->Start(Config, bCreateAccount)) { FinishSignInFailure(TEXT("Could not open browser sign-in. Try QR sign-in instead.")); return; }
    bSigningIn = true; AuthDeadline = FPlatformTime::Seconds() + 180;
    Status = TEXT("Complete sign-in in your browser."); Touch();
}
void UOpenGolfOnlineSubsystem::OpenVerificationPage() const
{
    if (!QrUrl.IsEmpty() && QrUrl.StartsWith(Config.Authority, ESearchCase::CaseSensitive)) FPlatformProcess::LaunchURL(*QrUrl, nullptr, nullptr);
}
void UOpenGolfOnlineSubsystem::PollDevice()
{
    const uint64 Attempt = AuthAttempt;
    PollAt = TNumericLimits<double>::Max();
    const FString Payload = Form({{TEXT("grant_type"),TEXT("urn:ietf:params:oauth:grant-type:device_code")},
        {TEXT("client_id"),Config.ClientId},{TEXT("device_code"),DeviceCode}});
    Send(Config.Authority + TEXT("oauth/token"), TEXT("POST"), Payload, {}, [this, Attempt](FReply Reply)
    {
        if (Attempt != AuthAttempt) return;
        const FString Error = Text(Object(Reply.Body), TEXT("error"));
        if (Success(Reply)) { AcceptTokens(Reply, Attempt, false); return; }
        if (Error == TEXT("slow_down")) PollInterval += 5;
        else if (Error != TEXT("authorization_pending") && Reply.bConnected && Reply.Status != 429 && Reply.Status < 500)
        { FinishSignInFailure(TEXT("QR sign-in was declined or expired. Try again.")); return; }
        PollAt = FPlatformTime::Seconds() + PollInterval;
    }, true, true);
}
void UOpenGolfOnlineSubsystem::AcceptTokens(const FReply& Reply, uint64 Attempt, bool bRefresh)
{
    const auto Data = Object(Reply.Body);
    const FString Access = Text(Data,TEXT("access_token")), RefreshValue = Text(Data,TEXT("refresh_token"));
    const double Life = Number(Data,TEXT("expires_in"));
    if (!Success(Reply) || !Data || Access.IsEmpty() || Access.Len() > 32768 || RefreshValue.IsEmpty() || RefreshValue.Len() > 8192
        || !Text(Data,TEXT("token_type")).Equals(TEXT("Bearer"), ESearchCase::IgnoreCase) || Life <= 0 || Life > 86400)
    {
        if (bRefresh)
        {
            InvalidateSession();
            auto Waiters = MoveTemp(RefreshWaiters); bRefreshing = false;
            for (auto& W : Waiters) W(false);
        }
        else FinishSignInFailure(TEXT("The login service did not provide a usable session."));
        return;
    }
    const uint64 SessionVersion = Epoch;
    const double Expires = FPlatformTime::Seconds() + Life;
    auto VerifyIdentity = [this, Attempt, SessionVersion, Access, RefreshValue, Expires, bRefresh]()
    {
    Send(Config.Authority + TEXT("userinfo"), TEXT("GET"), {}, Access,
        [this, Attempt, SessionVersion, Access, RefreshValue, Expires, bRefresh](FReply Identity)
    {
        if (SessionVersion != Epoch || (!bRefresh && Attempt != AuthAttempt)) return;
        const auto User = Object(Identity.Body);
        const FString Subject = Text(User,TEXT("sub"));
        if (!Success(Identity) || Subject.IsEmpty() || Subject.Len() > 128)
        {
            if (bRefresh)
            {
                // Preserve the newly rotated token even if /userinfo is temporarily unavailable.
                RefreshToken = RefreshValue; AccessToken.Empty(); TokenExpiresAt = 0;
                SaveCredentials(RefreshValue, [this](bool)
                {
                    bRefreshing = false; Status = TEXT("Offline. Your session is kept for retry."); RetryAt = FPlatformTime::Seconds() + 10; Touch();
                    auto Waiters = MoveTemp(RefreshWaiters); for (auto& W : Waiters) W(false);
                });
            }
            else FinishSignInFailure(TEXT("Could not verify your account. Try signing in again."));
            return;
        }
        const FString Key = AccountHash(Subject);
        if (bRefresh && !AccountKey.IsEmpty() && AccountKey != Key)
        {
            InvalidateSession(); bRefreshing = false;
            auto Waiters = MoveTemp(RefreshWaiters); for (auto& W : Waiters) W(false);
            return;
        }
        if (!bRefresh)
        {
            if (AccountKey != Key) OnAccountChanged.Broadcast();
            ++Epoch; ++CredentialWork->Epoch;
            for (auto& R : Requests) R->Cancel();
            bUploading = false; bLoading = false; bDeleting = false; bRefreshing = false;
            auto Waiters = MoveTemp(RefreshWaiters); for (auto& W : Waiters) W(false);
        }
        if (OutboxOwner != Key) { Outbox.Empty(); OutboxOwner = Key; }
        if (AccountKey != Key) { Profile.Reset(); Carry.Empty(); History.Empty(); HistoryClub.Empty(); }
        DisplayNameHint = Text(User,TEXT("name")).Left(40);
        AccountKey = Key; AccessToken = Access; RefreshToken = RefreshValue; TokenExpiresAt = Expires;
        if (!bRefresh) CancelSignIn();
        SaveLocal();
        SaveCredentials(RefreshValue, [this, bRefresh](bool bSaved)
        {
            if (!bRefresh && bSaved && IsSessionRestoreBlocked()
                && !IFileManager::Get().Delete(*(FPaths::GetPath(LocalPath()) / TEXT("signed-out")), false, true))
                StorageWarning = TEXT("Signed in for this session only. Automatic sign-in remains disabled.");
            Status = TEXT("Connected."); bRefreshing = false; RetryStep = 0; RetryAt = 0; Touch();
            if (bRefresh) { auto Waiters = MoveTemp(RefreshWaiters); for (auto& W : Waiters) W(true); }
            else SyncProfile();
        });
    }, false, !bRefresh);
    };
    if (bRefresh) SaveCredentials(RefreshValue, [VerifyIdentity = MoveTemp(VerifyIdentity)](bool) mutable { VerifyIdentity(); });
    else VerifyIdentity();
}
void UOpenGolfOnlineSubsystem::Refresh(TFunction<void(bool)> Done)
{
    if (!HasSession()) { Done(false); return; }
    RefreshWaiters.Add(MoveTemp(Done));
    if (bRefreshing) return;
    bRefreshing = true;
    const uint64 Version = Epoch;
    const FString Payload = Form({{TEXT("grant_type"),TEXT("refresh_token")},{TEXT("client_id"),Config.ClientId},{TEXT("refresh_token"),RefreshToken}});
    Send(Config.Authority + TEXT("oauth/token"), TEXT("POST"), Payload, {}, [this, Version](FReply Reply)
    {
        if (Version != Epoch) return;
        if (Success(Reply)) { AcceptTokens(Reply, AuthAttempt, true); return; }
        const FString Error = Text(Object(Reply.Body), TEXT("error"));
        if (Error == TEXT("invalid_grant") || Error == TEXT("invalid_client") || Error == TEXT("unauthorized_client")) InvalidateSession();
        else { Status = TEXT("Offline. Your session is kept for retry."); RetryAt = FPlatformTime::Seconds() + 10; Touch(); }
        bRefreshing = false;
        auto Waiters = MoveTemp(RefreshWaiters); for (auto& W : Waiters) W(false);
    }, true);
}
void UOpenGolfOnlineSubsystem::Api(const FString& Relative, const FString& Verb, const FString& Body, FCompletion Done, bool bRetried)
{
    const uint64 Version = Epoch;
    if (!HasSession()) { Done({401, {}, true}); return; }
    if (AccessToken.IsEmpty() || TokenExpiresAt <= FPlatformTime::Seconds() + 30)
    {
        Refresh([this, Version, Relative, Verb, Body, Done = MoveTemp(Done), bRetried](bool Ok) mutable
        {
            if (Version != Epoch) return;
            if (Ok) Api(Relative, Verb, Body, MoveTemp(Done), bRetried); else Done({});
        });
        return;
    }
    const FString SentToken = AccessToken;
    Send(Config.ApiOrigin + Config.MePath + Relative, Verb, Body, SentToken,
        [this, Version, SentToken, Relative, Verb, Body, Done = MoveTemp(Done), bRetried](FReply Reply) mutable
    {
        if (Version != Epoch) return;
        if (Reply.Status == 401 && !bRetried)
        {
            if (AccessToken != SentToken && !AccessToken.IsEmpty())
            { Api(Relative, Verb, Body, MoveTemp(Done), true); return; }
            Refresh([this, Version, Relative, Verb, Body, Done = MoveTemp(Done)](bool Ok) mutable
            {
                if (Version != Epoch) return;
                if (Ok) Api(Relative, Verb, Body, MoveTemp(Done), true); else Done({});
            });
        }
        else Done(MoveTemp(Reply));
    });
}
void UOpenGolfOnlineSubsystem::SyncProfile()
{
    if (!HasSession() || bLoading) return;
    bLoading = true; Touch();
    auto Body=MakeShared<FJsonObject>();
    if(DisplayNameHint.IsEmpty())Body->SetField(TEXT("display_name_hint"),MakeShared<FJsonValueNull>());
    else Body->SetStringField(TEXT("display_name_hint"),DisplayNameHint);
    Api(TEXT("/sync"), TEXT("POST"), Json(Body), [this](FReply Reply)
    {
        bLoading = false;
        auto Value = Object(Reply.Body);
        if (Success(Reply) && Value && !Text(Value,TEXT("id")).IsEmpty())
        {
            Profile = Value; Status = TEXT("Connected."); RetryStep = 0; RetryAt = 0; SaveLocal(); Flush();
        }
        else
        {
            Status = Reply.Status == 401 ? TEXT("The API refused this session. Try signing in again.") : TEXT("Offline. Your session and pending shots are kept.");
            static const double Delays[] = {3,10,30,60,120};
            RetryAt = FPlatformTime::Seconds() + Delays[FMath::Min(RetryStep++,4)];
        }
        Touch();
    });
}
void UOpenGolfOnlineSubsystem::Retry() { RetryAt = 0; if (HasSession()) SyncProfile(); }
void UOpenGolfOnlineSubsystem::InvalidateSession(bool bDiscardShots)
{
    if (bDiscardShots) OnAccountChanged.Broadcast();
    ++Epoch; if (CredentialWork) ++CredentialWork->Epoch;
    AccessToken.Empty(); RefreshToken.Empty(); DisplayNameHint.Empty();
    if (bDiscardShots) { AccountKey.Empty(); OutboxOwner.Empty(); Outbox.Empty(); }
    bRestoring = false;
    Profile.Reset(); Carry.Empty(); History.Empty(); HistoryClub.Empty();
    ++HistoryRevision; OnHistoryChanged.Broadcast();
    bUploading = false; bLoading = false; bDeleting = false; bCarryRequested = false; bHistoryRequested = false; PendingDelete.Empty();
    SaveLocal(); SaveCredentials({}, [](bool){});
    Status = TEXT("Session ended. Sign in again."); Touch();
}
void UOpenGolfOnlineSubsystem::SignOut()
{
    if (!BlockSessionRestore())
    {
        Status = TEXT("Could not finish signing out on this computer. Check storage and try again."); Touch(); return;
    }
    bSigningOut = true; Touch();
    OnBeforeReadProfile.Broadcast();
    const FString OldRefresh = RefreshToken, OldAccess = AccessToken;
    FString FinalBatch;
    if (!bUploading && TokenExpiresAt > FPlatformTime::Seconds() && AccountKey == OutboxOwner && !Outbox.IsEmpty())
    {
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (int32 I=0; I<FMath::Min(100,Outbox.Num()); ++I) Rows.Add(Outbox[I]);
        auto Body = MakeShared<FJsonObject>(); Body->SetArrayField(TEXT("shots"),Rows); FinalBatch=Json(Body);
    }
    CancelSignIn();
    for (auto& R : Requests) R->Cancel();
    auto Waiters = MoveTemp(RefreshWaiters); bRefreshing = false;
    InvalidateSession(true); bSigningOut = false; Status = TEXT("Signed out."); Touch();
    for (auto& W : Waiters) W(false);
    if (!FinalBatch.IsEmpty()) Send(Config.ApiOrigin+Config.MePath+TEXT("/range-shots/batch"),TEXT("POST"),FinalBatch,OldAccess,[](FReply){});
    if (!OldRefresh.IsEmpty())
        Send(Config.Authority + TEXT("oauth/revoke"), TEXT("POST"), Form({{TEXT("client_id"),Config.ClientId},{TEXT("token"),OldRefresh}}), {}, [](FReply){}, true);
}
bool UOpenGolfOnlineSubsystem::Tick(float)
{
    if (bStopped) return false;
    const double Now = FPlatformTime::Seconds();
    Requests.RemoveAll([](const auto& R){return R->Finished.load();});
    AuthRequests.RemoveAll([](const auto& R){return R->Finished.load();});
    if (bSigningIn)
    {
        if (Now >= AuthDeadline) FinishSignInFailure(TEXT("Sign-in expired. Try again."));
        else if (Browser)
        {
            FString Code, Error;
            if (Browser->Poll(Code, Error))
            {
                const FString Verifier = Browser->Verifier, Redirect = Browser->Redirect;
                const uint64 Attempt = AuthAttempt; CloseBrowser();
                if (!Error.IsEmpty()) FinishSignInFailure(Error);
                else
                {
                    Status = TEXT("Completing sign-in..."); Touch();
                    Send(Config.Authority + TEXT("oauth/token"), TEXT("POST"), Form({{TEXT("grant_type"),TEXT("authorization_code")},
                        {TEXT("client_id"),Config.ClientId},{TEXT("code"),Code},{TEXT("code_verifier"),Verifier},{TEXT("redirect_uri"),Redirect}}), {},
                        [this, Attempt](FReply Reply){ if (Attempt == AuthAttempt) AcceptTokens(Reply,Attempt,false); }, true,true);
                }
            }
        }
        else if (!DeviceCode.IsEmpty() && Now >= PollAt) PollDevice();
    }
    if (HasSession() && !bSigningIn && !bRefreshing && !bLoading && RetryAt > 0 && Now >= RetryAt) { RetryAt = 0; SyncProfile(); }
    if (HasSession() && !bUploading && !bDeleting && UploadAt > 0 && Now >= UploadAt) { UploadAt = 0; Flush(); }
    ProcessPlayerRequests();
    if (bLocalWriteFailed && Now >= NextLocalRetry) { NextLocalRetry = Now + 10; SaveLocal(); }
    return true;
}

#if WITH_DEV_AUTOMATION_TESTS
void UOpenGolfOnlineSubsystem::SetupForTests(const FConfig& TestConfig, FTestSender Sender, const FString& LocalRoot)
{
    if (CredentialWork) ++CredentialWork->Epoch;
    Config=TestConfig; TestSender=MoveTemp(Sender); TestLocalRoot=LocalRoot;
    CredentialWork=MakeShared<FOpenGolfCredentialWork,ESPMode::ThreadSafe>();
}
#endif
