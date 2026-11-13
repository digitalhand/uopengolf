// SPDX-License-Identifier: MIT
// SOpenGolfAccountPanel draws its own heading by default (ShowHeading). The heading is built once, so its back icon,
// title and logout icon must follow the panel's page and the session: one club's shots open and close, and the
// session ends. Checked headless against a stubbed service; SlatePrepass refreshes the bound attributes.
#if WITH_DEV_AUTOMATION_TESTS
#include "OpenGolfOnlineSubsystem.h"
#include "SOpenGolfAccountPanel.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

namespace OnlinePanelHeadingTestDetail
{
    TSharedPtr<SWidget> FindTag(const TSharedRef<SWidget>& Root, FName Tag)
    {
        if (Root->GetTag() == Tag)
            return Root;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (TSharedPtr<SWidget> Found = FindTag(Children->GetChildAt(Index), Tag))
                return Found;
        }
        return nullptr;
    }

    bool IsShown(const TSharedPtr<SWidget>& Widget) { return Widget.IsValid() && Widget->GetVisibility().IsVisible(); }

    /** Whether a shown text block under Root reads Text; collapsed subtrees are skipped. */
    bool ShowsText(const TSharedRef<SWidget>& Root, const FString& Text)
    {
        if (!Root->GetVisibility().IsVisible())
            return false;
        if (Root->GetTypeAsString() == TEXT("STextBlock") && StaticCastSharedRef<STextBlock>(Root)->GetText().ToString() == Text)
            return true;
        FChildren* Children = Root->GetChildren();
        for (int32 Index = 0; Index < Children->Num(); ++Index)
        {
            if (ShowsText(Children->GetChildAt(Index), Text))
                return true;
        }
        return false;
    }
} // namespace OnlinePanelHeadingTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenGolfPanelHeadingTest, "OpenGolf.Online.PanelHeading",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenGolfPanelHeadingTest::RunTest(const FString&)
{
    using namespace OpenGolfOnline;
    using namespace OnlinePanelHeadingTestDetail;
    FConfig Config;
    Config.Authority = TEXT("https://login.example.test/"); Config.ClientId = TEXT("test-client");
    Config.Audience = TEXT("https://api.example.test"); Config.ApiOrigin = TEXT("https://api.example.test");
    Config.ApplicationName = TEXT("Test golf"); Config.StorageName = TEXT("OpenGolfOnlinePanelTests"); Config.CallbackPorts = {47801};
    struct FPending { FString Url; FHttpCallback Done; };
    TArray<FPending> Pending;
    auto* Session = NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    const FString Root = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Session->SetupForTests(Config, [&Pending](const FString& Url, const FString&, const FString&, const FString&, FHttpCallback Done)
    {
        Pending.Add({Url, MoveTemp(Done)});
        auto Cancel = MakeShared<FRequestCancellation, ESPMode::ThreadSafe>(); Cancel->Finished.store(true); return Cancel;
    }, Root);
    // Answers every pending request whose URL contains Part (the service and the panel can both ask for club
    // averages); an error names the pending URLs when there is none.
    auto Respond = [&](const TCHAR* Part, const FString& Body)
    {
        int32 Answered = 0;
        for (int32 Index; (Index = Pending.IndexOfByPredicate([Part](const FPending& Request) { return Request.Url.Contains(Part); })) != INDEX_NONE; ++Answered)
        {
            FPending Request = MoveTemp(Pending[Index]);
            Pending.RemoveAt(Index);
            Request.Done({200, Body, true});
        }
        if (Answered == 0)
        {
            FString Waiting;
            for (const FPending& Request : Pending) Waiting += TEXT(" ") + Request.Url;
            AddError(FString::Printf(TEXT("Expected a request to %s; pending:%s"), Part, *Waiting));
        }
    };

    // Sign in as OpenGolf.Online.Session does.
    Session->SignInWithQr();
    Respond(TEXT("oauth/device/code"), TEXT("{\"device_code\":\"private-code\",\"user_code\":\"USER-CODE\",\"verification_uri\":\"https://login.example.test/activate\",\"verification_uri_complete\":\"https://login.example.test/activate?user_code=USER-CODE\",\"expires_in\":600,\"interval\":5}"));
    Session->PollAt = 0; Session->Tick(0);
    Respond(TEXT("oauth/token"), TEXT("{\"access_token\":\"access\",\"refresh_token\":\"refresh\",\"token_type\":\"Bearer\",\"expires_in\":3600}"));
    Respond(TEXT("userinfo"), TEXT("{\"sub\":\"auth0|heading\"}"));
    Respond(TEXT("/sync"), TEXT("{\"id\":\"player\",\"display_name\":\"Golfer\",\"created_at\":\"2026-11-11T00:00:00Z\"}"));
    TestTrue(TEXT("Signed in with a profile"), Session->HasSession() && Session->GetProfile().IsValid());

    const TSharedRef<SOpenGolfAccountPanel> Panel = SNew(SOpenGolfAccountPanel).Online(Session).Page(EOpenGolfAccountPage::Shots);
    auto Refresh = [&] { Panel->Tick(FGeometry(), 0, 0); Panel->SlatePrepass(1.0f); };
    // The service queues player requests and sends them from its ticker; the test pumps it.
    Session->Tick(0);
    Respond(TEXT("/clubs/carry"), TEXT("[{\"club\":\"7I\",\"shots\":12,\"avg_carry_yd\":150.5}]"));
    Refresh();
    Session->Tick(0);
    TestFalse(TEXT("Club averages are requested once per visit"),
        Pending.ContainsByPredicate([](const FPending& Request) { return Request.Url.Contains(TEXT("/clubs/carry")); }));
    TestTrue(TEXT("Club list title"), ShowsText(Panel, TEXT("Shot history")));
    TestFalse(TEXT("No back icon on the club list"), IsShown(FindTag(Panel, TEXT("PageBack"))));
    TestTrue(TEXT("Logout icon while signed in"), IsShown(FindTag(Panel, TEXT("AccountSignOut"))));

    const TSharedPtr<SWidget> Row = FindTag(Panel, TEXT("ClubRow:7I"));
    TestTrue(TEXT("The 7i row is a button"), Row.IsValid() && Row->GetTypeAsString() == TEXT("SButton"));
    if (Row.IsValid() && Row->GetTypeAsString() == TEXT("SButton"))
        StaticCastSharedPtr<SButton>(Row)->SimulateClick();
    Session->Tick(0);
    Respond(TEXT("/shots?"), TEXT("[{\"id\":\"019a0000-0000-7000-8000-000000000001\",\"club\":\"7I\",\"shot_at\":\"2026-11-11T00:00:00Z\",\"carry_yd\":150.5}]"));
    Refresh();
    TestTrue(TEXT("One club's title"), ShowsText(Panel, TEXT("7i — shot history")));
    TestTrue(TEXT("Back icon in one club's shots"), IsShown(FindTag(Panel, TEXT("PageBack"))));

    TestTrue(TEXT("Back leaves one club's shots"), Panel->HandleBack());
    Session->Tick(0);
    Respond(TEXT("/clubs/carry"), TEXT("[{\"club\":\"7I\",\"shots\":12,\"avg_carry_yd\":150.5}]"));
    Refresh();
    TestTrue(TEXT("Club list title again"), ShowsText(Panel, TEXT("Shot history")));
    TestFalse(TEXT("No back icon after Back"), IsShown(FindTag(Panel, TEXT("PageBack"))));

    Session->SignOut();
    Pending.Empty();
    Refresh();
    TestFalse(TEXT("No logout icon after sign-out"), IsShown(FindTag(Panel, TEXT("AccountSignOut"))));
    TestFalse(TEXT("No back icon after sign-out"), IsShown(FindTag(Panel, TEXT("PageBack"))));
    TestTrue(TEXT("Signed out, the page asks for sign-in"), ShowsText(Panel, TEXT("Sign in to see your saved shots.")));
    IFileManager::Get().DeleteDirectory(*Root, false, true);
    return true;
}

#endif
