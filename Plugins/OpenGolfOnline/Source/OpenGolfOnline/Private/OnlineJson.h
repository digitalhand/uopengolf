// SPDX-License-Identifier: MIT
#pragma once
#include "Dom/JsonObject.h"
#include "OnlineTransport.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "GenericPlatform/GenericPlatformHttp.h"

namespace OpenGolfOnline
{
    inline bool SuccessReply(const FHttpResult& R) { return R.bConnected && R.Status >= 200 && R.Status < 300; }
    inline FString Json(const TSharedRef<FJsonObject>& Object)
    {
        FString Text;
        FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text));
        return Text;
    }
    inline TSharedPtr<FJsonObject> Object(const FString& Text)
    {
        TSharedPtr<FJsonObject> Result;
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Result, FJsonSerializer::EFlags::StoreNumbersAsStrings);
        return Result;
    }
    inline FString Text(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key)
    {
        FString Value;
        if (Object) Object->TryGetStringField(Key, Value);
        return Value;
    }
    inline double Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Key, double Default = 0)
    {
        double Value;
        return Object && Object->TryGetNumberField(Key, Value) && FMath::IsFinite(Value) ? Value : Default;
    }
    inline FString Form(const TArray<TPair<FString, FString>>& Values)
    {
        FString Result;
        for (const auto& Value : Values)
        {
            if (!Result.IsEmpty()) Result += TEXT("&");
            Result += FGenericPlatformHttp::UrlEncode(Value.Key) + TEXT("=") + FGenericPlatformHttp::UrlEncode(Value.Value);
        }
        return Result;
    }
}
