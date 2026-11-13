// SPDX-License-Identifier: MIT
#include "UI/Menu/MainMenu.h"
#include "OpenGolfOnlineSubsystem.h"
#include "SOpenGolfAccountPanel.h"
#include "SOpenGolfAccountButton.h"
#include "UI/Common/HudStyle.h"
#include "UI/Menu/MainMenuInternal.h"
#include "UI/Common/MenuStyle.h"
#include "UI/Common/PageHeader.h"
#include "UI/Hud/ShotDisplay.h"
#include "OpenGolfGameUserSettings.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfHUD.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"

using namespace MainMenuDetail;
namespace
{
    /** Null when the project settings class is not configured; render resolutions are then not offered. */
    UOpenGolfGameUserSettings* RenderSettings() { return Cast<UOpenGolfGameUserSettings>(EngineSettings()); }
    // Confirmation dialogs, identified by their titles.
    const TCHAR* MainMenuDialog = TEXT("Return to the main menu?");
    const TCHAR* ExitDialog = TEXT("Exit to desktop?");
}
bool SMainMenu::FVideo::operator==(const FVideo& Other) const
{
    if (Resolution != Other.Resolution || Mode != Other.Mode || VSync != Other.VSync || FPS != Other.FPS || Scale != Other.Scale) return false;
    for (int32 I=0; I<10; ++I) if (Quality[I] != Other.Quality[I]) return false;
    return true;
}
DisplayOptions::FFacts SMainMenu::Facts()
{
    DisplayOptions::FFacts F;
    F.bBorderlessIsFullscreen=BorderlessIsFullscreen;
    UKismetSystemLibrary::GetSupportedFullscreenResolutions(F.RhiModes);
    if(const auto* S=EngineSettings()) F.Desktop=S->GetDesktopResolution();
    if(FSlateApplication::IsInitialized())
    {
        FDisplayMetrics Metrics; FSlateApplication::Get().GetCachedDisplayMetrics(Metrics);
        const FPlatformRect& Work=Metrics.PrimaryDisplayWorkAreaRect;
        F.WorkArea=FIntPoint(Work.Right-Work.Left,Work.Bottom-Work.Top);
    }
    // Without the project settings class a render resolution cannot be stored, so desktop-sized modes offer the desktop only.
    F.bRenderResolution=RenderSettings()!=nullptr;
    return F;
}
SMainMenu::FVideo SMainMenu::ReadVideo()
{
    FVideo V;
    if (auto* S = EngineSettings())
    {
        const DisplayOptions::FFacts F=Facts();
        V.Mode=DisplayOptions::ShownMode(int32(S->GetFullscreenMode()),BorderlessIsFullscreen);
        const auto* R=RenderSettings();
        const FIntPoint Stored=R?R->GetRenderResolution():S->GetScreenResolution();
        V.Resolution=DisplayOptions::Choose(DisplayOptions::Resolutions(V.Mode,F,Stored),Stored);
        V.VSync=S->IsVSyncEnabled(); V.FPS=FMath::RoundToInt(S->GetFrameRateLimit());
        // A zero resolution quality is the engine's "project default", which renders at full size.
        float Normalized, Current, Min, Max; S->GetResolutionScaleInformationEx(Normalized, Current, Min, Max); V.Scale=Current>0?FMath::RoundToInt(Current):100;
        const int32 Q[] = {S->GetViewDistanceQuality(),S->GetAntiAliasingQuality(),S->GetShadowQuality(),S->GetGlobalIlluminationQuality(),S->GetReflectionQuality(),S->GetPostProcessingQuality(),S->GetTextureQuality(),S->GetVisualEffectQuality(),S->GetFoliageQuality(),S->GetShadingQuality()};
        for(int32 I=0;I<10;++I)V.Quality[I]=Q[I];
    }
    return V;
}
void SMainMenu::Construct(const FArguments& Args)
{
    Controller=Args._Controller;
    OnlineOverride=Args._Online;
    Style=MenuStyle::Make();
    NavStyle=MenuStyle::NavButton();
    SetVisibility(EVisibility::Collapsed);
    ChildSlot[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(HudStyle::LaunchTileColor()).Padding(32)
        [SNew(SOverlay)
            +SOverlay::Slot()[SNew(SVerticalBox)
                +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,20)
                    [SNew(SBox).IsEnabled_Lambda([this]{return !bConfirmDisplay && Dialog.IsEmpty();})[BuildTopBar()]]
                +SVerticalBox::Slot().FillHeight(1)[SAssignNew(Content,SBox).Tag(TEXT("MenuPageHost"))
                    .IsEnabled_Lambda([this]{return !bConfirmDisplay && Dialog.IsEmpty();})]
                +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0,8,0,0)
                    [Text(ShotDisplay::VersionLabel(ShotDisplay::ProjectVersion()),MenuStyle::HelpSize,true)]]
            +SOverlay::Slot()[SAssignNew(ModalContent,SBox)
                .Visibility_Lambda([this]{return bConfirmDisplay || !Dialog.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;})]]];
    const UOpenGolfOnlineSubsystem* Online=OnlineService();
    bWasSignedIn=Online&&Online->HasSession();
}
SMainMenu::~SMainMenu() { RevertDisplay(); }
TSharedRef<SWidget> SMainMenu::Text(const FString& Value,int32 Size,bool Muted)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font_Lambda([Size]{return HudStyle::Font("Regular",Size);})
        .ColorAndOpacity(Muted?HudStyle::LabelColor():HudStyle::TextColor()).AutoWrapText(true);
}
TSharedRef<SWidget> SMainMenu::Button(const FString& Label,TFunction<void()> Action,bool Primary,int32 Size)
{
    return SNew(SButton).ButtonStyle(Primary?&Style.PrimaryButton:&Style.Button).OnClicked_Lambda([Action]{Action();return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(Label)).Font(HudStyle::Font("Regular",Size)).ColorAndOpacity(Primary?Style.PrimaryLabelColor:Style.TextColor)];
}
TSharedRef<SWidget> SMainMenu::NavButton(const FString& Label,TFunction<void()> Action,bool Selected)
{
    return SNew(SButton).ButtonStyle(Selected?&Style.PrimaryButton:&NavStyle).OnClicked_Lambda([Action]{Action();return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(Label)).Font(HudStyle::Font("Regular",MenuStyle::NavSize)).ColorAndOpacity(Selected?Style.PrimaryLabelColor:Style.TextColor)];
}
void SMainMenu::AddMenuButton(TSharedRef<SVerticalBox> Body,const FString& Label,TFunction<void()> Action,bool Primary)
{
    Body->AddSlot().AutoHeight().HAlign(HAlign_Left).Padding(0,0,0,MenuStyle::ButtonGap)[SNew(SBox).WidthOverride(MenuStyle::MenuButtonWidth)[Button(Label,MoveTemp(Action),Primary)]];
}
UOpenGolfOnlineSubsystem* SMainMenu::OnlineService() const
{
    if(OnlineOverride.IsValid())return OnlineOverride.Get();
    const UGameInstance* Instance=Controller.IsValid()?Controller->GetGameInstance():nullptr;
    return Instance?Instance->GetSubsystem<UOpenGolfOnlineSubsystem>():nullptr;
}
bool SMainMenu::HasProfile() const
{
    const UOpenGolfOnlineSubsystem* Online=OnlineService();
    return Online&&Online->HasSession()&&Online->GetProfile().IsValid();
}
void SMainMenu::Open()
{
    bOpen=true; Route=bStarted?ERoute::Pause:ERoute::Home; bReturnToTraining=false; Dialog.Empty(); Search.Empty(); SetVisibility(EVisibility::Visible);
    if(Controller.IsValid()) Controller->SetMainMenuOpen(true);
    Rebuild();
    if(FSlateApplication::IsInitialized())FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}
void SMainMenu::OpenLaunchMonitorSettings()
{
    const bool bFromTraining=!bOpen;
    if(!bOpen) Open();
    bReturnToTraining=bFromTraining;
    OpenSettings(SimulatorCategory);
}
UOpenLaunchMonitorSubsystem* SMainMenu::Monitor() const
{
    const UGameInstance* Instance=Controller.IsValid()?Controller->GetGameInstance():nullptr;
    return Instance?Instance->GetSubsystem<UOpenLaunchMonitorSubsystem>():nullptr;
}
FString SMainMenu::ConnectionSummary() const
{
    const UOpenLaunchMonitorSubsystem* M=Monitor();
    if(!M)return TEXT("LAUNCH MONITOR UNAVAILABLE");
    return (TEXT("LAUNCH MONITOR  ")+StaticEnum<ELaunchMonitorConnectionState>()->GetDisplayNameTextByValue(int64(M->GetConnectionState())).ToString()).ToUpper();
}
void SMainMenu::OpenSettings(int32 InCategory,bool bDeferRebuild)
{
    if(bConfirmDisplay || !Dialog.IsEmpty()) return;
    if(!InSettings()) { Saved=Draft=GolfSettings::Get(); SavedVideo=Video=ReadVideo(); Pages.Empty(); }
    Route=ERoute::Settings;Category=InCategory;Search.Empty();Dialog.Empty();
    if(bDeferRebuild)bRebuildPending=true;else Rebuild();
}
void SMainMenu::OnSettingsIcon()
{
    if(bConfirmDisplay||!Dialog.IsEmpty())return;
    if(InShots()){Route=ERoute::Settings;bRebuildPending=true;return;}
    if(!InSettings())OpenSettings(PlayerCategory,true);
}
void SMainMenu::OnSignInIcon()
{
    if(bConfirmDisplay||!Dialog.IsEmpty())return;
    if(!InSettings()){OpenSettings(PlayerCategory,true);return;}
    Route=ERoute::Settings;Category=PlayerCategory;Search.Empty();bRebuildPending=true;
}
void SMainMenu::Resume()
{
    FSlateApplication::Get().DismissAllMenus();
    RevertDisplay(); bOpen=false; bStarted=true; SetVisibility(EVisibility::Collapsed);
    if(Controller.IsValid()) Controller->SetMainMenuOpen(false);
}
bool SMainMenu::Dirty() const { return Draft!=Saved || Video!=SavedVideo; }
void SMainMenu::Back()
{
    if(bConfirmDisplay){RevertDisplay();Rebuild();return;}
    if(!Dialog.IsEmpty()){Dialog.Empty();Rebuild();return;}
    // Shot history's back leaves one club's shots first, then returns to Settings › Player.
    if(InShots()){if(ShotsPanel.IsValid()&&ShotsPanel->HandleBack())return;Route=ERoute::Settings;Rebuild();return;}
    if(InSettings())
    {
        if(Dirty()) Dialog=TEXT("Unsaved changes"); else { if(bReturnToTraining){Resume();return;} Route=bStarted?ERoute::Pause:ERoute::Home; }
        Rebuild();
    }
    else Resume();
}
FReply SMainMenu::OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right){if(FSlateApplication::Get().AnyMenusVisible()) FSlateApplication::Get().DismissAllMenus(); else Back();return FReply::Handled();}
    if(InSettings() && !InShots() && Dialog.IsEmpty() && !bConfirmDisplay && (Event.GetKey()==EKeys::Gamepad_LeftShoulder || Event.GetKey()==EKeys::Gamepad_RightShoulder))
    {Category=(Category+(Event.GetKey()==EKeys::Gamepad_LeftShoulder?CategoryCount-1:1))%CategoryCount;Search.Empty();Rebuild();return FReply::Handled();}
    return FReply::Unhandled();
}
void SMainMenu::Tick(const FGeometry& G,double T,float D)
{
    SCompoundWidget::Tick(G,T,D);
    // Account widgets update locally; only leaving private history changes the route.
    const UOpenGolfOnlineSubsystem* Online=OnlineService();
    const bool bSignedIn=Online&&Online->HasSession();
    if(bSignedIn!=bWasSignedIn)
    {
        bWasSignedIn=bSignedIn;
        if(!bSignedIn && InShots()){Route=ERoute::Settings;bRebuildPending=true;}
    }
    if(bConfirmDisplay && FPlatformTime::Seconds()>=ConfirmDeadline){RevertDisplay();Rebuild();}
    else if(bRebuildPending){bRebuildPending=false;Rebuild();}
    if(bFocusPending){bFocusPending=false;FocusPage();}
    if(TransitionStart>=0)
    {
        const float Alpha=GolfSettings::Get().ReducedMotion?1.f:FMath::Clamp(float((T-TransitionStart)/0.16),0.f,1.f);
        const float Ease=1.f-FMath::Square(1.f-Alpha);
        Content->SetRenderOpacity(FMath::Lerp(0.35f,1.f,Ease));
        Content->SetRenderTransform(FSlateRenderTransform(FVector2f(0,8*(1-Ease))));
        if(Alpha>=1)TransitionStart=-1;
    }
}
void SMainMenu::RefreshSettings()
{
    Pages.Empty();
    bRebuildPending=true;
}
void SMainMenu::FocusPage()
{
    if(!bOpen || !FSlateApplication::IsInitialized())return;
    if(Dialog.IsEmpty() && !bConfirmDisplay)
        if(const auto* SavedFocus=PageFocus.Find(PageKey))
            if(auto Widget=SavedFocus->Pin(); Widget && Widget->IsEnabled() && Widget->GetVisibility().IsVisible())
            {FSlateApplication::Get().SetKeyboardFocus(Widget,EFocusCause::Navigation);return;}
    TFunction<TSharedPtr<SWidget>(TSharedRef<SWidget>)> First=[&First](TSharedRef<SWidget> Widget)->TSharedPtr<SWidget>
    {
        if(!Widget->GetVisibility().IsVisible() || !Widget->IsEnabled())return nullptr;
        if(Widget->SupportsKeyboardFocus())return Widget;
        auto* Children=Widget->GetChildren();
        for(int32 I=0;I<Children->Num();++I)if(auto Found=First(Children->GetChildAt(I)))return Found;
        return nullptr;
    };
    auto Target=First((bConfirmDisplay || !Dialog.IsEmpty())?ModalContent.ToSharedRef():Content.ToSharedRef());
    FSlateApplication::Get().SetKeyboardFocus(Target?Target:SharedThis(this),EFocusCause::Navigation);
}
void SMainMenu::Rebuild()
{
    const FString Next=InShots()?TEXT("shots"):InSettings()?FString::Printf(TEXT("settings:%d:%s"),Category,*Search):bStarted?TEXT("pause"):TEXT("home");
    if(PageKey!=Next && FSlateApplication::IsInitialized())PageFocus.Add(PageKey,FSlateApplication::Get().GetKeyboardFocusedWidget());
    if(bConfirmDisplay || !Dialog.IsEmpty())
    {
        if(FSlateApplication::IsInitialized())PageFocus.Add(PageKey,FSlateApplication::Get().GetKeyboardFocusedWidget());
        auto Body=SNew(SVerticalBox);BuildDialog(Body);
        ModalContent->SetContent(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(HudStyle::LaunchTileColor()).Padding(32)[Body]);
        bFocusPending=true;return;
    }
    const bool LeavingDialog=ModalContent->GetChildren()->GetChildAt(0)!=SNullWidget::NullWidget;
    auto* Cached=Pages.Find(Next);
    if(!Cached)
    {
        auto Body=SNew(SVerticalBox);
        if(InShots())BuildShots(Body);else if(InSettings())BuildSettings(Body);else BuildHome(Body);
        Pages.Add(Next,Body);Cached=Pages.Find(Next);
    }
    const bool Changed=PageKey!=Next || Content->GetChildren()->GetChildAt(0)!=Cached->ToSharedRef();
    Content->SetContent(Cached->ToSharedRef());
    ModalContent->SetContent(SNullWidget::NullWidget);
    if(Changed){TransitionStart=FSlateApplication::Get().GetCurrentTime();bFocusPending=true;}
    else if(LeavingDialog || !FSlateApplication::Get().GetKeyboardFocusedWidget().IsValid())bFocusPending=true;
    PageKey=Next;
}
TSharedRef<SWidget> SMainMenu::BuildTopBar()
{
    UOpenGolfOnlineSubsystem* Online=OnlineService();
    const TWeakObjectPtr<UOpenGolfOnlineSubsystem> Service=Online;
    // The header shows the real launch-monitor state; selecting it opens the connection settings. Right of it, one
    // account icon (each button shows itself only in its own session state), then the cog.
    return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("UOPEN GOLF"),22)]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).ButtonStyle(&Style.Button).Tag(TEXT("MenuLaunchMonitor"))
            .OnClicked_Lambda([this]{OpenSettings(SimulatorCategory,true);return FReply::Handled();})
            [SNew(STextBlock).Font(HudStyle::Font("Regular",14))
                .ColorAndOpacity_Lambda([this]{const UOpenLaunchMonitorSubsystem* M=Monitor();return FSlateColor(M?HudStyle::StateColor(M->GetConnectionState()):HudStyle::LabelColor());})
                .Text_Lambda([this]{return FText::FromString(ConnectionSummary());})]]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0,0,0)
        [SNew(SOpenGolfAccountButton).Online(Online).Style(Style).OnClicked_Lambda([this]{OnSignInIcon();})]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16,0,0,0)
        [SNew(SOpenGolfAccountButton).Online(Online).SignOut(true).Style(Style).OnClicked_Lambda([Service]{if(Service.IsValid())Service->SignOut();})]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(MenuStyle::ButtonGap,0,0,0)
        [SNew(SOpenGolfIconButton).Style(Style).Icon(EOpenGolfAccountIcon::Settings).Tag(TEXT("MenuSettings"))
            .Label(FText::FromString(TEXT("Settings"))).OnClicked_Lambda([this]{OnSettingsIcon();})];
}
void SMainMenu::BuildDialog(TSharedRef<SVerticalBox> Body)
{
    const FString Title=bConfirmDisplay?TEXT("Keep these display settings?"):Dialog;
    Body->AddSlot().AutoHeight().Padding(0,90,0,20)[Text(Title,MenuStyle::TitleSize)];
    if(bConfirmDisplay)
    {
        Body->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Font(HudStyle::Font("Regular",MenuStyle::BodySize)).ColorAndOpacity(Style.TextColor)
            .Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("Reverting in %d seconds"),FMath::Max(0,FMath::CeilToInt(ConfirmDeadline-FPlatformTime::Seconds()))));})];
        AddMenuButton(Body,TEXT("Keep"),[this]{bConfirmDisplay=false;EngineSettings()->ConfirmVideoMode();Commit();},true);
        AddMenuButton(Body,TEXT("Revert"),[this]{RevertDisplay();Rebuild();});
    }
    else if(Dialog==TEXT("Unsaved changes"))
    {
        AddMenuButton(Body,TEXT("Apply"),[this]{Dialog.Empty();Apply();},true);
        AddMenuButton(Body,TEXT("Discard and go back"),[this]{Draft=Saved;Video=SavedVideo;Dialog.Empty();Pages.Empty();if(bReturnToTraining){Resume();return;}Route=bStarted?ERoute::Pause:ERoute::Home;Rebuild();});
        AddMenuButton(Body,TEXT("Stay in Settings"),[this]{Dialog.Empty();Rebuild();});
    }
    else if(Dialog==MainMenuDialog)
    {
        Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Your training session will end. Recorded shots are saved and the launch monitor stays connected."),MenuStyle::BodySize,true)];
        AddMenuButton(Body,TEXT("Yes"),[this]{if(Controller.IsValid())Controller->ReturnToMainMenu();},true);
        AddMenuButton(Body,TEXT("Cancel"),[this]{Dialog.Empty();Rebuild();});
    }
    else
    {
        if(bStarted)Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Your training session will end."),MenuStyle::BodySize,true)];
        AddMenuButton(Body,TEXT("Yes"),[this]{if(Controller.IsValid())Controller->QuitGame();},true);
        AddMenuButton(Body,TEXT("Cancel"),[this]{Dialog.Empty();Rebuild();});
    }
    Body->AddSlot().FillHeight(1);
}
void SMainMenu::BuildShots(TSharedRef<SVerticalBox> Body)
{
    // The time zone applies at once in Settings › Player, so the saved choice is the one shown there.
    const GolfSettings::FTimeZoneChoice& Zone=GolfSettings::ChosenTimeZone();
    SAssignNew(ShotsPanel,SOpenGolfAccountPanel).Online(OnlineService()).Style(Style).ShowHeading(false)
        .Page(EOpenGolfAccountPage::Shots).TimeZone(Zone.Id).TimeZoneName(Zone.Short);
    // The panel changes between the club list and one club's shots without a menu rebuild, so the title follows it.
    const TWeakPtr<SOpenGolfAccountPanel> Panel=ShotsPanel;
    const TAttribute<FText> Title=TAttribute<FText>::CreateLambda([Panel]
    {const TSharedPtr<SOpenGolfAccountPanel> Pinned=Panel.Pin();return Pinned.IsValid()?Pinned->GetTitle():FText::GetEmpty();});
    Body->AddSlot().AutoHeight().Padding(0,0,0,16)[PageHeader::Make(Style,Title,FSimpleDelegate::CreateSP(this,&SMainMenu::Back))];
    Body->AddSlot().FillHeight(1)[ShotsPanel.ToSharedRef()];
}
void SMainMenu::BuildHome(TSharedRef<SVerticalBox> Body)
{
    // Once Training has started, Escape opens this page as the pause menu.
    Body->AddSlot().AutoHeight().Padding(0,100,0,12)[Text(bStarted?TEXT("Paused"):TEXT("Community golf simulation!"),16,true)];
    Body->AddSlot().AutoHeight().Padding(0,0,0,32)[Text(bStarted?TEXT("Training"):TEXT("Golf for Everyone."),54)];
    if(bStarted)
    {
        AddMenuButton(Body,TEXT("Resume training"),[this]{Resume();},true);
        AddMenuButton(Body,TEXT("Main menu"),[this]{Dialog=MainMenuDialog;Rebuild();});
        AddMenuButton(Body,TEXT("Exit to desktop"),[this]{Dialog=ExitDialog;Rebuild();});
    }
    else
    {
        AddMenuButton(Body,TEXT("Training"),[this]{Resume();},true);
        AddMenuButton(Body,TEXT("Exit"),[this]{Dialog=ExitDialog;Rebuild();});
    }
    Body->AddSlot().FillHeight(1);
}
void SMainMenu::Apply()
{
    if(!Dirty())return;
    auto* S=EngineSettings();if(!S)return;
    // Only a different window needs the Keep/Revert countdown; a render size inside the same window applies at once.
    const DisplayOptions::FFacts F=Facts();
    const DisplayOptions::FRequest Next=DisplayOptions::Plan(Video.Mode,Video.Resolution,F), Prev=DisplayOptions::Plan(SavedVideo.Mode,SavedVideo.Resolution,F);
    if(Next.EngineMode!=Prev.EngineMode || Next.ScreenResolution!=Prev.ScreenResolution)
    {
        S->SetScreenResolution(Next.ScreenResolution);S->SetFullscreenMode(EWindowMode::Type(Next.EngineMode));S->ApplyResolutionSettings(false);
        bConfirmDisplay=true;ConfirmDeadline=FPlatformTime::Seconds()+15;Rebuild();
    }
    else Commit();
}
void SMainMenu::RevertDisplay()
{
    if(!bConfirmDisplay)return;
    bConfirmDisplay=false;
    if(auto* S=EngineSettings())
    {
        const DisplayOptions::FRequest Prev=DisplayOptions::Plan(SavedVideo.Mode,SavedVideo.Resolution,Facts());
        S->SetScreenResolution(Prev.ScreenResolution);S->SetFullscreenMode(EWindowMode::Type(Prev.EngineMode));S->ApplyResolutionSettings(false);S->ConfirmVideoMode();
    }
    Video.Resolution=SavedVideo.Resolution;Video.Mode=SavedVideo.Mode;
}
void SMainMenu::Commit()
{
    auto* S=EngineSettings();if(!S)return;
    // In a desktop-sized mode the chosen resolution is the render size, from which the engine derives the render scale.
    auto* R=RenderSettings();
    if(R && DisplayOptions::DesktopSized(Video.Mode,Facts())) R->SetRenderResolution(Video.Resolution);
    else { if(R) R->ClearRenderResolution(); S->SetResolutionScaleValueEx(Video.Scale); }
    S->SetVSyncEnabled(Video.VSync!=0);S->SetFrameRateLimit(Video.FPS);
    S->SetViewDistanceQuality(Video.Quality[0]);S->SetAntiAliasingQuality(Video.Quality[1]);S->SetShadowQuality(Video.Quality[2]);S->SetGlobalIlluminationQuality(Video.Quality[3]);S->SetReflectionQuality(Video.Quality[4]);S->SetPostProcessingQuality(Video.Quality[5]);S->SetTextureQuality(Video.Quality[6]);S->SetVisualEffectQuality(Video.Quality[7]);S->SetFoliageQuality(Video.Quality[8]);S->SetShadingQuality(Video.Quality[9]);
    S->ApplyNonResolutionSettings();S->SaveSettings();
    SavedVideo=Video=ReadVideo();
    if (!GolfSettings::Commit(Draft,Controller.IsValid()?Controller->GetWorld():nullptr))
    { SaveError=TEXT("Video saved; could not save player preferences. Check storage and retry."); Rebuild(); return; }
    SaveError.Empty(); Saved=Draft=GolfSettings::Get();
    Style=MenuStyle::Make();NavStyle=MenuStyle::NavButton();
    if(Controller.IsValid())if(auto* Hud=Cast<AUOpenGolfHUD>(Controller->GetHUD()))Hud->RefreshTheme();
    RefreshSettings();
}
void SMainMenu::CommitNow(TFunction<void(GolfSettings::FPreferences&)> Change,const TCHAR* Error)
{
    // Saves and applies only this change; other unapplied edits stay pending in Draft.
    GolfSettings::FPreferences Next=GolfSettings::Get();
    Change(Next);
    if(!GolfSettings::Commit(Next,Controller.IsValid()?Controller->GetWorld():nullptr))
    {
        SaveError=Error;
    }
    else
    {
        SaveError.Empty();
        Change(Draft);
        Change(Saved);
    }
    RefreshSettings();
}
