// SPDX-License-Identifier: MIT
#include "Profile/AvatarCatalog.h"
#include "Engine/Texture2D.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenGolfSettings.h"
#include "UObject/WeakObjectPtr.h"

namespace AvatarCatalog
{
    namespace
    {
        // Ids match the manifest in Plugins/OpenGolfSavanna/Scripts/FetchFluentEmoji.py.
        constexpr FAvatar Avatars[] = {
            {TEXT("Golfer"), TEXT("Golfer")},
            {TEXT("Flag"), TEXT("Flag in Hole")},
            {TEXT("Trophy"), TEXT("Trophy")},
            {TEXT("Birdie"), TEXT("Birdie")},
            {TEXT("Eagle"), TEXT("Eagle")},
            {TEXT("Bogey"), TEXT("Bogey")},
            {TEXT("DuckHook"), TEXT("Duck Hook")},
            {TEXT("Goat"), TEXT("G.O.A.T.")},
            {TEXT("SlowPlay"), TEXT("Slow Play")},
            {TEXT("Flamingo"), TEXT("Flamingo")},
            {TEXT("WaterHazard"), TEXT("Water Hazard")},
            {TEXT("Giraffe"), TEXT("Giraffe")},
            {TEXT("Zebra"), TEXT("Zebra")},
            {TEXT("Hippo"), TEXT("Hippo")},
            {TEXT("Elephant"), TEXT("Elephant")},
            {TEXT("Lion"), TEXT("Lion")},
            {TEXT("Rhino"), TEXT("Rhino")},
            {TEXT("GreenReader"), TEXT("Green Reader")},
            {TEXT("Shades"), TEXT("Shades")},
            {TEXT("Cowboy"), TEXT("Cowboy")},
            {TEXT("Incognito"), TEXT("Incognito")},
            {TEXT("SwingRobot"), TEXT("Swing Robot")},
            {TEXT("LongHitter"), TEXT("Long Hitter")},
            {TEXT("HoleInOne"), TEXT("Hole-in-One")},
        };
        const TCHAR* const Folder = TEXT("/OpenGolfSavanna/UI/Avatars");
    } // namespace

    TConstArrayView<FAvatar> All() { return Avatars; }

    const FAvatar* Find(const FString& Id)
    {
        if (Id.IsEmpty())
            return nullptr;
        for (const FAvatar& Avatar : Avatars)
            if (Id.Equals(Avatar.Id, ESearchCase::CaseSensitive))
                return &Avatar;
        return nullptr;
    }

    const FAvatar& Default() { return Avatars[0]; }

    FSoftObjectPath TexturePath(const FAvatar& Avatar)
    {
        const FString Name = FString(TEXT("T_Avatar_")) + Avatar.Id;
        return FSoftObjectPath(FString::Printf(TEXT("%s/%s.%s"), Folder, *Name, *Name));
    }

    UTexture2D* LoadTexture(const FAvatar& Avatar)
    {
        // Weak, so nothing is held past the object system's shutdown; widgets keep what they show alive.
        static TMap<const FAvatar*, TWeakObjectPtr<UTexture2D>> Loaded;
        if (UTexture2D* Known = Loaded.FindRef(&Avatar).Get())
            return Known;
        UTexture2D* Texture = Cast<UTexture2D>(TexturePath(Avatar).TryLoad());
        if (Texture)
            Loaded.Add(&Avatar, Texture);
        return Texture;
    }
} // namespace AvatarCatalog

namespace PlayerAvatar
{
    FChoice Resolve(const GolfSettings::FPreferences& Preferences, const FString& AccountKey, bool bHasAccountPicture)
    {
        FChoice Choice;
        const AvatarCatalog::FAvatar* Picked = AvatarCatalog::Find(Preferences.AvatarId);
        if (Picked && !AccountKey.IsEmpty() && Preferences.AvatarAccount == AccountKey)
            Choice.BuiltIn = Picked;
        else if (!bHasAccountPicture)
            Choice.BuiltIn = &AvatarCatalog::Default();
        return Choice;
    }

    UTexture2D* Texture(const GolfSettings::FPreferences& Preferences, const UOpenGolfOnlineSubsystem* Online)
    {
        if (!Online || !Online->HasSession())
            return nullptr;
        UTexture2D* Picture = Online->GetPicture();
        const FChoice Choice = Resolve(Preferences, Online->GetAccountKey(), Picture != nullptr);
        return Choice.IsAccountPicture() ? Picture : AvatarCatalog::LoadTexture(*Choice.BuiltIn);
    }
} // namespace PlayerAvatar
