// SPDX-License-Identifier: MIT
#include "BrowserLogin.h"
#include "OnlineJson.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Base64.h"
#define UI OPENSSL_UI
#include "openssl/rand.h"
#include "openssl/sha.h"
#undef UI

namespace
{
    FString RandomValue()
    {
        uint8 Bytes[32];
        if (RAND_bytes(Bytes, sizeof(Bytes)) != 1) return {};
        FString Result = FBase64::Encode(Bytes, sizeof(Bytes));
        return Result.Replace(TEXT("+"), TEXT("-")).Replace(TEXT("/"), TEXT("_")).Replace(TEXT("="), TEXT(""));
    }
}
void FOpenGolfBrowserLogin::CloseClient()
{
    if (Client) { Client->Close(); ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Client); Client = nullptr; }
    Input.Empty();
}
FOpenGolfBrowserLogin::~FOpenGolfBrowserLogin()
{
    CloseClient();
    if (Listener) { Listener->Close(); ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Listener); }
}
bool FOpenGolfBrowserLogin::Start(const OpenGolfOnline::FConfig& Config, bool bCreateAccount)
{
    Verifier = RandomValue(); State = RandomValue();
    if (Verifier.IsEmpty() || State.IsEmpty()) return false;
    auto* Sockets = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    for (int32 Port : Config.CallbackPorts)
    {
        Listener = Sockets->CreateSocket(NAME_Stream, TEXT("Native login callback"), false);
        if (!Listener) continue;
        auto Address = Sockets->CreateInternetAddr();
        bool bValid = false; Address->SetIp(TEXT("127.0.0.1"), bValid); Address->SetPort(Port);
        Listener->SetNonBlocking(true);
        if (bValid && Listener->Bind(*Address) && Listener->Listen(4))
        {
            Redirect = FString::Printf(TEXT("http://127.0.0.1:%d/callback"), Port);
            break;
        }
        Listener->Close(); Sockets->DestroySocket(Listener); Listener = nullptr;
    }
    if (!Listener) return false;
    uint8 Hash[SHA256_DIGEST_LENGTH];
    const FTCHARToUTF8 Bytes(*Verifier);
    SHA256(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length(), Hash);
    const FString Challenge = FBase64::Encode(Hash, sizeof(Hash)).Replace(TEXT("+"), TEXT("-")).Replace(TEXT("/"), TEXT("_")).Replace(TEXT("="), TEXT(""));
    TArray<TPair<FString,FString>> Args = {{TEXT("response_type"),TEXT("code")}, {TEXT("client_id"),Config.ClientId},
        {TEXT("audience"),Config.Audience}, {TEXT("redirect_uri"),Redirect}, {TEXT("scope"),TEXT("openid profile offline_access")},
        {TEXT("state"),State}, {TEXT("code_challenge"),Challenge}, {TEXT("code_challenge_method"),TEXT("S256")},
        // Always show the login page. Without this, a browser still logged in to the login service approves the
        // sign-in silently for its last user, and a player who signed out could never switch accounts.
        {TEXT("prompt"),TEXT("login")}};
    if (bCreateAccount) Args.Add({TEXT("screen_hint"),TEXT("signup")});
    const FString Url = Config.Authority + TEXT("authorize?") + OpenGolfOnline::Form(Args);
    FString Error;
    FPlatformProcess::LaunchURL(*Url, nullptr, &Error);
    return Error.IsEmpty();
}
bool FOpenGolfBrowserLogin::Poll(FString& Code, FString& Error)
{
    bool Pending = false;
    if (!Client && Listener && Listener->HasPendingConnection(Pending) && Pending)
    {
        Client = Listener->Accept(TEXT("Native login"));
        if (Client) { Client->SetNonBlocking(true); ConnectedAt = FPlatformTime::Seconds(); }
    }
    if (!Client) return false;
    if (FPlatformTime::Seconds() - ConnectedAt > 2) { CloseClient(); return false; }
    uint8 Bytes[1024]; int32 Read = 0;
    while (Client->Recv(Bytes, sizeof(Bytes), Read) && Read > 0)
    {
        const FUTF8ToTCHAR Text(reinterpret_cast<const ANSICHAR*>(Bytes), Read);
        Input.AppendChars(Text.Get(), Text.Length());
        if (Input.Len() > 8192) { CloseClient(); return false; }
    }
    if (!Input.Contains(TEXT("\r\n\r\n"))) return false;
    FString Line, Rest; Input.Split(TEXT("\r\n"), &Line, &Rest);
    const bool Accepted = ParseCallback(Line,State,Code,Error);
    const ANSICHAR* Reply = Accepted ? "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nCache-Control: no-store\r\nConnection: close\r\n\r\nSign-in received. Return to your game."
        : "HTTP/1.1 400 Bad Request\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nUnrecognized login callback.";
    int32 Sent = 0; Client->Send(reinterpret_cast<const uint8*>(Reply), FCStringAnsi::Strlen(Reply), Sent);
    CloseClient();
    if (!Accepted) return false;
    return true;
}

bool FOpenGolfBrowserLogin::ParseCallback(const FString& RequestLine,const FString& ExpectedState,FString& Code,FString& Error)
{
    Code.Empty(); Error.Empty();
    TArray<FString> Parts; RequestLine.ParseIntoArray(Parts, TEXT(" "));
    bool Accepted = Parts.Num() == 3 && Parts[0] == TEXT("GET") && Parts[1].StartsWith(TEXT("/callback?"));
    TMap<FString, FString> Query;
    if (Accepted)
    {
        TArray<FString> Pairs; Parts[1].RightChop(10).ParseIntoArray(Pairs, TEXT("&"));
        for (const FString& Pair : Pairs)
        {
            FString Key, Value;
            if (!Pair.Split(TEXT("="), &Key, &Value)) { Accepted = false; break; }
            Key = FGenericPlatformHttp::UrlDecode(Key);
            if (Query.Contains(Key)) { Accepted = false; break; }
            Query.Add(Key, FGenericPlatformHttp::UrlDecode(Value));
        }
        Accepted &= Query.FindRef(TEXT("state")) == ExpectedState;
    }
    if (!Accepted || ExpectedState.IsEmpty()) return false;
    Code = Query.FindRef(TEXT("code"));
    if (Code.IsEmpty()) Error = TEXT("Sign-in was declined or did not return a code.");
    return true;
}
