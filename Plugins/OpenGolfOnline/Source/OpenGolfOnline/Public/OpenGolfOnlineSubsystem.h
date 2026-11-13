// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "OnlineTransport.h"
#include "TokenStore.h"
#include "OpenGolfOnlineSubsystem.generated.h"

class FOpenGolfBrowserLogin;
struct FOpenGolfCredentialWork;
class FOpenGolfOnlineSessionTest;
class FOpenGolfPanelHeadingTest;

enum class EOpenGolfAccountState : uint8
{
    Restoring, SignedOut, SigningIn, LoadingProfile, Ready, Reconnecting, SigningOut
};

UCLASS()
class OPENGOLFONLINE_API UOpenGolfOnlineSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
#if WITH_DEV_AUTOMATION_TESTS
    using FTestSender = TFunction<OpenGolfOnline::FCancelHandle(const FString&, const FString&, const FString&, const FString&, OpenGolfOnline::FHttpCallback)>;
    void AdvanceForTests() { Tick(0); }
    void SetupForTests(const OpenGolfOnline::FConfig& TestConfig, FTestSender Sender, const FString& LocalRoot);
#endif
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    FSimpleMulticastDelegate OnAccountStateChanged, OnProfileChanged, OnHistoryChanged, OnStatusChanged;
    EOpenGolfAccountState GetAccountState() const;
    uint64 GetHistoryRevision() const { return HistoryRevision; }
    FSimpleMulticastDelegate OnBeforeReadProfile;
    FSimpleMulticastDelegate OnAccountChanged;
    void SignInWithQr();
    void SignInWithBrowser(bool bCreateAccount = false);
    void CancelSignIn();
    void SignOut();
    void Retry();
    void LoadCarry();
    void LoadHistory(const FString& Club);
    void UpdateDisplayName(const FString& Name);
    void DeleteShot(const FString& Id);
    FString ExportHistory();
    bool QueueShot(const TSharedPtr<FJsonObject>& Shot, const FString& Owner);
    void Flush();
    bool HasSession() const { return !RefreshToken.IsEmpty(); }
    bool IsSigningIn() const { return bSigningIn; }
    bool IsBusy() const { return bRefreshing || bLoading || bUploading || bDeleting; }
    bool IsConfigured() const { return Config.IsValid(); }
    const FString& GetAccountKey() const { return AccountKey; }
    const FString& GetStatus() const { return Status; }
    const FString& GetStorageWarning() const { return StorageWarning; }
    const FString& GetUserCode() const { return UserCode; }
    const FString& GetVerificationUrl() const { return VerificationUrl; }
    const FString& GetQrUrl() const { return QrUrl; }
    const FString& GetHistoryClub() const { return HistoryClub; }
    const FString& GetApplicationName() const { return Config.ApplicationName; }
    TSharedPtr<FJsonObject> GetProfile() const { return Profile; }
    const TArray<TSharedPtr<FJsonValue>>& GetCarry() const { return Carry; }
    const TArray<TSharedPtr<FJsonValue>>& GetHistory() const { return History; }
    int32 GetPendingCount() const { return Outbox.Num(); }
    int32 GetRejectedCount() const { return RejectedCount; }
    uint64 GetRevision() const { return Revision; }
    void OpenVerificationPage() const;

private:
    friend class FOpenGolfOnlineSessionTest;
    friend class FOpenGolfPanelHeadingTest;
#if WITH_DEV_AUTOMATION_TESTS
    FTestSender TestSender;
    TFunction<void(TFunction<void()>)> TestCredentialDispatch;
    TFunction<OpenGolfOnline::EStoreResult(const FString&,const FString&)> TestCredentialWriter;
    FString TestLocalRoot;
#endif
    using FReply = OpenGolfOnline::FHttpResult;
    using FCompletion = OpenGolfOnline::FHttpCallback;
    bool Tick(float Delta);
    void Touch();
    void Send(const FString& Url, const FString& Verb, const FString& Body, const FString& Token,
        FCompletion Completion, bool bForm = false, bool bAuth = false);
    void PollDevice();
    void AcceptTokens(const FReply& Reply, uint64 Attempt, bool bRefresh);
    void SaveCredentials(const FString& Token, TFunction<void(bool)> Done);
    void Refresh(TFunction<void(bool)> Done);
    void Api(const FString& Relative, const FString& Verb, const FString& Body, FCompletion Done, bool bRetried = false);
    void SyncProfile();
    void UploadBatch(int32 Count);
    void InvalidateSession(bool bDiscardShots = false);
    bool BlockSessionRestore();
    bool IsSessionRestoreBlocked() const;
    void LoadLocal();
    bool SaveLocal();
    void CloseBrowser();
    void ProcessPlayerRequests();
    void FinishSignInFailure(const FString& Message);
    FString LocalPath() const;
    OpenGolfOnline::FConfig Config;
    FTSTicker::FDelegateHandle TickHandle;
    TArray<TSharedPtr<OpenGolfOnline::FRequestCancellation, ESPMode::ThreadSafe>> Requests, AuthRequests;
    TSharedPtr<FOpenGolfCredentialWork, ESPMode::ThreadSafe> CredentialWork;
    TSharedPtr<FOpenGolfBrowserLogin> Browser;
    FString AccessToken, RefreshToken, AccountKey, Status, StorageWarning, DisplayNameHint;
    FString DeviceCode, UserCode, VerificationUrl, QrUrl, HistoryClub, PendingDelete;
    FString OutboxOwner;
    TSharedPtr<FJsonObject> Profile;
    TArray<TSharedPtr<FJsonValue>> Carry, History, Outbox;
    TArray<TFunction<void(bool)>> RefreshWaiters;
    double TokenExpiresAt = 0, PollAt = 0, AuthDeadline = 0, UploadAt = 0, RetryAt = 0, NextLocalRetry = 0;
    int32 PollInterval = 5, RetryStep = 0, RejectedCount = 0, UploadLimit = 100;
    bool bRestoring = false, bSigningOut = false;
    EOpenGolfAccountState LastAccountState = EOpenGolfAccountState::SignedOut;
    TSharedPtr<FJsonObject> LastProfile;
    FString LastStatus, LastStorageWarning;
    uint64 HistoryRevision = 0;
    uint64 Epoch = 0, AuthAttempt = 0, Revision = 0, HistoryRequest = 0;
    bool bSigningIn = false, bRefreshing = false, bLoading = false, bUploading = false, bDeleting = false;
    bool bCarryRequested = false, bHistoryRequested = false;
    bool bLocalWriteFailed = false, bStopped = false;
};
