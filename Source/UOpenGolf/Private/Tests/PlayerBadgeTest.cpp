// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenGolfSettings.h"
#include "Profile/AvatarCatalog.h"
#include "UI/Common/AvatarImage.h"
#include "UI/Common/HudStyle.h"
#include "UI/Hud/PlayerBadge.h"
#include "Widgets/Text/STextBlock.h"

namespace PlayerBadgeTestDetail
{
    TSharedPtr<SWidget> FindTag(const TSharedRef<SWidget>& Root, const FName Tag)
    {
        if (Root->GetTag() == Tag) return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 I = 0; I < Children->Num(); ++I)
            if (TSharedPtr<SWidget> Found = FindTag(Children->GetChildAt(I), Tag)) return Found;
        return nullptr;
    }

    /** Ticks every widget under Root, as a window would before painting. */
    void TickAll(const TSharedRef<SWidget>& Root)
    {
        Root->Tick(FGeometry(), 0, 0);
        FChildren* Children = Root->GetChildren();
        for (int32 I = 0; I < Children->Num(); ++I)
            TickAll(Children->GetChildAt(I));
    }
} // namespace PlayerBadgeTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfPlayerBadgeTest, "UOpenGolf.Hud.PlayerBadge",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FOpenGolfPlayerBadgeTest::RunTest(const FString&)
{
    using namespace PlayerBadgeTestDetail;
    using namespace OpenGolfOnline;
    auto* Session = NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    FConfig Config;
    Config.Authority = TEXT("https://login.example.test/"); Config.ClientId = TEXT("test-client");
    Config.Audience = TEXT("https://api.example.test"); Config.ApiOrigin = Config.Audience;
    Config.ApplicationName = TEXT("UOpenGolf"); Config.StorageName = TEXT("PlayerBadgeTest"); Config.CallbackPorts = {47801};
    const FString Root = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString();
    Session->SetupForTests(Config, [](const FString&, const FString&, const FString&, const FString&, FHttpCallback)
    {
        auto Cancel = MakeShared<FRequestCancellation, ESPMode::ThreadSafe>(); Cancel->Finished.store(true); return Cancel;
    }, Root);
    // A synthetic signed-in account whose key matches no real pick in this computer's preferences.
    Session->AccountKey = TEXT("player-badge-test"); Session->RefreshToken = TEXT("test-refresh"); Session->AccessToken = TEXT("test-access");
    Session->TokenExpiresAt = FPlatformTime::Seconds() + 3600;
    Session->Profile = MakeShared<FJsonObject>(); Session->Profile->SetStringField(TEXT("display_name"), TEXT("Pat Putter"));

    const TSharedRef<SPlayerBadge> Badge = SNew(SPlayerBadge).Online(Session);
    auto Refresh = [&] { TickAll(Badge); Badge->SlatePrepass(1.0f); };
    auto Name = [&]
    {
        const TSharedPtr<SWidget> Text = FindTag(Badge, TEXT("PlayerBadgeName"));
        return Text.IsValid() ? StaticCastSharedPtr<STextBlock>(Text) : nullptr;
    };
    auto Avatar = [&]
    {
        const TSharedPtr<SWidget> Image = FindTag(Badge, TEXT("PlayerBadgeAvatar"));
        return Image.IsValid() ? StaticCastSharedPtr<SAvatarImage>(Image)->GetShownTexture() : nullptr;
    };
    Refresh();
    if (!TestTrue(TEXT("The badge names the player"), Name().IsValid())) return false;
    TestEqual(TEXT("The badge shows the display name"), Name()->GetText().ToString(), FString(TEXT("Pat Putter")));
    TestEqual(TEXT("The name uses the course tiles' number size"), Name()->GetFont().Size, HudStyle::ValueFont().Size);
    UTexture2D* Default = AvatarCatalog::LoadTexture(AvatarCatalog::Default());
    TestNotNull(TEXT("The default avatar is imported"), Default);
    TestTrue(TEXT("Without a picture the default avatar shows"), Avatar() == Default);

    UTexture2D* Picture = UTexture2D::CreateTransient(4, 4);
    Session->Picture = Picture;
    Refresh();
    TestTrue(TEXT("The account picture shows once it arrives"), Avatar() == Picture);

    Session->Profile->SetStringField(TEXT("display_name"), TEXT("A Remarkably Long Display Name For A Golfer"));
    Refresh();
    TestTrue(FString::Printf(TEXT("A long name is capped at %.0f"), SPlayerBadge::MaxNameWidth),
        Badge->GetDesiredSize().X <= SPlayerBadge::AvatarSize + SPlayerBadge::MaxNameWidth + 40.0f);

    Session->RefreshToken.Empty(); Session->Picture = nullptr;
    Refresh();
    TestTrue(TEXT("Signed out, the badge has no name"), Name()->GetText().IsEmpty());
    TestNull(TEXT("Signed out, the badge has no avatar"), Avatar());
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}
#endif
