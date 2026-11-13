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
    /**
     * Keeps a QR sign-in code ready while it is called at least every QrLeaseSeconds, as a widget showing the code
     * does each tick: requests a code, polls for approval and replaces a code that expired or was declined. When the
     * calls stop, so does the polling; a code that is still valid is reused when they resume.
     */
    void KeepQrSignInReady();
    static constexpr double QrLeaseSeconds = 2.0;
    /** Opens the login page in the system browser. It always asks for credentials, so a different account can sign in. */
    void SignInWithBrowser(bool bCreateAccount = false);
    /** Cancels a browser sign-in. A QR code that is ready stays ready. */
    void CancelSignIn();
    void SignOut();
    void Retry();
    void LoadCarry();
    void LoadHistory(const FString& Club);
    /** Completion reports server acceptance; failed saves leave the current profile unchanged. */
    void UpdateDisplayName(const FString& Name, TFunction<void(bool)> Completion = {});
    void DeleteShot(const FString& Id);
    FString ExportHistory();
    bool QueueShot(const TSharedPtr<FJsonObject>& Shot, const FString& Owner);
    void Flush();
    bool HasPermission(const FString& Permission) const;
    FSimpleMulticastDelegate OnPermissionsChanged;
    bool HasSession() const { return !RefreshToken.IsEmpty(); }
    /** A browser sign-in is open, or an approved sign-in (browser or QR) is being completed. */
    bool IsSigningIn() const { return bSigningIn; }
    /** The last request for a QR code failed, such as while offline; it is retried. */
    bool IsQrSignInFailing() const { return bQrFailed; }
    bool IsBusy() const { return bRefreshing || bLoading || bUploading || bDeleting; }
    bool IsConfigured() const { return Config.IsValid(); }
    const FString& GetAccountKey() const { return AccountKey; }
    const FString& GetStatus() const { return Status; }
    const FString& GetStorageWarning() const { return StorageWarning; }
    /** The short code the login page shows for the player to compare; empty without a QR code. */
    const FString& GetUserCode() const { return UserCode; }
    /** The address the QR code encodes; empty until KeepQrSignInReady has a code. */
    const FString& GetQrUrl() const { return QrUrl; }
    const FString& GetHistoryClub() const { return HistoryClub; }
    const FString& GetApplicationName() const { return Config.ApplicationName; }
    TSharedPtr<FJsonObject> GetProfile() const { return Profile; }
    const TArray<TSharedPtr<FJsonValue>>& GetCarry() const { return Carry; }
    const TArray<TSharedPtr<FJsonValue>>& GetHistory() const { return History; }
    int32 GetPendingCount() const { return Outbox.Num(); }
    int32 GetRejectedCount() const { return RejectedCount; }
    uint64 GetRevision() const { return Revision; }

private:
    friend class FOpenGolfOnlineSessionTest;
#if WITH_DEV_AUTOMATION_TESTS
    friend struct FOpenGolfPermissionTestAccess;
    friend class FOpenGolfPlayerSettingsTest;
#endif
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
    /** Requests, polls or renews the QR code while KeepQrSignInReady's lease lasts. */
    void MaintainQr();
    void RequestQr();
    void PollDevice();
    /** Forgets the QR code; replies to its requests are then ignored. */
    void DropQr();
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
    TArray<FString> Permissions;
    bool bLastAdminPermission = false;
    FString AccessToken, RefreshToken, AccountKey, Status, StorageWarning, DisplayNameHint;
    FString DeviceCode, UserCode, QrUrl, HistoryClub, PendingDelete;
    /** QR sign-in: the lease's end, the code's expiry and the earliest time for another request after a failure. */
    double QrWantedUntil = 0, QrExpiresAt = 0, QrRetryAt = 0;
    /** Bumped by DropQr, so replies for an earlier code are ignored. */
    uint64 QrAttempt = 0;
    bool bQrRequesting = false, bQrFailed = false;
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
