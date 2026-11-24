// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"

class UTexture2D;
class UOpenGolfOnlineSubsystem;
namespace GolfSettings { struct FPreferences; }

/**
 * The built-in player avatars: Fluent Emoji 3D images (MIT, Microsoft), fetched by
 * Plugins/OpenGolfSavanna/Scripts/FetchFluentEmoji.py and imported by ImportAvatars.py beside it. See the plugin's NOTICE.md.
 */
namespace AvatarCatalog
{
    struct FAvatar
    {
        /** Saved in preferences, so an id is never renamed. Its texture is T_Avatar_<Id>. */
        const TCHAR* Id;
        /** For screen readers and the picker's caption. */
        const TCHAR* Name;
    };

    /** In picker order. */
    TConstArrayView<FAvatar> All();
    /** Null for an empty or unknown id. */
    const FAvatar* Find(const FString& Id);
    /** Shown when there is neither a pick nor an account picture. */
    const FAvatar& Default();
    /** /OpenGolfSavanna/UI/Avatars/T_Avatar_<Id>.T_Avatar_<Id> */
    FSoftObjectPath TexturePath(const FAvatar& Avatar);
    /** The avatar's texture, loaded on first use; null when it was not imported. */
    UTexture2D* LoadTexture(const FAvatar& Avatar);
} // namespace AvatarCatalog

namespace PlayerAvatar
{
    /** What a signed-in player shows: a built-in avatar, or (BuiltIn null) the account's own picture. */
    struct FChoice
    {
        const AvatarCatalog::FAvatar* BuiltIn = nullptr;
        bool IsAccountPicture() const { return BuiltIn == nullptr; }
    };

    /**
     * The avatar picked for AccountKey wins; otherwise the account picture when there is one; otherwise the default
     * avatar. A pick made for another account, or one no longer in the catalogue, is ignored.
     */
    FChoice Resolve(const GolfSettings::FPreferences& Preferences, const FString& AccountKey, bool bHasAccountPicture);

    /** The texture Resolve chooses for Online's account; null while signed out. */
    UTexture2D* Texture(const GolfSettings::FPreferences& Preferences, const UOpenGolfOnlineSubsystem* Online);
} // namespace PlayerAvatar
