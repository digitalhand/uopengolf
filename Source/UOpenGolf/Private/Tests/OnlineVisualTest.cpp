// SPDX-License-Identifier: MIT
// The account in the real menu against a stubbed online service: the top bar's sign-in and logout icons beside the
// cog, Settings › Player's account, Shot history and time zone, and abbreviated clubs.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/RenderedPlay.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenGolfSettings.h"
#include "UI/Menu/MainMenu.h"
#include "UOpenGolfPlayerController.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace OnlineVisualTestDetail
{
    /** The 7 iron row of the stubbed club statistics, and the page header's back control. */
    const TCHAR* ClubRowTag = TEXT("ClubRow:7I");
    const TCHAR* PageBackTag = TEXT("PageBack");
    const TCHAR* PacificLabel = TEXT("Pacific (PT)");
    /** The stubbed shot's time, 2026-11-11T00:00:00Z, as Pacific (PST) wall time. */
    FString PacificShotTime()
    {
        return FText::AsDateTime(FDateTime(2026, 11, 10, 16, 0, 0), EDateTimeStyle::Medium, EDateTimeStyle::Short, FText::GetInvariantTimeZone()).ToString();
    }
    bool IsShownWidget(const TSharedPtr<SWidget>& Widget) { return Widget.IsValid() && Widget->GetVisibility().IsVisible(); }
} // namespace OnlineVisualTestDetail

class FOnlineVisualCapture : public IAutomationLatentCommand
{
public:
    explicit FOnlineVisualCapture(FAutomationTestBase* InTest) : Test(InTest) {}

    bool Update() override
    {
        using namespace OnlineVisualTestDetail;
        if (GetCurrentRunTime() > 120) { Test->AddError(TEXT("Online UI capture timed out.")); Cleanup(); return true; }
        UWorld* PlayWorld = GEditor->PlayWorld;
        AUOpenGolfPlayerController* Player = PlayWorld ? Cast<AUOpenGolfPlayerController>(PlayWorld->GetFirstPlayerController()) : nullptr;
        if (!Player || !PlayWorld->GetGameViewport() || !PlayWorld->GetGameViewport()->GetWindow())
            return false;
        const TSharedRef<SWindow> Window = PlayWorld->GetGameViewport()->GetWindow().ToSharedRef();
        if (!Menu)
        {
            Start(*PlayWorld, *Player);
            return false;
        }
        Service->AdvanceForTests();
        if (DeferredProfileReply && FPlatformTime::Seconds() >= ProfileReadyAt)
        {
            auto Reply = MoveTemp(DeferredProfileReply);
            Reply({200, ProfileBody, true});
        }
        if (GetCurrentRunTime() < Until)
            return false;
        if (!File.IsEmpty())
        {
            if (!IFileManager::Get().FileExists(*File))
                return false;
            Test->AddInfo(TEXT("Captured ") + File);
            File.Empty();
            ++Stage;
        }
        const TSharedRef<SWidget> Root = Menu.ToSharedRef();
        switch (Stage)
        {
        case 0:
            // Home page, signed out.
            Test->TestFalse(TEXT("No Settings button on the home page"), RenderedPlay::FindButton(Root, TEXT("Settings")).IsValid());
            ExpectTopBar(TEXT("AccountSignIn"), TEXT("AccountSignOut"), TEXT("signed out"));
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::WithTag(Root, TEXT("AccountSignIn")), TEXT("the sign-in icon")));
        case 1:
            SettingsPage=RenderedPlay::WithTag(Root,TEXT("MenuPageHost"))->GetChildren()->GetChildAt(0);
            TimeZoneControl=RenderedPlay::FindButton(Root,GolfSettings::ChosenTimeZone().Label);
            Test->TestTrue(TEXT("The sign-in icon opens Settings"), RenderedPlay::HasText(Root, TEXT("Settings")));
            Test->TestTrue(TEXT("Player offers sign-in"), RenderedPlay::HasText(Root, TEXT("Sign in with QR")));
            Test->TestFalse(TEXT("Signed out, Player has no display name"), RenderedPlay::HasText(Root, TEXT("Display name")));
            Test->TestFalse(TEXT("Signed out, Player has no shot history"), RenderedPlay::HasText(Root, TEXT("Shot history")));
            Test->TestTrue(TEXT("Player offers the time zone"), RenderedPlay::HasText(Root, TEXT("Time zone")));
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::FindButton(Root, TEXT("Sign in with QR")), TEXT("Sign in with QR")));
        case 2:
            Capture(Window, TEXT("qr"));
            return false;
        case 3:
            if (Service->IsSigningIn() || !Service->HasSession() || Service->IsBusy())
                return false;
            return Next(true);
        case 4:
            Test->TestTrue(TEXT("Login preserves the Settings page"),SettingsPage==RenderedPlay::WithTag(Root,TEXT("MenuPageHost"))->GetChildren()->GetChildAt(0));
            Test->TestTrue(TEXT("Login preserves unrelated controls"),TimeZoneControl==RenderedPlay::FindButton(Root,GolfSettings::ChosenTimeZone().Label));
            ExpectTopBar(TEXT("AccountSignOut"), TEXT("AccountSignIn"), TEXT("signed in"));
            Test->TestTrue(TEXT("Player shows the display name"), RenderedPlay::HasText(Root, TEXT("Display name")));
            Test->TestTrue(TEXT("Player shows shot history"), RenderedPlay::HasText(Root, TEXT("Shot history")));
            Test->TestFalse(TEXT("No range-shot counts"), RenderedPlay::HasText(Root, TEXT("Pending"), true));
            for (const TCHAR* Label : {TEXT("Sign in with QR"), TEXT("Sign in on this computer"), TEXT("Create account"), TEXT("Try again")})
                Test->TestFalse(FString(TEXT("Signed in, Player excludes ")) + Label, RenderedPlay::HasText(Root, Label));
            Capture(Window, TEXT("player-signed-in"));
            return false;
        case 5:
            // The time-zone drop-down shows the current zone; the choice opens in a menu window.
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::FindButton(Root, GolfSettings::ChosenTimeZone().Label), TEXT("the time zone")));
        case 6:
            return Next(RenderedPlay::ClickLabelled(*Test, PacificLabel));
        case 7:
            Test->TestEqual(TEXT("Pacific applies at once"), FString(GolfSettings::ChosenTimeZone().Short), FString(TEXT("PT")));
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::FindButton(Root, TEXT("View")), TEXT("Shot history's View")));
        case 8:
            if (Service->IsBusy())
                return false;
            Test->TestTrue(TEXT("Shot history page"), RenderedPlay::HasText(Root, TEXT("Shot history")));
            for (const TCHAR* Label : {TEXT("7i"), TEXT("150.5 yd average"), TEXT("12 shots")})
                Test->TestTrue(FString(TEXT("Club row shows ")) + Label, RenderedPlay::HasText(Root, Label));
            Capture(Window, TEXT("clubs"));
            return false;
        case 9:
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::WithTag(Root, ClubRowTag), TEXT("the 7i row")));
        case 10:
            if (Service->IsBusy())
                return false;
            Test->TestTrue(TEXT("History title names the club"), RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            Test->TestTrue(TEXT("Time column names the zone"), RenderedPlay::HasText(Root, TEXT("Time (PT)")));
            Test->TestTrue(TEXT("Shot time is Pacific"), RenderedPlay::HasText(Root, PacificShotTime()));
            Capture(Window, TEXT("history"));
            return false;
        case 11:
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::WithTag(Root, PageBackTag), TEXT("back from one club")));
        case 12:
            if (Service->IsBusy())
                return false;
            Test->TestTrue(TEXT("Back returns to the club list"), RenderedPlay::HasText(Root, TEXT("7i")) && !RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::WithTag(Root, ClubRowTag), TEXT("the 7i row again")));
        case 13:
            if (Service->IsBusy())
                return false;
            Test->TestTrue(TEXT("The club's shots reopen"), RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            // Leave one club's shots by the cog rather than Back.
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::WithTag(Root, TEXT("MenuSettings")), TEXT("the cog from one club's shots")));
        case 14:
            Test->TestTrue(TEXT("The cog returns to Player"), RenderedPlay::HasText(Root, TEXT("Display name")));
            Test->TestFalse(TEXT("Club averages stay on their own page"), RenderedPlay::HasText(Root, TEXT("CLUB AVERAGES")));
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::FindButton(Root, TEXT("View")), TEXT("Shot history's View again")));
        case 15:
            if (Service->IsBusy())
                return false;
            // A cached page would reopen on the last club's shots.
            Test->TestTrue(TEXT("View reopens on the club averages"), RenderedPlay::HasText(Root, TEXT("Shot history")) && RenderedPlay::HasText(Root, TEXT("7i")));
            Test->TestFalse(TEXT("View does not reopen the last club's shots"), RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::WithTag(Root, PageBackTag), TEXT("back from shot history")));
        case 16:
            Test->TestTrue(TEXT("Back returns to Player"), RenderedPlay::HasText(Root, TEXT("Display name")));
            return Next(RenderedPlay::ClickOn(*Test, Window, RenderedPlay::WithTag(Root, TEXT("AccountSignOut")), TEXT("the logout icon")));
        case 17:
            Test->TestFalse(TEXT("The logout icon ends the session"), Service->HasSession());
            ExpectTopBar(TEXT("AccountSignIn"), TEXT("AccountSignOut"), TEXT("after logout"));
            Test->TestTrue(TEXT("Sign-in choices return"), RenderedPlay::HasText(Root, TEXT("Sign in with QR")));
            Test->TestFalse(TEXT("Signed out, no display name"), RenderedPlay::HasText(Root, TEXT("Display name")));
            Test->TestFalse(TEXT("Signed out, no shot history"), RenderedPlay::HasText(Root, TEXT("Shot history")));
            Capture(Window, TEXT("signed-out"));
            return false;
        default:
            Cleanup();
            return true;
        }
    }

private:
    TSharedPtr<SWidget> SettingsPage, TimeZoneControl;
    void Start(UWorld& PlayWorld, AUOpenGolfPlayerController& Player)
    {
        StorageRoot = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
        ScreenRoot = FPaths::ProjectSavedDir() / TEXT("Screenshots/Online") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Service.Reset(NewObject<UOpenGolfOnlineSubsystem>(PlayWorld.GetGameInstance()));
        OpenGolfOnline::FConfig Config;
        Config.Authority = TEXT("https://login.example.test/"); Config.ClientId = TEXT("test"); Config.Audience = TEXT("https://api.example.test");
        Config.ApiOrigin = TEXT("https://api.example.test"); Config.ApplicationName = TEXT("UOpenGolf"); Config.StorageName = TEXT("OnlineVisualTest");
        Config.CallbackPorts = {47801};
        Service->SetupForTests(Config, [this](const FString& Url, const FString&, const FString&, const FString&, OpenGolfOnline::FHttpCallback Done)
        {
            FString Body = TEXT("{}");
            if (Url.EndsWith(TEXT("oauth/device/code"))) Body = TEXT("{\"device_code\":\"test-only\",\"user_code\":\"TEST-CODE\",\"verification_uri\":\"https://login.example.test/activate\",\"verification_uri_complete\":\"https://login.example.test/activate?user_code=TEST-CODE\",\"expires_in\":600,\"interval\":5}");
            else if (Url.EndsWith(TEXT("oauth/token"))) Body = TEXT("{\"access_token\":\"test\",\"refresh_token\":\"test\",\"token_type\":\"Bearer\",\"expires_in\":3600}");
            else if (Url.EndsWith(TEXT("userinfo"))) Body = TEXT("{\"sub\":\"test|visual\"}");
            else if (Url.EndsWith(TEXT("/sync"))) Body = TEXT("{\"id\":\"test-player\",\"display_name\":\"Test Golfer\",\"created_at\":\"2026-11-11T00:00:00Z\"}");
            else if (Url.Contains(TEXT("/clubs/carry"))) Body = TEXT("[{\"club\":\"7I\",\"shots\":12,\"avg_carry_yd\":150.5}]");
            else if (Url.Contains(TEXT("/shots?"))) Body = TEXT("[{\"id\":\"019a0000-0000-7000-8000-000000000001\",\"club\":\"7I\",\"shot_at\":\"2026-11-11T00:00:00Z\",\"carry_yd\":150.5,\"roll_yd\":-2.1,\"ball_speed_mph\":105.5,\"launch_angle_deg\":18.0,\"total_spin_rpm\":6000}]");
            auto Cancel = MakeShared<OpenGolfOnline::FRequestCancellation, ESPMode::ThreadSafe>();
            Cancel->Finished.store(true);
            if (Url.EndsWith(TEXT("/sync")))
            {
                DeferredProfileReply = MoveTemp(Done);
                ProfileBody = Body;
                ProfileReadyAt = FPlatformTime::Seconds() + 0.6;
            }
            else
            {
                if (Url.Contains(TEXT("/clubs/carry"))) Test->TestTrue(TEXT("Club statistics wait for first profile sync"), Service->GetProfile().IsValid());
                Done({200, Body, true});
            }
            return Cancel;
        }, StorageRoot);
        // The time zone is changed through the menu; it is restored at the end. Automation never writes the player's file.
        SavedPreferences = GolfSettings::Get();
        Controller = &Player;
        Menu = SNew(SMainMenu).Controller(&Player).Online(Service.Get());
        PlayWorld.GetGameViewport()->AddViewportWidgetContent(Menu.ToSharedRef(), 1000);
        Menu->Open();
        Until = GetCurrentRunTime() + 1;
    }

    /** Advances after a step's action; a failed click ends the test. */
    bool Next(bool bDone)
    {
        if (!bDone) { Cleanup(); return true; }
        ++Stage;
        Until = GetCurrentRunTime() + 1;
        return false;
    }

    /** The account icon for the session shows alone, between the launch-monitor status and the cog. */
    void ExpectTopBar(const TCHAR* ShownTag, const TCHAR* HiddenTag, const FString& Where)
    {
        using namespace OnlineVisualTestDetail;
        const TSharedRef<SWidget> Root = Menu.ToSharedRef();
        const TSharedPtr<SWidget> Shown = RenderedPlay::WithTag(Root, ShownTag), Monitor = RenderedPlay::WithTag(Root, TEXT("MenuLaunchMonitor"));
        const TSharedPtr<SWidget> Cog = RenderedPlay::WithTag(Root, TEXT("MenuSettings"));
        Test->TestTrue(FString(ShownTag) + TEXT(" shows ") + Where, IsShownWidget(Shown));
        Test->TestFalse(FString(HiddenTag) + TEXT(" hides ") + Where, IsShownWidget(RenderedPlay::WithTag(Root, HiddenTag)));
        Test->TestTrue(TEXT("The cog shows ") + Where, IsShownWidget(Cog));
        if (IsShownWidget(Shown) && IsShownWidget(Monitor) && IsShownWidget(Cog))
            Test->TestTrue(TEXT("Launch monitor, account icon, cog, in that order, ") + Where,
                RenderedPlay::PointOn(*Monitor).X < RenderedPlay::PointOn(*Shown).X && RenderedPlay::PointOn(*Shown).X < RenderedPlay::PointOn(*Cog).X);
    }

    void Capture(const TSharedRef<SWindow>& Window, const FString& Name)
    {
        RenderedPlay::ExpectNoToolTips(*Test, Window, Name);
        File = FPaths::ConvertRelativePathToFull(ScreenRoot / (Name + TEXT(".png")));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(File), true);
        FScreenshotRequest::RequestScreenshot(File, true, false);
    }

    void Cleanup()
    {
        UWorld* PlayWorld = GEditor->PlayWorld;
        if (PlayWorld && PlayWorld->GetGameViewport() && Menu)
            PlayWorld->GetGameViewport()->RemoveViewportWidgetContent(Menu.ToSharedRef());
        if (Controller.IsValid())
        {
            Controller->SetMainMenuOpen(false);
            GolfSettings::Commit(SavedPreferences, Controller->GetWorld());
        }
        if (Service.IsValid()) Service->CancelSignIn();
        Menu.Reset();
        Service.Reset();
        if (!StorageRoot.IsEmpty()) IFileManager::Get().DeleteDirectory(*StorageRoot, false, true);
        FScreenshotRequest::Reset();
    }

    FAutomationTestBase* Test;
    TStrongObjectPtr<UOpenGolfOnlineSubsystem> Service;
    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TSharedPtr<SMainMenu> Menu;
    GolfSettings::FPreferences SavedPreferences;
    OpenGolfOnline::FHttpCallback DeferredProfileReply;
    int32 Stage = 0;
    double Until = 0, ProfileReadyAt = 0;
    FString File, StorageRoot, ScreenRoot, ProfileBody;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnlineVisualTest, "UOpenGolf.Visual.Online",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FOnlineVisualTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TEXT("/Game/UOpenGolf/Maps/UOpenGolf"), false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOnlineVisualCapture>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
