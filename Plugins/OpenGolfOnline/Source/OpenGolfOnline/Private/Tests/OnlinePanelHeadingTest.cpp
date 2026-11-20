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
    struct FPending { FString Url; FHttpCallback Done; FString Body; };
    TArray<FPending> Pending;
    auto* Session = NewObject<UOpenGolfOnlineSubsystem>(NewObject<UGameInstance>());
    const FString Root = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
    Session->SetupForTests(Config, [&Pending](const FString& Url, const FString&, const FString& Body, const FString&, FHttpCallback Done)
    {
        Pending.Add({Url, MoveTemp(Done), Body});
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
    Session->KeepQrSignInReady();
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

    // Ball speed and total are averaged from the club's shots, loaded after the list without making the service busy.
    for (const TCHAR* Caption : {TEXT("Club"), TEXT("Ball speed"), TEXT("Carry"), TEXT("Total"), TEXT("Shots")})
        TestTrue(FString(TEXT("Column caption ")) + Caption, ShowsText(Panel, Caption));
    TestTrue(TEXT("Carry is the server's average"), ShowsText(Panel, TEXT("150.5 yd")));
    TestTrue(TEXT("Averages show pending while they load"), Session->IsLoadingClubAverages() && ShowsText(Panel, TEXT("…")));
    TestFalse(TEXT("Loading averages does not make the service busy"), Session->IsBusy());
    const uint64 PendingRevision = Session->GetAveragesRevision();
    // Speeds 100 and 110; totals 150 + 5 and 152 - 1; the third shot measured neither.
    Respond(TEXT("/clubs/7i/shots?"), TEXT("[{\"carry_yd\":150,\"roll_yd\":5,\"ball_speed_mph\":100},"
        "{\"carry_yd\":152,\"roll_yd\":-1,\"ball_speed_mph\":110},{\"carry_yd\":149}]"));
    Refresh();
    TestFalse(TEXT("Averages finished loading"), Session->IsLoadingClubAverages());
    TestNotEqual(TEXT("Stored averages change the revision the cells follow"), Session->GetAveragesRevision(), PendingRevision);
    TestTrue(TEXT("Average ball speed"), ShowsText(Panel, TEXT("105.0 mph")));
    TestTrue(TEXT("Average total is carry plus signed roll"), ShowsText(Panel, TEXT("153.0 yd")));

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
    Session->Tick(0);
    Refresh();
    TestFalse(TEXT("An unchanged shot count does not reload the club's averages"),
        Pending.ContainsByPredicate([](const FPending& Request) { return Request.Url.Contains(TEXT("/shots?")); }));
    TestTrue(TEXT("Club list title again"), ShowsText(Panel, TEXT("Shot history")));
    TestFalse(TEXT("No back icon after Back"), IsShown(FindTag(Panel, TEXT("PageBack"))));

    TestTrue(TEXT("Whole history action on overview"), ShowsText(Panel, TEXT("Clear all")));
    auto Queue = [&](const TCHAR* Club)
    {
        auto Shot = MakeShared<FJsonObject>();
        Shot->SetStringField(TEXT("club"), Club);
        Shot->SetStringField(TEXT("id"), FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens));
        return Session->QueueShot(Shot, Session->GetAccountKey());
    };
    Queue(TEXT("7I"));
    Queue(TEXT("DR"));
    Session->ClearClubHistory(TEXT("7I"));
    Session->ClearAllHistory();
    const FString SingleId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
    const FString Single = FString::Printf(TEXT("[{\"id\":\"%s\"}]"), *SingleId);
    Respond(TEXT("/clubs/7i/shots?"), Single);
    auto DeleteBodies = [&]
    {
        return Pending.FilterByPredicate([](const FPending& Request) { return Request.Url.EndsWith(TEXT("/range-shots/delete")); });
    };
    const auto ClubDeletes = DeleteBodies();
    TestEqual(TEXT("Only one deletion in flight"), ClubDeletes.Num(), 1);
    if (!ClubDeletes.IsEmpty()) TestTrue(TEXT("Delete includes selected club shot"), ClubDeletes[0].Body.Contains(SingleId));
    Queue(TEXT("7I"));
    Respond(TEXT("/range-shots/delete"), TEXT("{\"deleted\":1}"));
    TestEqual(TEXT("Clear club keeps another club and the newer shot"), Session->GetPendingCount(), 2);
    Session->ClearAllHistory();
    Respond(TEXT("/clubs/carry"), TEXT("[{\"club\":\"7I\"},{\"club\":\"DR\"}]"));
    TArray<FString> DriverIds, DriverRows;
    for (int32 I = 0; I < 24; ++I)
    {
        const FString Id = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphens);
        DriverIds.Add(Id);
        DriverRows.Add(FString::Printf(TEXT("{\"id\":\"%s\"}"), *Id));
    }
    Respond(TEXT("/clubs/dr/shots?"), TEXT("[") + FString::Join(DriverRows, TEXT(",")) + TEXT("]"));
    Respond(TEXT("/clubs/7i/shots?"), Single);
    const auto AllDeletes = DeleteBodies();
    TestEqual(TEXT("All 25 stored shots use one POST"), AllDeletes.Num(), 1);
    if (!AllDeletes.IsEmpty())
    {
        TestTrue(TEXT("Includes iron"), AllDeletes[0].Body.Contains(SingleId));
        for (const FString& Id : DriverIds) TestTrue(TEXT("Includes driver shot"), AllDeletes[0].Body.Contains(Id));
    }
    Queue(TEXT("DR"));
    Respond(TEXT("/range-shots/delete"), TEXT("{\"deleted\":25}"));
    TestEqual(TEXT("Clear all keeps shots recorded after confirmation"), Session->GetPendingCount(), 1);

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
