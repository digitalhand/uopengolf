// SPDX-License-Identifier: MIT
#include "OpenGolfOnlineConfig.h"
#include "Misc/ConfigCacheIni.h"

namespace OpenGolfOnline
{
    static bool HttpsHost(FString Value)
    {
        if (!Value.RemoveFromStart(TEXT("https://"), ESearchCase::CaseSensitive) || Value.IsEmpty()) return false;
        Value.RemoveFromEnd(TEXT("/"));
        for (TCHAR C : Value)
            if (!((C >= 'a' && C <= 'z') || (C >= 'A' && C <= 'Z') || (C >= '0' && C <= '9') || C == '.' || C == '-')) return false;
        return Value.Contains(TEXT(".")) && !Value.StartsWith(TEXT(".")) && !Value.EndsWith(TEXT("."));
    }
    bool FConfig::IsValid() const
    {
        if (!HttpsHost(Authority) || !Authority.EndsWith(TEXT("/")) || !HttpsHost(ApiOrigin)
            || ApiOrigin.EndsWith(TEXT("/")) || ClientId.IsEmpty() || ClientId.Len() > 128
            || !Audience.StartsWith(TEXT("https://")) || ApplicationName.IsEmpty() || StorageName.IsEmpty() || StorageName.Len() > 64
            || CallbackPorts.IsEmpty() || MePath != TEXT("/api/v1/ogolf/me")) return false;
        for (TCHAR C : ClientId) if (!FChar::IsAlnum(C) && C != '-' && C != '_') return false;
        for (TCHAR C : StorageName) if (!FChar::IsAlnum(C) || C > 127) { if (C != '-' && C != '_') return false; }
        for (TCHAR C : Audience) if (C <= 32 || C >= 127) return false;
        for (int32 Port : CallbackPorts) if (Port < 1024 || Port > 65535) return false;
        return true;
    }
    FConfig FConfig::Load()
    {
        FConfig C;
        const TCHAR* Section = TEXT("OpenGolfOnline");
        GConfig->GetString(Section, TEXT("Authority"), C.Authority, GGameIni);
        GConfig->GetString(Section, TEXT("ClientId"), C.ClientId, GGameIni);
        GConfig->GetString(Section, TEXT("Audience"), C.Audience, GGameIni);
        GConfig->GetString(Section, TEXT("ApiOrigin"), C.ApiOrigin, GGameIni);
        GConfig->GetString(Section, TEXT("ApplicationName"), C.ApplicationName, GGameIni);
        GConfig->GetString(Section, TEXT("StorageName"), C.StorageName, GGameIni);
        TArray<FString> Ports;
        GConfig->GetArray(Section, TEXT("CallbackPorts"), Ports, GGameIni);
        for (const FString& Port : Ports) C.CallbackPorts.Add(FCString::Atoi(*Port));
        return C;
    }
}
