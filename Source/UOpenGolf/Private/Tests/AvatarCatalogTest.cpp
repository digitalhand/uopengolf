// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Texture2D.h"
#include "OpenGolfSettings.h"
#include "Profile/AvatarCatalog.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAvatarCatalogTest, "UOpenGolf.Profile.Avatars",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAvatarCatalogTest::RunTest(const FString&)
{
    const TConstArrayView<AvatarCatalog::FAvatar> All = AvatarCatalog::All();
    TestTrue(FString::Printf(TEXT("At least 20 built-in avatars (%d)"), All.Num()), All.Num() >= 20);
    TSet<FString> Ids;
    for (const AvatarCatalog::FAvatar& Avatar : All)
    {
        const FString Id = Avatar.Id;
        TestFalse(TEXT("Avatar id is unique: ") + Id, Ids.Contains(Id));
        Ids.Add(Id);
        TestTrue(TEXT("Avatar has a name: ") + Id, FCString::Strlen(Avatar.Name) > 0);
        TestTrue(TEXT("Find returns the avatar: ") + Id, AvatarCatalog::Find(Id) == &Avatar);
        UTexture2D* Texture = AvatarCatalog::LoadTexture(Avatar);
        if (!TestNotNull(TEXT("Avatar texture is imported: ") + AvatarCatalog::TexturePath(Avatar).ToString(), Texture))
            continue;
        TestEqual(TEXT("Avatar texture is a UI texture: ") + Id, static_cast<int32>(Texture->LODGroup), static_cast<int32>(TEXTUREGROUP_UI));
        TestTrue(TEXT("Avatar texture is sRGB: ") + Id, Texture->SRGB != 0);
#if WITH_EDITORONLY_DATA
        TestEqual(TEXT("Avatar texture has no mips: ") + Id, static_cast<int32>(Texture->MipGenSettings), static_cast<int32>(TMGS_NoMipmaps));
#endif
        TestTrue(FString::Printf(TEXT("Avatar texture is at most 256 px: %s %dx%d"), *Id, Texture->GetSizeX(), Texture->GetSizeY()),
            Texture->GetSizeX() > 0 && Texture->GetSizeX() <= 256 && Texture->GetSizeY() <= 256);
    }
    TestNull(TEXT("An empty id is no avatar"), AvatarCatalog::Find(TEXT("")));
    TestNull(TEXT("An unknown id is no avatar"), AvatarCatalog::Find(TEXT("NotAnAvatar")));
    TestTrue(TEXT("The default avatar is in the catalogue"), AvatarCatalog::Find(AvatarCatalog::Default().Id) == &AvatarCatalog::Default());

    // Resolve: the pick for this account, else the account picture, else the default avatar.
    GolfSettings::FPreferences P;
    const FString Me = TEXT("account-me");
    TestTrue(TEXT("No pick shows the account picture"), PlayerAvatar::Resolve(P, Me, true).IsAccountPicture());
    TestTrue(TEXT("No pick and no picture shows the default"), PlayerAvatar::Resolve(P, Me, false).BuiltIn == &AvatarCatalog::Default());
    P.AvatarId = TEXT("Birdie");
    P.AvatarAccount = Me;
    TestTrue(TEXT("This account's pick wins over its picture"), PlayerAvatar::Resolve(P, Me, true).BuiltIn == AvatarCatalog::Find(TEXT("Birdie")));
    TestTrue(TEXT("This account's pick shows without a picture"), PlayerAvatar::Resolve(P, Me, false).BuiltIn == AvatarCatalog::Find(TEXT("Birdie")));
    TestTrue(TEXT("Another account's pick is ignored"), PlayerAvatar::Resolve(P, TEXT("account-other"), true).IsAccountPicture());
    TestTrue(TEXT("Another account's pick falls back to the default"),
        PlayerAvatar::Resolve(P, TEXT("account-other"), false).BuiltIn == &AvatarCatalog::Default());
    TestTrue(TEXT("Signed out, a pick is ignored"), PlayerAvatar::Resolve(P, FString(), false).BuiltIn == &AvatarCatalog::Default());
    P.AvatarId.Empty();
    TestTrue(TEXT("Picking the account picture again shows it"), PlayerAvatar::Resolve(P, Me, true).IsAccountPicture());
    P.AvatarId = TEXT("Removed");
    TestTrue(TEXT("A pick no longer in the catalogue shows the account picture"), PlayerAvatar::Resolve(P, Me, true).IsAccountPicture());
    return true;
}
#endif
