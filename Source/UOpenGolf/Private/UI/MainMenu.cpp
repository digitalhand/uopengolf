// SPDX-License-Identifier: MIT
#include "UI/MainMenu.h"
#include "UI/HudStyle.h"
#include "UI/ShotDisplay.h"
#include "OpenGolfGameUserSettings.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfHUD.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    const TCHAR* Categories[] = {TEXT("Player"), TEXT("Simulator"), TEXT("Video"), TEXT("Audio"), TEXT("Camera & HUD"), TEXT("Accessibility"), TEXT("Game Defaults"), TEXT("Data & system")};
    UGameUserSettings* EngineSettings() { return GEngine ? GEngine->GetGameUserSettings() : nullptr; }
    /** Null when the project settings class is not configured; render resolutions are then not offered. */
    UOpenGolfGameUserSettings* RenderSettings() { return Cast<UOpenGolfGameUserSettings>(EngineSettings()); }
    // SDL gives Borderless and Fullscreen the same desktop-fullscreen window (FLinuxWindow::SetWindowMode).
    constexpr bool BorderlessIsFullscreen = PLATFORM_LINUX != 0;
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
    ButtonStyle=FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
    ButtonStyle.SetNormal(FSlateColorBrush(FLinearColor(0,0,0,0)));
    ButtonStyle.SetHovered(FSlateColorBrush(HudStyle::HoverTileColor()));
    ButtonStyle.SetPressed(FSlateColorBrush(HudStyle::HoverTileColor()));
    ButtonStyle.SetNormalPadding(FMargin(18,12)); ButtonStyle.SetPressedPadding(FMargin(18,12));
    PrimaryStyle=ButtonStyle; PrimaryStyle.SetNormal(FSlateColorBrush(HudStyle::AccentColor()));
    SetVisibility(EVisibility::Collapsed);
    ChildSlot[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)[SNew(SBox).WidthOverride(1280).HeightOverride(720)[SAssignNew(Content,SBox)]]];
}
SMainMenu::~SMainMenu() { RevertDisplay(); }
TSharedRef<SWidget> SMainMenu::Text(const FString& Value,int32 Size,bool Muted)
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(HudStyle::Font("Regular",Size))
        .ColorAndOpacity(Muted?HudStyle::LabelColor():HudStyle::TextColor()).AutoWrapText(true);
}
TSharedRef<SWidget> SMainMenu::Button(const FString& Label,TFunction<void()> Action,bool Primary,int32 Size)
{
    return SNew(SButton).ButtonStyle(Primary?&PrimaryStyle:&ButtonStyle).OnClicked_Lambda([Action]{Action();return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(Label)).Font(HudStyle::Font("Regular",Size)).ColorAndOpacity(Primary?FLinearColor::Black:HudStyle::TextColor())];
}
void SMainMenu::Open()
{
    bOpen=true; bSettings=false; Dialog.Empty(); Search.Empty(); SetVisibility(EVisibility::Visible);
    if(Controller.IsValid()) Controller->SetMainMenuOpen(true);
    Rebuild();
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
void SMainMenu::OpenSettings(int32 InCategory)
{
    Saved=Draft=GolfSettings::Get();SavedVideo=Video=ReadVideo();bSettings=true;Category=InCategory;Search.Empty();Dialog.Empty();Rebuild();
}
void SMainMenu::Resume()
{
    RevertDisplay(); bOpen=false; bStarted=true; SetVisibility(EVisibility::Collapsed);
    if(Controller.IsValid()) Controller->SetMainMenuOpen(false);
}
bool SMainMenu::Dirty() const { return Draft!=Saved || Video!=SavedVideo; }
void SMainMenu::Back()
{
    if(bConfirmDisplay){RevertDisplay();Rebuild();return;}
    if(!Dialog.IsEmpty()){Dialog.Empty();Rebuild();return;}
    if(bSettings)
    {
        if(Dirty()) Dialog=TEXT("Unsaved changes"); else bSettings=false;
        Rebuild();
    }
    else Resume();
}
FReply SMainMenu::OnPreviewKeyDown(const FGeometry&,const FKeyEvent& Event)
{
    if(Event.GetKey()==EKeys::Escape || Event.GetKey()==EKeys::Gamepad_FaceButton_Right){Back();return FReply::Handled();}
    if(bSettings && Dialog.IsEmpty() && !bConfirmDisplay && (Event.GetKey()==EKeys::Gamepad_LeftShoulder || Event.GetKey()==EKeys::Gamepad_RightShoulder))
    {Category=(Category+(Event.GetKey()==EKeys::Gamepad_LeftShoulder?7:1))%8;Search.Empty();Rebuild();return FReply::Handled();}
    return FReply::Unhandled();
}
void SMainMenu::Tick(const FGeometry& G,double T,float D)
{
    SCompoundWidget::Tick(G,T,D);
    if(bConfirmDisplay && FPlatformTime::Seconds()>=ConfirmDeadline){RevertDisplay();Rebuild();}
    else if(bRebuildPending){bRebuildPending=false;Rebuild();}
}
void SMainMenu::Rebuild()
{
    TSharedRef<SVerticalBox> Body=SNew(SVerticalBox);
    // The header shows the real launch-monitor state; selecting it opens the connection settings.
    Body->AddSlot().AutoHeight().Padding(0,0,0,20)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("UOPEN GOLF"),22)]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SButton).ButtonStyle(&ButtonStyle)
            .ToolTipText(FText::FromString(TEXT("Launch monitor connection settings")))
            .OnClicked_Lambda([this]{OpenSettings(SimulatorCategory);return FReply::Handled();})
            [SNew(STextBlock).Font(HudStyle::Font("Regular",14))
                .ColorAndOpacity_Lambda([this]{const UOpenLaunchMonitorSubsystem* M=Monitor();return FSlateColor(M?HudStyle::StateColor(M->GetConnectionState()):HudStyle::LabelColor());})
                .Text_Lambda([this]{return FText::FromString(ConnectionSummary());})]]];
    if(bConfirmDisplay || !Dialog.IsEmpty())
    {
        const FString Title=bConfirmDisplay?TEXT("Keep these display settings?"):Dialog;
        Body->AddSlot().AutoHeight().Padding(0,90,0,20)[Text(Title,32)];
        if(bConfirmDisplay)
        {
            Body->AddSlot().AutoHeight()[SNew(STextBlock).ColorAndOpacity(HudStyle::TextColor()).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("Reverting in %d seconds"),FMath::Max(0,FMath::CeilToInt(ConfirmDeadline-FPlatformTime::Seconds()))));})];
            Body->AddSlot().AutoHeight()[Button(TEXT("Keep"),[this]{bConfirmDisplay=false;EngineSettings()->ConfirmVideoMode();Commit();},true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Revert"),[this]{RevertDisplay();Rebuild();})];
        }
        else if(Dialog==TEXT("Unsaved changes"))
        {
            Body->AddSlot().AutoHeight()[Button(TEXT("Apply"),[this]{Dialog.Empty();Apply();},true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Discard and go back"),[this]{Draft=Saved;Video=SavedVideo;Dialog.Empty();bSettings=false;Rebuild();})];
            Body->AddSlot().AutoHeight()[Button(TEXT("Stay in Settings"),[this]{Dialog.Empty();Rebuild();})];
        }
        else if(Dialog==MainMenuDialog)
        {
            Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Your training session will end. Recorded shots are saved and the launch monitor stays connected."),18,true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Yes"),[this]{if(Controller.IsValid())Controller->ReturnToMainMenu();},true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Cancel"),[this]{Dialog.Empty();Rebuild();})];
        }
        else
        {
            if(bStarted)Body->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Your training session will end."),18,true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Yes"),[this]{if(Controller.IsValid())Controller->QuitGame();},true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Cancel"),[this]{Dialog.Empty();Rebuild();})];
        }
        Body->AddSlot().FillHeight(1);
    }
    else if(bSettings) BuildSettings(Body);
    else
    {
        if(bStarted)
        {
            // Pause menu: opened with Escape during Training.
            Body->AddSlot().AutoHeight().Padding(0,100,0,12)[Text(TEXT("Paused"),16,true)];
            Body->AddSlot().AutoHeight().Padding(0,0,0,32)[Text(TEXT("Training"),54)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Resume training"),[this]{Resume();},true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Settings"),[this]{OpenSettings(0);})];
            Body->AddSlot().AutoHeight()[Button(TEXT("Main menu"),[this]{Dialog=MainMenuDialog;Rebuild();})];
            Body->AddSlot().AutoHeight()[Button(TEXT("Exit to desktop"),[this]{Dialog=ExitDialog;Rebuild();})];
        }
        else
        {
            Body->AddSlot().AutoHeight().Padding(0,100,0,12)[Text(TEXT("Community golf simulation!"),16,true)];
            Body->AddSlot().AutoHeight().Padding(0,0,0,32)[Text(TEXT("Golf for Everyone."),54)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Training"),[this]{Resume();},true)];
            Body->AddSlot().AutoHeight()[Button(TEXT("Settings"),[this]{OpenSettings(0);})];
            Body->AddSlot().AutoHeight()[Button(TEXT("Exit"),[this]{Dialog=ExitDialog;Rebuild();})];
        }
        Body->AddSlot().FillHeight(1);
    }
    Body->AddSlot().AutoHeight().HAlign(HAlign_Right).Padding(0,12,0,0)[Text(ShotDisplay::VersionLabel(ShotDisplay::ProjectVersion()),12,true)];
    Content->SetContent(SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(bSettings?HudStyle::LaunchTileColor():HudStyle::TileColor()).Padding(40)[Body]);
    if(FSlateApplication::IsInitialized()) FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));
}
bool SMainMenu::Matches(const FString& Label,const FString& Help) const
{
    return Search.IsEmpty() || (Label+Help).Contains(Search,ESearchCase::IgnoreCase);
}
void SMainMenu::AddRow(TSharedRef<SVerticalBox> Rows,const FString& Label,const FString& Help,TSharedRef<SWidget> Control,bool Enabled)
{
    Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(1,1,1,.025f)).Padding(12)
        [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(Label,18,!Enabled)]+SVerticalBox::Slot().AutoHeight()[Text(Help,12,true)]]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(20,0)[SNew(SBox).WidthOverride(225)[Control]]]];
}
void SMainMenu::AddChoice(TSharedRef<SVerticalBox> Rows,const FString& Label,const FString& Help,TArray<FString> Labels,TArray<int32> Values,TFunction<int32()> Get,TFunction<void(int32)> Set,bool Enabled)
{
    if(!Matches(Label,Help)) return;
    auto Choice=SNew(SComboButton).IsEnabled(Enabled).ButtonStyle(&ButtonStyle)
        .OnGetMenuContent_Lambda([this,Labels,Values,Set]{
            TSharedRef<SVerticalBox> Options=SNew(SVerticalBox);
            for(int32 I=0;I<Labels.Num();++I) Options->AddSlot().AutoHeight()[Button(Labels[I],[Set,Values,I]{Set(Values[I]);FSlateApplication::Get().DismissAllMenus();})];
            return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HudStyle::LaunchTileColor())[SNew(SScrollBox)+SScrollBox::Slot()[Options]];
        })
        .ButtonContent()[SNew(STextBlock).Font(HudStyle::Font("Regular",16)).ColorAndOpacity(HudStyle::TextColor()).Text_Lambda([Get,Labels,Values]{int32 I=Values.IndexOfByKey(Get());return FText::FromString(Labels.IsValidIndex(I)?Labels[I]:FString::Printf(TEXT("Custom (%d)"),Get()));})];
    AddRow(Rows,Label,Help,Choice,Enabled);
}
void SMainMenu::AddPreference(TSharedRef<SVerticalBox> Rows,const TCHAR* Label,const TCHAR* Help,int32 GolfSettings::FPreferences::*Member,TArray<FString> Labels,TArray<int32> Values)
{AddChoice(Rows,Label,Help,Labels,Values,[this,Member]{return Draft.*Member;},[this,Member](int32 Value){Draft.*Member=Value;});}
void SMainMenu::Disabled(TSharedRef<SVerticalBox> Rows,const TCHAR* Label,const TCHAR* Reason)
{AddChoice(Rows,Label,Reason,{TEXT("Unavailable")},{0},[]{return 0;},[](int32){},false);}

void SMainMenu::BuildSettings(TSharedRef<SVerticalBox> Body)
{
    Body->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)[Button(TEXT("\u2190  Back"),[this]{Back();})]
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("Settings"),32)]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(300)[SNew(SSearchBox).HintText(FText::FromString(TEXT("Search..."))).InitialText(FText::FromString(Search))
            .OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type Type){if(Type==ETextCommit::OnEnter){Search=Value.ToString();Rebuild();}})]]];
    TSharedRef<SVerticalBox> Nav=SNew(SVerticalBox),Rows=SNew(SVerticalBox);
    for(int32 C=0;C<8;++C) Nav->AddSlot().AutoHeight().Padding(0,1)[Button(Categories[C],[this,C]{Category=C;Search.Empty();Rebuild();},C==Category,16)];
    for(int32 C=0;C<8;++C)
    {
        if(Search.IsEmpty() && Category!=C)continue;
        if(!Search.IsEmpty())Rows->AddSlot().AutoHeight().Padding(0,12)[Text(Categories[C],16,true)];
        using P=GolfSettings::FPreferences;
        switch(C)
        {
        case 0:
            AddPreference(Rows,TEXT("Handedness"),TEXT("Player and device preference; measured launch directions stay unchanged."),&P::LeftHanded,{TEXT("Right"),TEXT("Left")},{0,1});
            AddPreference(Rows,TEXT("Distance units"),TEXT("Carry, total and long distances."),&P::DistanceUnits,{TEXT("Yards"),TEXT("Meters")},{0,1});
            AddPreference(Rows,TEXT("Putting units"),TEXT("Short distances and height."),&P::PuttingUnits,{TEXT("Feet"),TEXT("Meters")},{0,1});
            AddPreference(Rows,TEXT("Speed units"),TEXT("Ball speed and wind."),&P::SpeedUnits,{TEXT("MPH"),TEXT("KM/H"),TEXT("M/S")},{0,1,2});
            Disabled(Rows,TEXT("Club suggestion"),TEXT("Automatic club recommendations are not implemented yet."));
            AddPreference(Rows,TEXT("Auto tee delay"),TEXT("After the next shot stops. Manual uses R to reset."),&P::AutoTee,{TEXT("3 seconds"),TEXT("5 seconds"),TEXT("8 seconds"),TEXT("Manual")},{3,5,8,0});break;
        case 2:
        {
            const DisplayOptions::FFacts F=Facts();
            const DisplayOptions::FModeChoice ModeChoice=DisplayOptions::Modes(BorderlessIsFullscreen);
            // A mode change re-selects a resolution the new mode offers and rebuilds the page from the next tick.
            AddChoice(Rows,TEXT("Display mode"),TEXT("Display changes require confirmation."),ModeChoice.Labels,ModeChoice.Values,[this]{return Video.Mode;},
                [this,F](int32 V){Video.Mode=V;Video.Resolution=DisplayOptions::Choose(DisplayOptions::Resolutions(V,F,Video.Resolution),Video.Resolution);bRebuildPending=true;});
            const bool bDesktopSized=DisplayOptions::DesktopSized(Video.Mode,F);
            TArray<FIntPoint> Resolutions=DisplayOptions::Resolutions(Video.Mode,F,Video.Resolution);
            if(Resolutions.IsEmpty())Resolutions.Add(Video.Resolution);
            TArray<FString> Labels;TArray<int32> Values;
            for(int32 I=0;I<Resolutions.Num();++I){Labels.Add(FString::Printf(TEXT("%d x %d"),Resolutions[I].X,Resolutions[I].Y));Values.Add(I);}
            const TCHAR* ResolutionHelp=Video.Mode==DisplayOptions::Windowed?TEXT("Window sizes that fit the desktop."):bDesktopSized?TEXT("Rendered at this size and scaled to the display."):TEXT("Modes reported by the display.");
            AddChoice(Rows,TEXT("Resolution"),ResolutionHelp,Labels,Values,[this,Resolutions]{return Resolutions.IndexOfByKey(Video.Resolution);},[this,Resolutions](int32 V){Video.Resolution=Resolutions[V];},Resolutions.Num()>1);
            AddChoice(Rows,TEXT("Frame rate limit"),TEXT("Limit rendering work and power use."),{TEXT("60 FPS"),TEXT("120 FPS"),TEXT("144 FPS"),TEXT("Unlimited")},{60,120,144,0},[this]{return Video.FPS;},[this](int32 V){Video.FPS=V;});
            AddChoice(Rows,TEXT("VSync"),TEXT("Synchronize with the display."),{TEXT("Off"),TEXT("On")},{0,1},[this]{return Video.VSync;},[this](int32 V){Video.VSync=V;});
            if(bDesktopSized)
                AddChoice(Rows,TEXT("Render scale"),TEXT("Set by Resolution in this mode."),{FString::Printf(TEXT("%d%%"),Video.Scale)},{Video.Scale},[this]{return Video.Scale;},[](int32){},false);
            else
                AddChoice(Rows,TEXT("Render scale"),TEXT("Percentage of output resolution used for rendering."),{TEXT("75%"),TEXT("85%"),TEXT("100%")},{75,85,100},[this]{return Video.Scale;},[this](int32 V){Video.Scale=V;});
            const TArray<FString> Quality={TEXT("Low"),TEXT("Medium"),TEXT("High"),TEXT("Epic")}; const TArray<int32> Levels={0,1,2,3};
            AddChoice(Rows,TEXT("Quality preset"),TEXT("Individual changes produce a Custom preset."),Quality,Levels,[this]{for(int32 I=1;I<10;++I)if(Video.Quality[I]!=Video.Quality[0])return -1;return Video.Quality[0];},[this](int32 V){for(int32& Q:Video.Quality)Q=V;});
            const TCHAR* Names[]={TEXT("View distance"),TEXT("Anti-aliasing"),TEXT("Shadows"),TEXT("Global illumination"),TEXT("Reflections"),TEXT("Post-processing"),TEXT("Textures"),TEXT("Effects"),TEXT("Foliage"),TEXT("Shading")};
            // A missing console variable means the feature can't be confirmed, so its quality is left enabled.
            auto FeatureOn=[](const TCHAR* Name){const IConsoleVariable* Var=IConsoleManager::Get().FindConsoleVariable(Name);return !Var||Var->GetInt()!=0;};
            for(int32 I=0;I<10;++I)
            {
                const bool Enabled=(I!=3 || FeatureOn(TEXT("r.DynamicGlobalIlluminationMethod"))) && (I!=4 || FeatureOn(TEXT("r.ReflectionMethod")));
                AddChoice(Rows,Names[I],Enabled?TEXT("Higher detail can increase GPU cost."):TEXT("This rendering feature is disabled in the project."),Quality,Levels,[this,I]{return Video.Quality[I];},[this,I](int32 V){Video.Quality[I]=V;},Enabled);
            }
            AddPreference(Rows,TEXT("Motion blur"),TEXT("Blur during camera movement."),&P::MotionBlur,{TEXT("Off"),TEXT("On")},{0,1});
            AddPreference(Rows,TEXT("Depth of field"),TEXT("Cinematic focus where the camera supports it."),&P::DepthOfField,{TEXT("Off"),TEXT("On")},{0,1});
            break;
        }
        case 3:
            for(const TCHAR* Label:{TEXT("Master volume"),TEXT("Course ambience"),TEXT("Ball & club"),TEXT("Menu sounds"),TEXT("Dynamic range"),TEXT("Mute in background")}) Disabled(Rows,Label,TEXT("Game audio controls are not implemented yet."));break;
        case 4:
            AddPreference(Rows,TEXT("Shot camera"),TEXT("Watch from the address position or follow the ball."),&P::FixedCamera,{TEXT("Follow ball"),TEXT("Fixed address")},{0,1});
            Disabled(Rows,TEXT("Landing camera"),TEXT("Landing camera is not implemented yet."));
            AddPreference(Rows,TEXT("Cup zoom"),TEXT("Close in when the ball approaches the cup."),&P::CupZoom,{TEXT("Off"),TEXT("On")},{0,1});
            AddPreference(Rows,TEXT("Ball tracer"),TEXT("Includes current and previous shot ribbons."),&P::Tracer,{TEXT("Off"),TEXT("Subtle"),TEXT("Full")},{0,1,2});
            Disabled(Rows,TEXT("Green contours"),TEXT("The playable slope overlay is not implemented yet."));
            AddPreference(Rows,TEXT("Shot data"),TEXT("Choose the amount of shot feedback."),&P::ShotData,{TEXT("Minimal"),TEXT("Essential"),TEXT("Detailed")},{0,1,2});
            AddPreference(Rows,TEXT("HUD scale"),TEXT("Increase panel size on distant screens."),&P::HudPercent,{TEXT("100%"),TEXT("125%"),TEXT("150%")},{100,125,150});break;
        case 1:
            BuildSimulator(Rows);break;
        case 5:
            AddPreference(Rows,TEXT("Text size"),TEXT("Increase labels and values throughout the interface."),&P::TextPercent,{TEXT("Standard"),TEXT("Large"),TEXT("Extra large")},{100,125,150});
            AddPreference(Rows,TEXT("High contrast"),TEXT("Opaque panel backgrounds for stronger separation."),&P::HighContrast,{TEXT("Off"),TEXT("On")},{0,1});
            AddPreference(Rows,TEXT("Reduced motion"),TEXT("Use a fixed shot camera and instant return to address."),&P::ReducedMotion,{TEXT("Off"),TEXT("On")},{0,1});
            Disabled(Rows,TEXT("Color palette"),TEXT("Color-vision palettes are not implemented yet."));Disabled(Rows,TEXT("Subtitles"),TEXT("Spoken content and subtitles are not implemented yet."));break;
        case 6:
            AddPreference(Rows,TEXT("Gimmie radius"),TEXT("New scored holes: on the green, inside the ring adds one conceded putt."),&P::GimmieFeet,{TEXT("Off"),TEXT("3 ft"),TEXT("5 ft"),TEXT("8 ft"),TEXT("10 ft")},{0,3,5,8,10});
            AddPreference(Rows,TEXT("Stimp"),TEXT("New sessions and holes. Higher stimp means faster greens."),&P::Stimp,{TEXT("8 ft"),TEXT("9 ft"),TEXT("10 ft"),TEXT("11 ft"),TEXT("12 ft"),TEXT("13 ft"),TEXT("14 ft"),TEXT("15 ft")},{8,9,10,11,12,13,14,15});
            AddPreference(Rows,TEXT("Wind"),TEXT("Starting speed for a new session or hole; direction comes from the course."),&P::WindMph,{TEXT("Course default"),TEXT("Calm"),TEXT("Light (5 mph)"),TEXT("Moderate (10 mph)"),TEXT("Strong (20 mph)")},{-1,0,5,10,20});
            for(const TCHAR* Label:{TEXT("Mulligans"),TEXT("Fairway firmness"),TEXT("Time of day")})Disabled(Rows,Label,TEXT("This gameplay setting is not implemented yet."));break;
        case 7:
            Rows->AddSlot().AutoHeight().Padding(12)[Text(TEXT("Shot recording\nDuring Training, press F9 to start or stop recording. Check the in-game recording indicator for status. Settings never changes recording state."),18)];
            Disabled(Rows,TEXT("Diagnostic logging"),TEXT("User-controlled logging is not implemented yet."));
            Disabled(Rows,TEXT("Recording retention"),TEXT("Automatic retention is not implemented. Recordings are kept."));break;
        }
    }
    Body->AddSlot().FillHeight(1)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,24,0)[SNew(SBox).WidthOverride(230)[SNew(SScrollBox)+SScrollBox::Slot()[Nav]]]
        +SHorizontalBox::Slot().FillWidth(1)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]]];
    Body->AddSlot().AutoHeight().Padding(0,18,0,0)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).ColorAndOpacity(HudStyle::LabelColor()).Text_Lambda([this]{return FText::FromString(!SaveError.IsEmpty()?SaveError:Dirty()?TEXT("Unsaved changes"):TEXT("All changes saved"));})]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Reset All"),[this]{
            // The launch-monitor connection is changed live, never by Reset All.
            Draft={};Draft.Provider=Saved.Provider;Draft.OpenConnectPort=Saved.OpenConnectPort;Draft.OpenConnectLan=Saved.OpenConnectLan;
            Video.VSync=0;Video.FPS=60;Video.Scale=100;for(int32& Q:Video.Quality)Q=2;
            Video.Mode=DisplayOptions::ShownMode(DisplayOptions::Borderless,BorderlessIsFullscreen);Video.Resolution=EngineSettings()->GetDesktopResolution();Rebuild();})]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Discard"),[this]{Draft=Saved;Video=SavedVideo;Rebuild();})]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Apply"),[this]{Apply();},true)]];
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
    if(Controller.IsValid())if(auto* Hud=Cast<AUOpenGolfHUD>(Controller->GetHUD()))Hud->RefreshTheme();
    Rebuild();
}
void SMainMenu::CommitConnection(TFunction<void(GolfSettings::FPreferences&)> Change)
{
    // Saves and applies only the connection change; other unapplied edits stay pending in Draft.
    GolfSettings::FPreferences Next=GolfSettings::Get();
    Change(Next);
    if(!GolfSettings::Commit(Next,Controller.IsValid()?Controller->GetWorld():nullptr))
    {
        SaveError=TEXT("Could not save the launch monitor choice. Check storage and retry.");
    }
    else
    {
        SaveError.Empty();
        Change(Draft);
        Change(Saved);
    }
    bRebuildPending=true;
}
FString SMainMenu::LiveStatus() const
{
    const UOpenLaunchMonitorSubsystem* M=Monitor();
    if(!M)return TEXT("Launch monitor unavailable.");
    const ELaunchMonitorConnectionState State=M->GetConnectionState();
    FString Status;
    if(M->GetProvider()==ELaunchMonitorProvider::OpenConnect)
    {
        switch(State)
        {
        case ELaunchMonitorConnectionState::Disconnected: Status=TEXT("Not listening."); break;
        case ELaunchMonitorConnectionState::Ready: Status=FString::Printf(TEXT("Ready: %s, on %s."),*M->GetDeviceName(),*M->GetOpenConnectAddress()); break;
        case ELaunchMonitorConnectionState::Connected: Status=FString::Printf(TEXT("Connector attached: %s, on %s."),*M->GetDeviceName(),*M->GetOpenConnectAddress()); break;
        default: Status=FString::Printf(TEXT("Listening on %s; waiting for a connector."),*M->GetOpenConnectAddress()); break;
        }
        FDateTime Last;
        if(State!=ELaunchMonitorConnectionState::Disconnected && M->GetOpenConnectLastMessageUtc(Last))
            Status+=FString::Printf(TEXT("  Last message %d s ago."),FMath::Max(0,int32((FDateTime::UtcNow()-Last).GetTotalSeconds())));
    }
    else
    {
        Status=StaticEnum<ELaunchMonitorConnectionState>()->GetDisplayNameTextByValue(int64(State)).ToString();
        if(!M->GetDeviceName().IsEmpty())Status+=TEXT(": ")+M->GetDeviceName();
        Status+=TEXT(".");
    }
    if(!M->GetLastError().IsEmpty())Status+=TEXT("\n")+M->GetLastError();
    return Status;
}
void SMainMenu::BuildSimulator(TSharedRef<SVerticalBox> Rows)
{
    using P=GolfSettings::FPreferences;
    const bool bOpenConnect=Draft.Provider==int32(ELaunchMonitorProvider::OpenConnect);
    auto ProviderName=[](int32 Provider){return StaticEnum<ELaunchMonitorProvider>()->GetDisplayNameTextByValue(Provider).ToString();};

    // Connection choices act at once, like Connect and Disconnect: no Apply. The block below always shows the live connection.
    AddChoice(Rows,TEXT("Launch monitor"),TEXT("Switches immediately. Square Home connects over Bluetooth; Open Connect accepts shots from a connector app over TCP."),
        {TEXT("Square Home"),TEXT("Open Connect")},{0,1},[this]{return Draft.Provider;},[this](int32 V){CommitConnection([V](P& Prefs){Prefs.Provider=V;});});

    if(Matches(TEXT("Launch monitor connection status connect disconnect listening connector"),FString()))
    {
        const UOpenLaunchMonitorSubsystem* Live=Monitor();
        const bool bLiveOpenConnect=Live && Live->GetProvider()==ELaunchMonitorProvider::OpenConnect;
        Rows->AddSlot().AutoHeight().Padding(0,8,0,4)[Text(TEXT("Launch monitor connection"),24)];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Font(HudStyle::Font("Regular",14)).AutoWrapText(true)
            .ColorAndOpacity_Lambda([this]{const UOpenLaunchMonitorSubsystem* M=Monitor();return FSlateColor(M?HudStyle::StateColor(M->GetConnectionState()):HudStyle::LabelColor());})
            .Text_Lambda([this,ProviderName]{const UOpenLaunchMonitorSubsystem* M=Monitor();
                return FText::FromString((M?ProviderName(int32(M->GetProvider()))+TEXT("  ·  "):FString())+LiveStatus());})];
        Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth()[Button(bLiveOpenConnect?TEXT("Start listening"):TEXT("Connect"),[this]{if(Controller.IsValid())Controller->ConnectLaunchMonitor();bRebuildPending=true;})]
            +SHorizontalBox::Slot().AutoWidth()[Button(bLiveOpenConnect?TEXT("Stop listening"):TEXT("Disconnect"),[this]{if(Controller.IsValid())Controller->DisconnectLaunchMonitor();bRebuildPending=true;})]];
    }

    if(bOpenConnect)
    {
        const FString PortHelp=TEXT("TCP port the connector sends to (default 4921); changes restart the listener. Set the same port in the connector.");
        if(Matches(TEXT("Connector port"),PortHelp))
            AddRow(Rows,TEXT("Connector port"),PortHelp,SNew(SEditableTextBox).Font(HudStyle::Font("Regular",16))
                .Text_Lambda([this]{return FText::AsNumber(Draft.OpenConnectPort,&FNumberFormattingOptions::DefaultNoGrouping());})
                .OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type)
                {
                    // Anything but a whole number from 1 to 65535 leaves the port unchanged.
                    int32 Port=0;
                    if(Value.ToString().IsNumeric() && LexTryParseString(Port,*Value.ToString()) && Port>=1 && Port<=65535 && Port!=Draft.OpenConnectPort)
                        CommitConnection([Port](P& Prefs){Prefs.OpenConnectPort=Port;});
                    else
                        bRebuildPending=true;
                }));
        AddChoice(Rows,TEXT("Allow connectors on other PCs"),TEXT("On listens on every network interface for a connector on another computer on your local network; Windows may ask to allow it through the firewall. Connectors are not authenticated, so use this only on a trusted home network. Connections from outside private network ranges are refused."),
            {TEXT("Off (this PC only)"),TEXT("On (local network)")},{0,1},[this]{return Draft.OpenConnectLan;},[this](int32 V){CommitConnection([V](P& Prefs){Prefs.OpenConnectLan=V;});});
        const FString ConnectorHelp=FString::Printf(TEXT("Any connector that sends Open Connect v1 shots works, for example the Rapsodo MLM2PRO and FlightScope Mevo+ connector, SkyTrak (OpenSkyPlus2), the Garmin R10 connector and PiTrac.\nPoint the connector at %s:%d."),
            Draft.OpenConnectLan?TEXT("this PC's network address"):TEXT("127.0.0.1"),Draft.OpenConnectPort);
        if(Matches(TEXT("Compatible connectors"),ConnectorHelp))
            Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(1,1,1,.025f)).Padding(12)
                [SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(TEXT("Compatible connectors"),18)]+SVerticalBox::Slot().AutoHeight()[Text(ConnectorHelp,12,true)]]];
        Disabled(Rows,TEXT("Auto reconnect"),TEXT("Square Home only. The Open Connect listener keeps waiting when a connector disconnects."));
        Disabled(Rows,TEXT("Square spin mode"),TEXT("Square Home only. Connectors send the spin their launch monitor reports."));
    }
    else
    {
        AddPreference(Rows,TEXT("Auto reconnect"),TEXT("Reconnect when an established connection drops."),&P::Reconnect,{TEXT("Off"),TEXT("On")},{0,1});
        AddPreference(Rows,TEXT("Square spin mode"),TEXT("Choose measured or device-calculated spin."),&P::CalculatedSpin,{TEXT("Measured"),TEXT("Calculated")},{0,1});
    }
    Disabled(Rows,TEXT("Readiness sound"),TEXT("Visual readiness is available in-game; sound is not implemented yet."));
}
