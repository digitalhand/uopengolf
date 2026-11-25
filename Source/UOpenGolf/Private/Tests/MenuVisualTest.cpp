// SPDX-License-Identifier: MIT
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "TestMaps.h"
#include "Tests/RenderedPlay.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "UOpenGolfPlayerController.h"
#include "OpenGolfOnlineSubsystem.h"
#include "Engine/GameInstance.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfRange.h"
#include "OpenGolfEnvironment.h"
#include "GolfGimmieIndicator.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

namespace
{
    /** A step that presses Escape instead of clicking a button. */
    const TCHAR* EscapeStep=TEXT("@Escape");
    /** A step that clicks the page header's back icon, found by its tag. */
    const TCHAR* BackStep=TEXT("#PageBack");
    bool MenuIconShown(const TSharedPtr<SWidget>& Widget){return Widget.IsValid()&&Widget->GetVisibility().IsVisible();}
    /**
     * The menu pages have no Settings button: the top bar's cog is the way in. Left of it sits one account icon,
     * matching the session. The launch monitor's status is in Settings › Launch Monitor, not the top bar.
     */
    void ExpectMenuTopBar(FAutomationTestBase& Test,const TSharedRef<SWindow>& Window,const UOpenGolfOnlineSubsystem* Online,const FString& Where)
    {
        Test.TestFalse(TEXT("No Settings button on ")+Where,RenderedPlay::FindButton(Window,TEXT("Settings")).IsValid());
        Test.TestFalse(TEXT("No launch-monitor status in the top bar on ")+Where,RenderedPlay::WithTag(Window,TEXT("MenuLaunchMonitor")).IsValid());
        const TSharedPtr<SWidget> SignIn=RenderedPlay::WithTag(Window,TEXT("AccountSignIn"));
        const TSharedPtr<SWidget> SignOut=RenderedPlay::WithTag(Window,TEXT("AccountSignOut")),Cog=RenderedPlay::WithTag(Window,TEXT("MenuSettings"));
        const bool bSignedIn=Online&&Online->HasSession();
        const TSharedPtr<SWidget> Account=bSignedIn?SignOut:SignIn;
        Test.TestTrue(TEXT("The account icon matches the session on ")+Where,MenuIconShown(Account)&&!MenuIconShown(bSignedIn?SignIn:SignOut));
        Test.TestTrue(TEXT("The cog shows on ")+Where,MenuIconShown(Cog));
        if(MenuIconShown(Account)&&MenuIconShown(Cog))
            Test.TestTrue(TEXT("The account icon, then the cog, on ")+Where,RenderedPlay::PointOn(*Account).X<RenderedPlay::PointOn(*Cog).X);
    }
}
class FMenuVisualCapture : public IAutomationLatentCommand
{
public:
    explicit FMenuVisualCapture(FAutomationTestBase* T):Test(T){}
    bool Update() override
    {
        if(GetCurrentRunTime()>120){Test->AddError(TEXT("Menu visual capture timed out"));FScreenshotRequest::Reset();return true;}
        UWorld* W=GEditor->PlayWorld;auto* P=W?Cast<AUOpenGolfPlayerController>(W->GetFirstPlayerController()):nullptr;
        auto* M=W&&W->HasBegunPlay()?W->GetAuthGameMode<AUOpenGolfGameMode>():nullptr;
        if(!P||!M||!M->GetRange()||!W->GetGameViewport())return false;
        auto Window=W->GetGameViewport()->GetWindow();if(!Window)return false;
        if(GetCurrentRunTime()<WaitUntil)return false;
        if(!bOpened){P->OpenMainMenu();bOpened=true;WaitUntil=GetCurrentRunTime()+3;return false;}
        // Each step captures the screen or clicks a button by its label. The provider drop-down switches at once,
        // so the test switches to Open Connect and back; nothing is left pending, and Back needs no dialog.
        // Under -NoMainMenu the switch is not written to the player's preferences file.
        struct FStep{const TCHAR* Capture;const TCHAR* Click;};
        static const FStep Steps[] = {
            {TEXT("home"), nullptr}, {nullptr, TEXT("#MenuSettings")}, {TEXT("player"), nullptr},
            {nullptr, TEXT("Video")}, {TEXT("video"), nullptr},
            {nullptr, TEXT("Environment")}, {TEXT("environment-day"), nullptr},
            {nullptr, TEXT("Sunset")}, {TEXT("environment-sunset"), nullptr},
            {nullptr, TEXT("Reset to course defaults")}, {TEXT("environment-reset"), nullptr},
            {nullptr, TEXT("Launch Monitor")}, {TEXT("launch-monitor"), nullptr}, {nullptr, TEXT("Square Home")},
            {nullptr, TEXT("Open Connect")}, {TEXT("open-connect"), nullptr}, {nullptr, TEXT("Open Connect")},
            {nullptr, TEXT("Square Home")}, {TEXT("square-home-again"), nullptr}, {nullptr, BackStep},
            {TEXT("gimmie-hud"), nullptr}, {nullptr, EscapeStep}, {TEXT("pause"), nullptr}};
        if(!File.IsEmpty())
        {
            if(!IFileManager::Get().FileExists(*File))return false;
            Test->AddInfo(TEXT("Captured ")+File);File.Empty();
            if(++Stage==UE_ARRAY_COUNT(Steps))return true;
            // Give Slate a new frame to refresh its hit-test grid after a screenshot.
            WaitUntil=GetCurrentRunTime()+.2;
            return false;
        }
        const FStep& Step=Steps[Stage];
        if(Step.Capture)
        {
            if (FString(Step.Capture).StartsWith(TEXT("environment-")))
            {
                auto* Environment = AOpenGolfEnvironment::Find(W);
                if (!Test->TestNotNull(TEXT("Environment controls have a target"), Environment)) return true;
                const float Expected = FString(Step.Capture) == TEXT("environment-sunset") ? 1.f : .5f;
                Test->TestEqual(TEXT("Real menu clicks apply preset and reset"), Environment->GetSettings().Daylight, Expected);
            }
            // The Training HUD names its icons in HUD-styled tooltips; the menu pages have none.
            RenderedPlay::ExpectNoToolTips(*Test,Window.ToSharedRef(),Step.Capture,FCString::Strcmp(Step.Capture,TEXT("gimmie-hud"))==0);
            if(FCString::Strcmp(Step.Capture,TEXT("home"))==0||FCString::Strcmp(Step.Capture,TEXT("pause"))==0)
                ExpectMenuTopBar(*Test,Window.ToSharedRef(),W->GetGameInstance()->GetSubsystem<UOpenGolfOnlineSubsystem>(),Step.Capture);
            File=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/Menu")/FString::Printf(TEXT("%s-%s.png"),*FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")),Step.Capture));
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(File),true);FScreenshotRequest::RequestScreenshot(File,true,false);return false;
        }
        const FString Label=Step.Click;
        if(Label==EscapeStep)
        {
            // Escape during Training opens the pause menu.
            // Route through the game so the editor's Escape shortcut does not end Play.
            P->ForwardUIKey(EKeys::Escape,true);
            ++Stage;WaitUntil=GetCurrentRunTime()+2;return false;
        }
        // A drop-down may open in its own window, so every visible window is searched, front-most first.
        // A label starting with # names a widget tag instead of a button label.
        TArray<TSharedRef<SWindow>> Windows;FSlateApplication::Get().GetAllVisibleWindowsOrdered(Windows);
        TSharedPtr<SWidget> B;TSharedPtr<SWindow> Owner;
        const bool bByTag=Label.StartsWith(TEXT("#"));
        for(int32 I=Windows.Num()-1;I>=0 && !B;--I)
            if((B=bByTag?RenderedPlay::WithTag(Windows[I],FName(*Label.Mid(1))):RenderedPlay::FindButton(Windows[I],Label)))Owner=Windows[I];
        if(!B){Test->AddError(TEXT("Missing menu button: ")+Label);return true;}
        const FVector2D At=RenderedPlay::PointOn(*B);const FWidgetPath Path=RenderedPlay::PathAt(Owner.ToSharedRef(),At);
        if(!Path.IsValid()||!Path.ContainsWidget(B.Get()))
        {
            FString Hit;for(int32 I=0;I<Path.Widgets.Num();++I)Hit+=Path.Widgets[I].Widget->GetTypeAsString()+TEXT(" ");
            Test->AddError(FString::Printf(TEXT("A click on '%s' at %s does not reach it; hit path: %s"),*Label,*At.ToString(),*Hit));return true;
        }
        RenderedPlay::Click(Owner.ToSharedRef(),At);
        if(Label==BackStep)
        {
            // Escape from the home page resumes training through the actual Slate menu handler.
            FKeyEvent Escape(EKeys::Escape,FModifierKeysState(),0,false,0,0);
            FSlateApplication::Get().ProcessKeyDownEvent(Escape);
            M->SetPlayMode(EOpenGolfPlayMode::Hole);
            // A visible host, as the game mode uses: components of the hidden game mode never render.
            AActor* Host=W->SpawnActor<AActor>();auto* Ring=NewObject<UGolfGimmieIndicator>(Host);Host->SetRootComponent(Ring);Ring->RegisterComponent();
            Ring->UpdateIndicator(M->GetRange()->Terrain,M->GetCupLocation(),5,true);
            Test->TestTrue(TEXT("Ring mesh created"),Ring->GetNumSections()==2);
            if(auto* Section=Ring->GetProcMeshSection(1))
            {
                // The inner perimeter is exactly five feet from the cup in horizontal distance. Each band has its own
                // rows (GroundBands), so the edge's inner row comes first.
                Test->TestEqual(TEXT("Ring inner radius matches feet conversion"),float(FVector::Dist2D(Section->ProcVertexBuffer[0].Position,M->GetCupLocation())),152.4f,.1f);
            }
            auto* Camera=W->SpawnActor<ACameraActor>();const FVector Offset(-700,0,550);
            Camera->SetActorLocation(M->GetCupLocation()+Offset);Camera->SetActorRotation((-Offset).Rotation());Camera->GetCameraComponent()->SetFieldOfView(60);P->SetViewTarget(Camera);
        }
        ++Stage;WaitUntil=GetCurrentRunTime()+2;return false;
    }
private:FAutomationTestBase* Test;int32 Stage=0;bool bOpened=false;double WaitUntil=0;FString File;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMenuVisualTest,"UOpenGolf.Visual.Menu",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter|EAutomationTestFlags::NonNullRHI)
bool FMenuVisualTest::RunTest(const FString&)
{
    if(!FEditorFileUtils::LoadMap(TestMaps::Range,false,true))return false;
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<RenderedPlay::FStartInWindow>());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<FMenuVisualCapture>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
#endif
