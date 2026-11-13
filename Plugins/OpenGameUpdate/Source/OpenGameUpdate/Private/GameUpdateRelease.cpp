// SPDX-License-Identifier: MIT
#include "GameUpdateRelease.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace GameUpdate
{
    bool ParseGitHubRelease(const FString& Json, const FGameUpdateConfig& Config, FGameUpdateRelease& Out, FString& Error)
    {
        TSharedPtr<FJsonObject> Root;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
        {
            Error = TEXT("The update server sent an unreadable reply.");
            return false;
        }
        FString Tag;
        if (!Root->TryGetStringField(TEXT("tag_name"), Tag) || Tag.IsEmpty())
        {
            Error = TEXT("The update server's reply has no release version.");
            return false;
        }
        FGameUpdateRelease R;
        R.Version = Tag.StartsWith(TEXT("v")) ? Tag.RightChop(1) : Tag;
        Root->TryGetStringField(TEXT("html_url"), R.PageUrl);
        Root->TryGetStringField(TEXT("body"), R.Notes);

        const FString Installer = Config.AssetName(R.Version), Checksum = Installer + TEXT(".sha256");
        const TArray<TSharedPtr<FJsonValue>>* Assets = nullptr;
        if (Root->TryGetArrayField(TEXT("assets"), Assets))
            for (const TSharedPtr<FJsonValue>& Value : *Assets)
            {
                const TSharedPtr<FJsonObject>* Asset = nullptr;
                FString Name, Url;
                if (!Value->TryGetObject(Asset) || !(*Asset)->TryGetStringField(TEXT("name"), Name)
                    || !(*Asset)->TryGetStringField(TEXT("browser_download_url"), Url) || !Url.StartsWith(TEXT("https://")))
                    continue;
                if (Name == Installer)
                {
                    R.InstallerName = Name;
                    R.InstallerUrl = Url;
                    (*Asset)->TryGetNumberField(TEXT("size"), R.InstallerSize);
                }
                else if (Name == Checksum)
                    R.ChecksumUrl = Url;
            }
        Out = MoveTemp(R);
        return true;
    }

    FString ParseChecksum(const FString& Text, const FString& FileName)
    {
        auto IsHash = [](const FString& Value)
        {
            if (Value.Len() != 64) return false;
            for (TCHAR C : Value) if (!FChar::IsHexDigit(C)) return false;
            return true;
        };
        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines);
        for (const FString& Line : Lines)
        {
            FString Hash, Name;
            if (!Line.TrimStartAndEnd().Split(TEXT(" "), &Hash, &Name)) Hash = Line.TrimStartAndEnd();
            // sha256sum marks binary-mode names with a leading '*'.
            Name.TrimStartInline();
            Name.RemoveFromStart(TEXT("*"));
            if (IsHash(Hash) && (Name.IsEmpty() ? Lines.Num() == 1 : Name == FileName)) return Hash.ToLower();
        }
        return {};
    }
}
