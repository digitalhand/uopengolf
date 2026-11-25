// SPDX-License-Identifier: MIT
// The account in the real menu against a stubbed online service: the top bar's sign-in and logout icons beside the
// cog, the home page's QR sign-in, Settings › Player's account, Shot history with local dates, club averages and
// abbreviated clubs.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TestMaps.h"
#include "Tests/RenderedPlay.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "OpenGolfOnlineSubsystem.h"
#include "UI/Menu/MainMenu.h"
#include "UOpenGolfPlayerController.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Widgets/Layout/SScrollBox.h"

namespace OnlineVisualTestDetail
{
    /** The 7 iron row of the stubbed club statistics, and the page header's back control. */
    const TCHAR* ClubRowTag = TEXT("ClubRow:7I");
    const TCHAR* PageBackTag = TEXT("PageBack");
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
            {
                // Signed out, the home page offers the QR sign-in under its buttons, without opening Settings.
                const TSharedPtr<SWidget> Qr = RenderedPlay::WithTag(Root, TEXT("HomeSignInQr")), Exit = RenderedPlay::FindButton(Root, TEXT("Exit"));
                Test->TestTrue(TEXT("The home page shows the QR sign-in"), IsShownWidget(Qr));
                if (IsShownWidget(Qr) && Exit.IsValid())
                    Test->TestTrue(TEXT("The QR sign-in sits under Exit"), RenderedPlay::PointOn(*Qr).Y > RenderedPlay::PointOn(*Exit).Y);
                Test->TestTrue(TEXT("The home QR shows its short code"), RenderedPlay::HasText(Root, TEXT("Code TEST-CODE")));
            }
            Test->TestFalse(TEXT("Starting Training does not require sign-in"), Service->HasSession());
            if (!ClickOn(Window, RenderedPlay::FindButton(Root, TEXT("Training")), TEXT("Training while signed out")))
                return true;
            Stage = 19;
            Until = GetCurrentRunTime() + 1;
            return false;
        case 19:
            Test->TestFalse(TEXT("Training closes the main menu"), Player->IsMainMenuOpen());
            Test->TestFalse(TEXT("Training keeps the player signed out"), Service->HasSession());
            {
                const TSharedPtr<SWidget> Cog = RenderedPlay::WithTag(Window, TEXT("HudSettings"));
                Test->TestTrue(TEXT("Training paints the settings cog while signed out"),
                    Cog.IsValid() && RenderedPlay::PathAt(Window, RenderedPlay::PointOn(*Cog)).ContainsWidget(Cog.Get()));
            }
            Menu->Open();
            Stage = 20;
            Until = GetCurrentRunTime() + 1;
            return false;
        case 20:
            Stage = 0;
            return Next(ClickOn(Window, RenderedPlay::WithTag(Root, TEXT("AccountSignIn")), TEXT("the sign-in icon")));
        case 1:
            SettingsPage=RenderedPlay::WithTag(Root,TEXT("MenuPageHost"))->GetChildren()->GetChildAt(0);
            Test->TestTrue(TEXT("The sign-in icon opens Settings"), RenderedPlay::HasText(Root, TEXT("Settings")));
            ExpectSignInChoices(TEXT("signed out"));
            Test->TestFalse(TEXT("Signed out, Player has no display name"), RenderedPlay::HasText(Root, TEXT("Display name")));
            Test->TestFalse(TEXT("Signed out, Player has no shot history"), RenderedPlay::HasText(Root, TEXT("Shot history")));
            return Next(true);
        case 2:
            // The QR code needs no button: it is requested once the home page or Settings shows it. The stubbed
            // service keeps it pending until now, then approves it at the next poll, at most five seconds later.
            Test->TestTrue(TEXT("The QR code's short code shows"), RenderedPlay::HasText(Root, TEXT("Code TEST-CODE")));
            bApproveQr = true;
            Capture(Window, TEXT("qr"));
            return false;
        case 3:
            if (Service->IsSigningIn() || !Service->HasSession() || Service->IsBusy())
                return false;
            return Next(true);
        case 4:
            Test->TestTrue(TEXT("Login preserves the Settings page"),SettingsPage==RenderedPlay::WithTag(Root,TEXT("MenuPageHost"))->GetChildren()->GetChildAt(0));
            ExpectTopBar(TEXT("AccountSignOut"), TEXT("AccountSignIn"), TEXT("signed in"));
            Test->TestTrue(TEXT("Player shows the display name"), RenderedPlay::HasText(Root, TEXT("Display name")));
            Test->TestTrue(TEXT("Player shows shot history"), RenderedPlay::HasText(Root, TEXT("Shot history")));
            Test->TestFalse(TEXT("No range-shot counts"), RenderedPlay::HasText(Root, TEXT("Pending"), true));
            for (const TCHAR* Label : {TEXT("Sign In"), TEXT("Create Account"), TEXT("Sign-In"), TEXT("Try again")})
                Test->TestFalse(FString(TEXT("Signed in, Settings excludes ")) + Label, RenderedPlay::HasText(Root, Label));
            Test->TestFalse(TEXT("Signed in, the QR sign-in hides"), IsShownWidget(RenderedPlay::WithTag(Root, TEXT("SignInQr"))));
            Capture(Window, TEXT("player-signed-in"));
            return false;
        case 5:
            return Next(ClickOn(Window, RenderedPlay::FindButton(Root, TEXT("View")), TEXT("Shot history's View")));
        case 6:
            if (Service->IsBusy() || Service->IsLoadingClubAverages())
                return false;
            Test->TestTrue(TEXT("Shot history page"), RenderedPlay::HasText(Root, TEXT("Shot history")));
            // Every club's shots are the stub's one 7i shot: 105.5 mph, 150.5 yd carry and -2.1 yd roll.
            for (const TCHAR* Label : {TEXT("Ball speed"), TEXT("Total"), TEXT("7i"), TEXT("105.5 mph"), TEXT("150.5 yd"), TEXT("148.4 yd"), TEXT("12 shots")})
                Test->TestTrue(FString(TEXT("Club averages show ")) + Label, RenderedPlay::HasText(Root, Label));
            ExpectClubOrder(Root, {TEXT("DR"), TEXT("5W"), TEXT("7I"), TEXT("PW"), TEXT("GW")});
            if (!ClickOn(Window, RenderedPlay::WithTag(Root, TEXT("RefreshHistory")), TEXT("Refresh club averages")))
                return true;
            Stage = 18;
            Until = GetCurrentRunTime() + 1;
            return false;
        case 18:
            if (Service->IsBusy() || Service->IsLoadingClubAverages())
                return false;
            ExpectClubOrder(Root, {TEXT("5W"), TEXT("DR"), TEXT("7I"), TEXT("PW"), TEXT("GW")});
            Stage = 6;
            Capture(Window, TEXT("clubs"));
            return false;
        case 7:
            return Next(ClickOn(Window, RenderedPlay::WithTag(Root, ClubRowTag), TEXT("the 7i row")));
        case 8:
            if (Service->IsBusy())
                return false;
            Test->TestTrue(TEXT("History title names the club"), RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            Test->TestTrue(TEXT("Time column is labeled local"), RenderedPlay::HasText(Root, TEXT("Time (local)")));
            Test->TestTrue(TEXT("Shot time is local"), RenderedPlay::HasText(Root,
                FText::AsDateTime(FDateTime(2026, 11, 11), TEXT("%b%e")).ToString() + TEXT(" ")
                + FText::AsDateTime(FDateTime(2026, 11, 11), TEXT("%H%M")).ToString()));
            {
                FString Csv;
                ExportFile = Service->ExportHistory();
                Test->TestTrue(TEXT("Shot history exports CSV"), FFileHelper::LoadFileToString(Csv, *ExportFile));
                Test->TestTrue(TEXT("CSV keeps the raw UTC timestamp"), Csv.Contains(TEXT("Timestamp (UTC)"))
                    && Csv.Contains(TEXT("\"2026-11-11T00:00:00Z\"")));
            }
            Capture(Window, TEXT("history"));
            return false;
        case 9:
        {
            const auto ScrollWidget = RenderedPlay::WithTag(Root, TEXT("ShotTableScroll"));
            if (!ScrollWidget) { Test->AddError(TEXT("Missing horizontal shot table")); Cleanup(); return true; }
            auto Scroll = StaticCastSharedPtr<SScrollBox>(ScrollWidget);
            Test->TestTrue(TEXT("Full table exceeds viewport width"), Scroll->GetScrollOffsetOfEnd() > 0);
            Scroll->SetScrollOffset(Scroll->GetScrollOffsetOfEnd());
            Stage = 30;
            Until = GetCurrentRunTime() + 0.25;
            return false;
        }
        case 30:
        {
            auto Scroll = StaticCastSharedPtr<SScrollBox>(RenderedPlay::WithTag(Root, TEXT("ShotTableScroll")));
            Test->TestTrue(TEXT("Shot table scrolls to the far end"), Scroll && Scroll->GetScrollOffset() > 0 &&
                FMath::IsNearlyEqual(Scroll->GetScrollOffset(),Scroll->GetScrollOffsetOfEnd(),1.f));
            Stage = 9;
            return Next(ClickOn(Window, RenderedPlay::WithTag(Root, PageBackTag), TEXT("back from one club")));
        }
        case 10:
            if (Service->IsBusy())
                return false;
            Test->TestTrue(TEXT("Back returns to the club list"), RenderedPlay::HasText(Root, TEXT("7i")) && !RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            return Next(ClickOn(Window, RenderedPlay::WithTag(Root, ClubRowTag), TEXT("the 7i row again")));
        case 11:
            if (Service->IsBusy())
                return false;
            Test->TestTrue(TEXT("The club's shots reopen"), RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            // Leave one club's shots by the cog rather than Back.
            return Next(ClickOn(Window, RenderedPlay::WithTag(Root, TEXT("MenuSettings")), TEXT("the cog from one club's shots")));
        case 12:
            Test->TestTrue(TEXT("The cog returns to Player"), RenderedPlay::HasText(Root, TEXT("Display name")));
            Test->TestFalse(TEXT("Club averages stay on their own page"), RenderedPlay::HasText(Root, TEXT("Club averages  /  Last 90 days")));
            return Next(ClickOn(Window, RenderedPlay::FindButton(Root, TEXT("View")), TEXT("Shot history's View again")));
        case 13:
            if (Service->IsBusy())
                return false;
            // A cached page would reopen on the last club's shots.
            Test->TestTrue(TEXT("View reopens on the club averages"), RenderedPlay::HasText(Root, TEXT("Shot history")) && RenderedPlay::HasText(Root, TEXT("7i")));
            Test->TestFalse(TEXT("View does not reopen the last club's shots"), RenderedPlay::HasText(Root, TEXT("7i — shot history")));
            return Next(ClickOn(Window, RenderedPlay::WithTag(Root, PageBackTag), TEXT("back from shot history")));
        case 14:
            Test->TestTrue(TEXT("Back returns to Player"), RenderedPlay::HasText(Root, TEXT("Display name")));
            return Next(ClickOn(Window, RenderedPlay::WithTag(Root, TEXT("AccountSignOut")), TEXT("the logout icon")));
        case 15:
            Test->TestFalse(TEXT("The logout icon ends the session"), Service->HasSession());
            ExpectTopBar(TEXT("AccountSignIn"), TEXT("AccountSignOut"), TEXT("after logout"));
            ExpectSignInChoices(TEXT("after logout"));
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
    TSharedPtr<SWidget> SettingsPage;
    void Start(UWorld& PlayWorld, AUOpenGolfPlayerController& Player)
    {
        StorageRoot = FPaths::ProjectSavedDir() / TEXT("Automation") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
        ScreenRoot = FPaths::ProjectSavedDir() / TEXT("Screenshots/Online") / FGuid::NewGuid().ToString(EGuidFormats::Digits);
        Service.Reset(NewObject<UOpenGolfOnlineSubsystem>(PlayWorld.GetGameInstance()));
        OpenGolfOnline::FConfig Config;
        Config.Authority = TEXT("https://login.example.test/"); Config.ClientId = TEXT("test"); Config.Audience = TEXT("https://api.example.test");
        Config.ApiOrigin = TEXT("https://api.example.test"); Config.ApplicationName = TEXT("UOpenGolf"); Config.StorageName = TEXT("OnlineVisualTest");
        Config.CallbackPorts = {47801};
        Service->SetupForTests(Config, [this](const FString& Url, const FString&, const FString& Request, const FString&, OpenGolfOnline::FHttpCallback Done)
        {
            FString Body = TEXT("{}");
            if (Url.EndsWith(TEXT("oauth/device/code"))) Body = TEXT("{\"device_code\":\"test-only\",\"user_code\":\"TEST-CODE\",\"verification_uri\":\"https://login.example.test/activate\",\"verification_uri_complete\":\"https://login.example.test/activate?user_code=TEST-CODE\",\"expires_in\":600,\"interval\":5}");
            else if (Url.EndsWith(TEXT("oauth/token")) && Request.Contains(TEXT("device_code")) && !bApproveQr)
            {
                // The home page asks for the QR code before the test reaches Settings, so its poll waits for the test.
                Done({400, TEXT("{\"error\":\"authorization_pending\"}"), true});
                auto Pending = MakeShared<OpenGolfOnline::FRequestCancellation, ESPMode::ThreadSafe>();
                Pending->Finished.store(true);
                return Pending;
            }
            else if (Url.EndsWith(TEXT("oauth/token"))) Body = TEXT("{\"access_token\":\"test\",\"refresh_token\":\"test\",\"token_type\":\"Bearer\",\"expires_in\":3600}");
            else if (Url.EndsWith(TEXT("userinfo"))) Body = TEXT("{\"sub\":\"test|visual\"}");
            else if (Url.EndsWith(TEXT("/sync"))) Body = TEXT("{\"id\":\"test-player\",\"display_name\":\"Test Golfer\",\"created_at\":\"2026-11-11T00:00:00Z\"}");
            else if (Url.Contains(TEXT("/clubs/carry")))
            {
                const double WoodCarry = CarryRequests++ == 0 ? 225.0 : 250.0;
                Body = FString::Printf(TEXT("[{\"club\":\"PW\",\"shots\":6,\"avg_carry_yd\":101.2},"
                    "{\"club\":\"GW\",\"shots\":4},{\"club\":\"DR\",\"shots\":5,\"avg_carry_yd\":225},"
                    "{\"club\":\"5W\",\"shots\":3,\"avg_carry_yd\":%.1f},{\"club\":\"7I\",\"shots\":12,\"avg_carry_yd\":150.5}]"), WoodCarry);
            }
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

    bool ClickOn(const TSharedRef<SWindow>& Window, const TSharedPtr<SWidget>& Widget, const FString& Label)
    {
        if (Widget.IsValid())
            FSlateApplication::Get().SetCursorPos(RenderedPlay::PointOn(*Widget));
        return RenderedPlay::ClickOn(*Test, Window, Widget, Label);
    }

    void ExpectClubOrder(const TSharedRef<SWidget>& Root, TArrayView<const TCHAR* const> Clubs)
    {
        TOptional<double> PreviousY;
        for (const TCHAR* Club : Clubs)
        {
            const FName Tag(*(FString(TEXT("ClubRow:")) + Club));
            const TSharedPtr<SWidget> Row = RenderedPlay::WithTag(Root, Tag);
            if (!Test->TestTrue(FString::Printf(TEXT("Shows club %s"), Club), Row.IsValid()))
                return;
            const double Y = RenderedPlay::PointOn(*Row).Y;
            if (PreviousY.IsSet())
                Test->TestTrue(FString::Printf(TEXT("Club %s follows the previous club"), Club), Y > PreviousY.GetValue());
            PreviousY = Y;
        }
    }

    /** The account icon for the session shows alone, left of the cog; the launch monitor's status is only in Settings. */
    void ExpectTopBar(const TCHAR* ShownTag, const TCHAR* HiddenTag, const FString& Where)
    {
        using namespace OnlineVisualTestDetail;
        const TSharedRef<SWidget> Root = Menu.ToSharedRef();
        const TSharedPtr<SWidget> Shown = RenderedPlay::WithTag(Root, ShownTag), Cog = RenderedPlay::WithTag(Root, TEXT("MenuSettings"));
        Test->TestTrue(FString(ShownTag) + TEXT(" shows ") + Where, IsShownWidget(Shown));
        Test->TestFalse(FString(HiddenTag) + TEXT(" hides ") + Where, IsShownWidget(RenderedPlay::WithTag(Root, HiddenTag)));
        Test->TestTrue(TEXT("The cog shows ") + Where, IsShownWidget(Cog));
        Test->TestFalse(TEXT("No launch-monitor status in the top bar ") + Where, RenderedPlay::WithTag(Root, TEXT("MenuLaunchMonitor")).IsValid());
        if (IsShownWidget(Shown) && IsShownWidget(Cog))
            Test->TestTrue(TEXT("Account icon, then cog, ") + Where, RenderedPlay::PointOn(*Shown).X < RenderedPlay::PointOn(*Cog).X);
    }

    /**
     * Signed out: Sign In and Create Account side by side on Player, and the QR sign-in in the Settings categories
     * column, below the last category.
     */
    void ExpectSignInChoices(const FString& Where)
    {
        using namespace OnlineVisualTestDetail;
        const TSharedRef<SWidget> Root = Menu.ToSharedRef();
        const TSharedPtr<SWidget> SignIn = RenderedPlay::FindButton(Root, TEXT("Sign In")), Create = RenderedPlay::FindButton(Root, TEXT("Create Account"));
        const TSharedPtr<SWidget> Qr = RenderedPlay::WithTag(Root, TEXT("SignInQr")), LastCategory = RenderedPlay::FindButton(Root, TEXT("Data & system"));
        Test->TestTrue(TEXT("Sign In and Create Account show ") + Where, SignIn.IsValid() && Create.IsValid());
        for (const TCHAR* Label : {TEXT("Sign in with QR"), TEXT("Sign in on this computer")})
            Test->TestFalse(FString(Label) + TEXT(" is gone ") + Where, RenderedPlay::HasText(Root, Label));
        if (SignIn.IsValid() && Create.IsValid())
            Test->TestTrue(TEXT("Sign In, then Create Account, on one row ") + Where,
                RenderedPlay::PointOn(*SignIn).X < RenderedPlay::PointOn(*Create).X
                && FMath::IsNearlyEqual(RenderedPlay::PointOn(*SignIn).Y, RenderedPlay::PointOn(*Create).Y, 1.0));
        Test->TestTrue(TEXT("The QR sign-in shows ") + Where, IsShownWidget(Qr) && RenderedPlay::HasText(Root, TEXT("Sign-In")));
        if (IsShownWidget(Qr) && LastCategory.IsValid() && SignIn.IsValid())
            Test->TestTrue(TEXT("The QR sign-in is under the last category, left of the page, ") + Where,
                RenderedPlay::PointOn(*Qr).Y > RenderedPlay::PointOn(*LastCategory).Y && RenderedPlay::PointOn(*Qr).X < RenderedPlay::PointOn(*SignIn).X);
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
        }
        if (Service.IsValid()) Service->CancelSignIn();
        Menu.Reset();
        Service.Reset();
        if (!ExportFile.IsEmpty())
        {
            IFileManager::Get().Delete(*ExportFile);
            IFileManager::Get().DeleteDirectory(*FPaths::GetPath(ExportFile));
        }
        if (!StorageRoot.IsEmpty()) IFileManager::Get().DeleteDirectory(*StorageRoot, false, true);
        FScreenshotRequest::Reset();
    }

    FAutomationTestBase* Test;
    TStrongObjectPtr<UOpenGolfOnlineSubsystem> Service;
    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TSharedPtr<SMainMenu> Menu;
    OpenGolfOnline::FHttpCallback DeferredProfileReply;
    int32 Stage = 0, CarryRequests = 0;
    double Until = 0, ProfileReadyAt = 0;
    /** Until set, the stub answers the QR code's poll with authorization_pending. */
    bool bApproveQr = false;
    FString File, StorageRoot, ScreenRoot, ProfileBody, ExportFile;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOnlineVisualTest, "UOpenGolf.Visual.Online",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter | EAutomationTestFlags::NonNullRHI)
bool FOnlineVisualTest::RunTest(const FString&)
{
    if (!FEditorFileUtils::LoadMap(TestMaps::Range, false, true))
        return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FOnlineVisualCapture>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
