// SPDX-License-Identifier: MIT
#include "OnlineTransport.h"
#include "OpenGolfOnlineConfig.h"
#include "Async/Async.h"
#include "HttpModule.h"
#include "SslModule.h"
#include "Interfaces/ISslCertificateManager.h"
#include "curl/curl.h"

namespace OpenGolfOnline
{
    bool IsAllowedRequest(const FConfig& Config, const FString& Url, const FString& Verb, const FString& AccessToken)
    {
        for (TCHAR C : Url) if (C <= 32 || C >= 127 || C == '\\' || C == '#') return false;
        for (TCHAR C : AccessToken) if (C <= 32 || C >= 127) return false;
        if (!Config.IsValid()) return false;
        const FString Auth = Config.Authority;
        if (Url == Auth + TEXT("oauth/device/code") || Url == Auth + TEXT("oauth/token") || Url == Auth + TEXT("oauth/revoke"))
            return Verb == TEXT("POST") && AccessToken.IsEmpty();
        if (Url == Auth + TEXT("userinfo")) return Verb == TEXT("GET") && !AccessToken.IsEmpty();
        const FString Base = Config.ApiOrigin + Config.MePath;
        if (AccessToken.IsEmpty()) return false;
        if (Url == Base) return Verb == TEXT("GET") || Verb == TEXT("PATCH");
        if (Verb == TEXT("POST"))
            return Url == Base + TEXT("/sync") || Url == Base + TEXT("/range-shots/batch") || Url == Base + TEXT("/range-shots/delete");
        if (Verb != TEXT("GET")) return false;
        FString Path, Query;
        if (!Url.Split(TEXT("?"), &Path, &Query)) return false;
        if (Path == Base + TEXT("/clubs/carry")) return Query == TEXT("days=90");
        static const TCHAR* Keys[] = {TEXT("dr"),TEXT("3w"),TEXT("5w"),TEXT("7w"),TEXT("4i"),TEXT("5i"),TEXT("6i"),TEXT("7i"),TEXT("8i"),TEXT("9i"),TEXT("pw"),TEXT("gw"),TEXT("sw")};
        for (const TCHAR* Key : Keys)
            if (Path == Base + TEXT("/clubs/") + Key + TEXT("/shots")) return Query == TEXT("days=90&limit=200");
        return false;
    }

    namespace
    {
        constexpr int32 MaxResponseBytes = 2 * 1024 * 1024;
        constexpr int32 MaxRequestBytes = 1024 * 1024;
        size_t Receive(char* Bytes, size_t Size, size_t Count, void* Destination)
        {
            auto& Buffer = *static_cast<TArray<uint8>*>(Destination);
            if (Size && Count > static_cast<size_t>(MaxResponseBytes) / Size) return 0;
            const size_t Length = Size * Count;
            if (Length > static_cast<size_t>(MaxResponseBytes - Buffer.Num())) return 0;
            Buffer.Append(reinterpret_cast<uint8*>(Bytes), static_cast<int32>(Length));
            return Length;
        }
        int Progress(void* State, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
        {
            return static_cast<FRequestCancellation*>(State)->Cancelled.load() ? 1 : 0;
        }
        CURLcode Certificates(CURL*, void* Context, void* Manager)
        {
            static_cast<ISslCertificateManager*>(Manager)->AddCertificatesToSslContext(static_cast<SSL_CTX*>(Context));
            return CURLE_OK;
        }
    }

    FCancelHandle Request(const FConfig& Config, const FString& Url, const FString& Verb, const FString& Body,
        const FString& AccessToken, FHttpCallback Completion, bool bForm)
    {
        check(IsInGameThread());
        auto Cancel = MakeShared<FRequestCancellation, ESPMode::ThreadSafe>();
        if (!IsAllowedRequest(Config, Url, Verb, AccessToken) || Body.Len() > MaxRequestBytes)
        {
            Cancel->Finished.store(true);
            AsyncTask(ENamedThreads::GameThread, [Completion = MoveTemp(Completion)]() mutable { Completion({}); });
            return Cancel;
        }
        FHttpModule::Get();
        auto* Manager = &FSslModule::Get().GetCertificateManager();
        Async(EAsyncExecution::ThreadPool, [Url, Verb, Body, AccessToken, bForm, Cancel, Manager, Completion = MoveTemp(Completion)]() mutable
        {
            FHttpResult Result;
            TArray<uint8> Bytes;
            CURL* Handle = curl_easy_init();
            if (Handle && !Cancel->Cancelled.load())
            {
                FTCHARToUTF8 Address(*Url), Method(*Verb), Payload(*Body);
                curl_slist* Headers = nullptr;
                Headers = curl_slist_append(Headers, "Accept: application/json");
                Headers = curl_slist_append(Headers, bForm ? "Content-Type: application/x-www-form-urlencoded" : "Content-Type: application/json");
                if (!AccessToken.IsEmpty()) Headers = curl_slist_append(Headers, TCHAR_TO_UTF8(*(TEXT("Authorization: Bearer ") + AccessToken)));
                curl_easy_setopt(Handle, CURLOPT_URL, Address.Get());
                curl_easy_setopt(Handle, CURLOPT_CUSTOMREQUEST, Method.Get());
                curl_easy_setopt(Handle, CURLOPT_HTTPHEADER, Headers);
                curl_easy_setopt(Handle, CURLOPT_USERAGENT, "OpenGolfOnline/1");
                curl_easy_setopt(Handle, CURLOPT_FOLLOWLOCATION, 0L);
                curl_easy_setopt(Handle, CURLOPT_PROTOCOLS_STR, "https");
                curl_easy_setopt(Handle, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
                curl_easy_setopt(Handle, CURLOPT_SSL_VERIFYPEER, 1L);
                curl_easy_setopt(Handle, CURLOPT_SSL_VERIFYHOST, 2L);
                curl_easy_setopt(Handle, CURLOPT_SSL_CTX_FUNCTION, Certificates);
                curl_easy_setopt(Handle, CURLOPT_SSL_CTX_DATA, Manager);
                curl_easy_setopt(Handle, CURLOPT_NOSIGNAL, 1L);
                curl_easy_setopt(Handle, CURLOPT_CONNECTTIMEOUT, 10L);
                curl_easy_setopt(Handle, CURLOPT_TIMEOUT, 20L);
                curl_easy_setopt(Handle, CURLOPT_WRITEFUNCTION, Receive);
                curl_easy_setopt(Handle, CURLOPT_WRITEDATA, &Bytes);
                curl_easy_setopt(Handle, CURLOPT_NOPROGRESS, 0L);
                curl_easy_setopt(Handle, CURLOPT_XFERINFOFUNCTION, Progress);
                curl_easy_setopt(Handle, CURLOPT_XFERINFODATA, &Cancel.Get());
                if (Verb != TEXT("GET"))
                {
                    curl_easy_setopt(Handle, CURLOPT_POSTFIELDS, Payload.Get());
                    curl_easy_setopt(Handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(Payload.Length()));
                }
                if (Payload.Length() <= MaxRequestBytes && curl_easy_perform(Handle) == CURLE_OK)
                {
                    long Status = 0;
                    curl_easy_getinfo(Handle, CURLINFO_RESPONSE_CODE, &Status);
                    if (Status < 300 || Status >= 400)
                    {
                        Result.bConnected = true;
                        Result.Status = static_cast<int32>(Status);
                        if (!Bytes.IsEmpty())
                        {
                            const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()), Bytes.Num());
                            Result.Body = FString(Text.Length(), Text.Get());
                        }
                    }
                }
                curl_slist_free_all(Headers);
            }
            if (Handle) curl_easy_cleanup(Handle);
            Cancel->Finished.store(true);
            AsyncTask(ENamedThreads::GameThread, [Cancel, Completion = MoveTemp(Completion), Result = MoveTemp(Result)]() mutable
            {
                Completion(Cancel->Cancelled.load() ? FHttpResult{} : MoveTemp(Result));
            });
        });
        return Cancel;
    }
}
