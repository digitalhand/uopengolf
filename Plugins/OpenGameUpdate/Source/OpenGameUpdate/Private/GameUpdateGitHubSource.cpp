// SPDX-License-Identifier: MIT
#include "GameUpdateSource.h"
#include "GameUpdateRelease.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

namespace GameUpdate
{
    namespace
    {
        class FGitHubSource final : public IGameUpdateSource, public TSharedFromThis<FGitHubSource>
        {
        public:
            explicit FGitHubSource(const FGameUpdateConfig& InConfig) : Config(InConfig) {}

            virtual void FetchLatest(FOnRelease OnDone) override
            {
                Cancel();
                const TSharedRef<IHttpRequest> Request = NewRequest(TEXT("https://api.github.com/repos/") + Config.Repository + TEXT("/releases/latest"));
                Request->SetHeader(TEXT("Accept"), TEXT("application/vnd.github+json"));
                Request->SetHeader(TEXT("X-GitHub-Api-Version"), TEXT("2022-11-28"));
                Request->SetTimeout(30);
                Send(Request, [this, OnDone](FHttpResponsePtr Response, bool bConnected)
                {
                    const int32 Code = Response ? Response->GetResponseCode() : 0;
                    FGameUpdateRelease Release;
                    FString Error;
                    if (Code != 200) { OnDone(false, Release, Explain(Code, bConnected)); return; }
                    if (!ParseGitHubRelease(Response->GetContentAsString(), Config, Release, Error)) { OnDone(false, Release, Error); return; }
                    if (!Release.HasInstaller() || Release.ChecksumUrl.IsEmpty()) { OnDone(true, Release, {}); return; }
                    FetchChecksum(MoveTemp(Release), OnDone);
                });
            }

            virtual void Download(const FString& Url, const FString& Path, FOnProgress OnProgress, FOnDone OnDone) override
            {
                Cancel();
                TSharedPtr<FArchive> File(IFileManager::Get().CreateFileWriter(*Path));
                if (!File) { OnDone(false, TEXT("Could not create the download file. Check free disk space.")); return; }
                const TSharedRef<IHttpRequest> Request = NewRequest(Url);
                Request->SetResponseBodyReceiveStream(File.ToSharedRef());
                // No limit on the whole download, only on a stalled one.
                Request->SetTimeout(0);
                Request->SetActivityTimeout(60);
                Request->OnRequestProgress64().BindLambda([Weak = AsWeak(), Generation = Generation, OnProgress](FHttpRequestPtr, uint64, uint64 Received)
                {
                    const TSharedPtr<FGitHubSource> This = Weak.Pin();
                    if (This && This->Generation == Generation) OnProgress(int64(Received));
                });
                Send(Request, [File, OnDone](FHttpResponsePtr Response, bool bConnected)
                {
                    const int32 Code = Response ? Response->GetResponseCode() : 0;
                    const bool bClosed = File->Close();
                    if (Code != 200) OnDone(false, Explain(Code, bConnected));
                    else if (!bClosed || File->IsError()) OnDone(false, TEXT("Could not save the download. Check free disk space."));
                    else OnDone(true, {});
                });
            }

            virtual void Cancel() override
            {
                ++Generation;
                if (Active) Active->CancelRequest();
                Active.Reset();
            }

        private:
            TSharedRef<IHttpRequest> NewRequest(const FString& Url) const
            {
                const TSharedRef<IHttpRequest> Request = FHttpModule::Get().CreateRequest();
                Request->SetURL(Url);
                Request->SetVerb(TEXT("GET"));
                Request->SetHeader(TEXT("User-Agent"), Config.UserAgent);
                return Request;
            }

            /** Starts Request; OnComplete runs unless the source was cancelled or destroyed meanwhile. */
            void Send(const TSharedRef<IHttpRequest>& Request, TFunction<void(FHttpResponsePtr, bool)> OnComplete)
            {
                Active = Request;
                Request->OnProcessRequestComplete().BindLambda(
                    [Weak = AsWeak(), Generation = Generation, OnComplete](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
                {
                    const TSharedPtr<FGitHubSource> This = Weak.Pin();
                    if (!This || This->Generation != Generation) return;
                    This->Active.Reset();
                    OnComplete(Response, bConnected);
                });
                Request->ProcessRequest();
            }

            void FetchChecksum(FGameUpdateRelease Release, FOnRelease OnDone)
            {
                const TSharedRef<IHttpRequest> Request = NewRequest(Release.ChecksumUrl);
                Request->SetTimeout(30);
                Send(Request, [Release = MoveTemp(Release), OnDone](FHttpResponsePtr Response, bool) mutable
                {
                    // Without a checksum the release is still reported; it just cannot be installed from the game.
                    if (Response && Response->GetResponseCode() == 200)
                        Release.Sha256 = ParseChecksum(Response->GetContentAsString(), Release.InstallerName);
                    OnDone(true, Release, {});
                });
            }

            static FString Explain(int32 Code, bool bConnected)
            {
                if (!bConnected || Code == 0) return TEXT("Could not reach GitHub. Check your internet connection.");
                if (Code == 404) return TEXT("No published release was found.");
                if (Code == 403 || Code == 429) return TEXT("GitHub is limiting requests. Try again in an hour.");
                return FString::Printf(TEXT("GitHub returned an error (HTTP %d). Try again later."), Code);
            }

            FGameUpdateConfig Config;
            TSharedPtr<IHttpRequest> Active;
            uint32 Generation = 0;
        };
    }

    TSharedRef<IGameUpdateSource> MakeGitHubSource(const FGameUpdateConfig& Config)
    {
        return MakeShared<FGitHubSource>(Config);
    }
}
