// SPDX-License-Identifier: MIT
// Settings page of SMainMenu: categories, rows and the Simulator connection block.
#include "UI/Menu/MainMenu.h"
#include "UI/Menu/MainMenuInternal.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/MenuStyle.h"
#include "UI/Common/PageHeader.h"
#include "OpenGolfOnlineSubsystem.h"
#include "SOpenGolfAccountPanel.h"
#include "UOpenGolfPlayerController.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/IConsoleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"

using namespace MainMenuDetail;
namespace
{
    const TCHAR* ConnectionSaveError=TEXT("Could not save the launch monitor choice. Check storage and retry.");
}
bool SMainMenu::Matches(const FString& Label,const FString& Help) const
{
    return Search.IsEmpty() || (Label+Help).Contains(Search,ESearchCase::IgnoreCase);
}
TSharedRef<SWidget> SMainMenu::AddRow(TSharedRef<SVerticalBox> Rows,const FString& Label,const FString& Help,TSharedRef<SWidget> Control,bool Enabled)
{
    TSharedRef<SWidget> Row=SNew(SBorder).BorderImage(&Style.Section).Padding(MenuStyle::RowPadding)
        [SNew(SHorizontalBox)+SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SVerticalBox)+SVerticalBox::Slot().AutoHeight()[Text(Label,MenuStyle::BodySize,!Enabled)]+SVerticalBox::Slot().AutoHeight()[Text(Help,MenuStyle::HelpSize,true)]]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(20,0)[SNew(SBox).WidthOverride(MenuStyle::ControlWidth)[Control]]];
    Rows->AddSlot().AutoHeight().Padding(0,0,0,MenuStyle::RowGap)[Row];
    return Row;
}
void SMainMenu::AddChoice(TSharedRef<SVerticalBox> Rows,const FString& Label,const FString& Help,TArray<FString> Labels,TArray<int32> Values,TFunction<int32()> Get,TFunction<void(int32)> Set,bool Enabled)
{
    if(!Matches(Label,Help)) return;
    auto Choice=SNew(SComboButton).IsEnabled(Enabled).ButtonStyle(&Style.Button)
        .OnGetMenuContent_Lambda([this,Labels,Values,Set]{
            TSharedRef<SVerticalBox> Options=SNew(SVerticalBox);
            for(int32 I=0;I<Labels.Num();++I) Options->AddSlot().AutoHeight()[NavButton(Labels[I],[Set,Values,I]{Set(Values[I]);FSlateApplication::Get().DismissAllMenus();},false)];
            return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HudStyle::LaunchTileColor())[SNew(SScrollBox)+SScrollBox::Slot()[Options]];
        })
        .ButtonContent()[SNew(STextBlock).Font(HudStyle::Font("Regular",MenuStyle::NavSize)).ColorAndOpacity(Style.TextColor).Text_Lambda([Get,Labels,Values]{int32 I=Values.IndexOfByKey(Get());return FText::FromString(Labels.IsValidIndex(I)?Labels[I]:FString::Printf(TEXT("Custom (%d)"),Get()));})];
    AddRow(Rows,Label,Help,Choice,Enabled);
}
void SMainMenu::AddPreference(TSharedRef<SVerticalBox> Rows,const TCHAR* Label,const TCHAR* Help,int32 GolfSettings::FPreferences::*Member,TArray<FString> Labels,TArray<int32> Values)
{AddChoice(Rows,Label,Help,Labels,Values,[this,Member]{return Draft.*Member;},[this,Member](int32 Value){Draft.*Member=Value;});}
void SMainMenu::Disabled(TSharedRef<SVerticalBox> Rows,const TCHAR* Label,const TCHAR* Reason)
{AddChoice(Rows,Label,Reason,{TEXT("Unavailable")},{0},[]{return 0;},[](int32){},false);}

void SMainMenu::BuildSettings(TSharedRef<SVerticalBox> Body)
{
    SearchStyle=MenuStyle::SearchBox();
    const TSharedRef<SWidget> SearchBox=SNew(SBox).WidthOverride(MenuStyle::SearchWidth)[SNew(SSearchBox).Style(&SearchStyle).HintText(FText::FromString(TEXT("Search..."))).InitialText(FText::FromString(Search))
        .OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type Type){if(Type==ETextCommit::OnEnter){Search=Value.ToString();Rebuild();}})];
    Body->AddSlot().AutoHeight().Padding(0,0,0,16)[PageHeader::Make(Style,FText::FromString(TEXT("Settings")),FSimpleDelegate::CreateSP(this,&SMainMenu::Back),SearchBox)];
    TSharedRef<SVerticalBox> Nav=SNew(SVerticalBox),Rows=SNew(SVerticalBox);
    for(int32 C=0;C<CategoryCount;++C) Nav->AddSlot().AutoHeight().Padding(0,0,0,4)[NavButton(Categories[C],[this,C]{Category=C;Search.Empty();Rebuild();},C==Category)];
    for(int32 C=0;C<CategoryCount;++C)
    {
        if(Search.IsEmpty() && Category!=C)continue;
        if(!Search.IsEmpty())Rows->AddSlot().AutoHeight().Padding(0,12)[Text(Categories[C],MenuStyle::NavSize,true)];
        using P=GolfSettings::FPreferences;
        switch(C)
        {
        case 0:
            BuildPlayer(Rows);break;
        case 2:
        {
            const DisplayOptions::FFacts F=Facts();
            const DisplayOptions::FModeChoice ModeChoice=DisplayOptions::Modes(BorderlessIsFullscreen);
            // A mode change re-selects a resolution the new mode offers and rebuilds the page from the next tick.
            AddChoice(Rows,TEXT("Display mode"),TEXT("Display changes require confirmation."),ModeChoice.Labels,ModeChoice.Values,[this]{return Video.Mode;},
                [this,F](int32 V){Video.Mode=V;Video.Resolution=DisplayOptions::Choose(DisplayOptions::Resolutions(V,F,Video.Resolution),Video.Resolution);RefreshSettings();});
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
            Rows->AddSlot().AutoHeight().Padding(12)[Text(TEXT("Shot recording\nOn the range (Training), press F9 or click the record icon in the bottom-right status box to start or stop recording. While you are signed in, recorded shots are also saved to your online profile. Shots on the hole are never recorded. Settings never changes recording state."),18)];
            Disabled(Rows,TEXT("Diagnostic logging"),TEXT("User-controlled logging is not implemented yet."));
            Disabled(Rows,TEXT("Recording retention"),TEXT("Automatic retention is not implemented. Recordings are kept."));break;
        }
    }
    Body->AddSlot().FillHeight(1)[SNew(SHorizontalBox)+SHorizontalBox::Slot().AutoWidth().Padding(0,0,24,0)[SNew(SBox).WidthOverride(MenuStyle::NavWidth)[SNew(SScrollBox)+SScrollBox::Slot()[Nav]]]
        +SHorizontalBox::Slot().FillWidth(1)[SNew(SScrollBox)+SScrollBox::Slot()[Rows]]];
    Body->AddSlot().AutoHeight().Padding(0,18,0,0)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Font(HudStyle::Font("Regular",MenuStyle::SmallSize)).ColorAndOpacity(Style.LabelColor).Text_Lambda([this]{return FText::FromString(!SaveError.IsEmpty()?SaveError:Dirty()?TEXT("Unsaved changes"):TEXT("All changes saved"));})]
        +SHorizontalBox::Slot().AutoWidth().Padding(MenuStyle::ButtonGap,0,0,0)[Button(TEXT("Reset All"),[this]{
            // The launch-monitor connection is changed live, never by Reset All.
            // Nor is the time zone, which also applies at once.
            Draft={};Draft.Provider=Saved.Provider;Draft.OpenConnectPort=Saved.OpenConnectPort;Draft.OpenConnectLan=Saved.OpenConnectLan;Draft.TimeZone=Saved.TimeZone;
            Video.VSync=0;Video.FPS=60;Video.Scale=100;for(int32& Q:Video.Quality)Q=2;
            Video.Mode=DisplayOptions::ShownMode(DisplayOptions::Borderless,BorderlessIsFullscreen);Video.Resolution=EngineSettings()->GetDesktopResolution();RefreshSettings();})]
        +SHorizontalBox::Slot().AutoWidth().Padding(MenuStyle::ButtonGap,0,0,0)[Button(TEXT("Discard"),[this]{Draft=Saved;Video=SavedVideo;RefreshSettings();})]
        +SHorizontalBox::Slot().AutoWidth().Padding(MenuStyle::ButtonGap,0,0,0)[Button(TEXT("Apply"),[this]{Apply();},true)]];
}
void SMainMenu::BuildPlayer(TSharedRef<SVerticalBox> Rows)
{
    using P=GolfSettings::FPreferences;
    // The account acts at once, like the connection choices: sign in, or the profile's display name. Signed out, the
    // profile and Shot history are not shown. Neither needs a page rebuild when the session changes: the account panel
    // rebuilds itself and the Shot history row's visibility follows HasProfile.
    UOpenGolfOnlineSubsystem* Online=OnlineService();
    if(Online&&Matches(TEXT("Account"),TEXT("Sign in, create account, profile, display name")))
        Rows->AddSlot().AutoHeight()[SNew(SOpenGolfAccountPanel).Online(Online).Style(Style).ShowHeading(false)
            .Page(EOpenGolfAccountPage::Account).TimeZone(GolfSettings::ChosenTimeZone().Id)];
    const FString ShotsHelp=TEXT("Club averages and saved shots from the last 90 days.");
    if(Matches(TEXT("Shot history"),ShotsHelp))
    {
        // A fresh page each time: a cached one would reopen on the last club's shots with old data.
        AddRow(Rows,TEXT("Shot history"),ShotsHelp,Button(TEXT("View"),[this]{Pages.Remove(TEXT("shots"));Route=ERoute::Shots;bRebuildPending=true;}))
            ->SetVisibility(TAttribute<EVisibility>::CreateLambda([this]{return HasProfile()?EVisibility::Visible:EVisibility::Collapsed;}));
    }
    TArray<FString> ZoneLabels;TArray<int32> ZoneValues;
    const TConstArrayView<GolfSettings::FTimeZoneChoice> Zones=GolfSettings::TimeZones();
    for(int32 I=0;I<Zones.Num();++I){ZoneLabels.Add(Zones[I].Label);ZoneValues.Add(I);}
    AddChoice(Rows,TEXT("Time zone"),TEXT("Times in your shot history. Changes at once; Local uses this computer's time zone."),ZoneLabels,ZoneValues,
        [this]{return Draft.TimeZone;},[this](int32 V){CommitNow([V](P& Prefs){Prefs.TimeZone=V;},TEXT("Could not save the time zone. Check storage and retry."));});
    AddPreference(Rows,TEXT("Handedness"),TEXT("Player and device preference; measured launch directions stay unchanged."),&P::LeftHanded,{TEXT("Right"),TEXT("Left")},{0,1});
    AddPreference(Rows,TEXT("Distance units"),TEXT("Carry, total and long distances."),&P::DistanceUnits,{TEXT("Yards"),TEXT("Meters")},{0,1});
    AddPreference(Rows,TEXT("Putting units"),TEXT("Short distances and height."),&P::PuttingUnits,{TEXT("Feet"),TEXT("Meters")},{0,1});
    AddPreference(Rows,TEXT("Speed units"),TEXT("Ball speed and wind."),&P::SpeedUnits,{TEXT("MPH"),TEXT("KM/H"),TEXT("M/S")},{0,1,2});
    Disabled(Rows,TEXT("Club suggestion"),TEXT("Automatic club recommendations are not implemented yet."));
    AddPreference(Rows,TEXT("Auto tee delay"),TEXT("After the next shot stops. Manual uses R to reset."),&P::AutoTee,{TEXT("3 seconds"),TEXT("5 seconds"),TEXT("8 seconds"),TEXT("Manual")},{3,5,8,0});
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
        {TEXT("Square Home"),TEXT("Open Connect")},{0,1},[this]{return Draft.Provider;},[this](int32 V){CommitNow([V](P& Prefs){Prefs.Provider=V;},ConnectionSaveError);});

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
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,MenuStyle::ButtonGap,0)[Button(bLiveOpenConnect?TEXT("Start listening"):TEXT("Connect"),[this]{if(Controller.IsValid())Controller->ConnectLaunchMonitor();bRebuildPending=true;})]
            +SHorizontalBox::Slot().AutoWidth()[Button(bLiveOpenConnect?TEXT("Stop listening"):TEXT("Disconnect"),[this]{if(Controller.IsValid())Controller->DisconnectLaunchMonitor();bRebuildPending=true;})]];
    }

    if(bOpenConnect)
    {
        const FString PortHelp=TEXT("TCP port the connector sends to (default 4921); changes restart the listener. Set the same port in the connector.");
        if(Matches(TEXT("Connector port"),PortHelp))
            AddRow(Rows,TEXT("Connector port"),PortHelp,SNew(SEditableTextBox).Style(&Style.TextField.GetValue()).Font(HudStyle::Font("Regular",MenuStyle::NavSize))
                .Text_Lambda([this]{return FText::AsNumber(Draft.OpenConnectPort,&FNumberFormattingOptions::DefaultNoGrouping());})
                .OnTextCommitted_Lambda([this](const FText& Value,ETextCommit::Type)
                {
                    // Anything but a whole number from 1 to 65535 leaves the port unchanged.
                    int32 Port=0;
                    if(Value.ToString().IsNumeric() && LexTryParseString(Port,*Value.ToString()) && Port>=1 && Port<=65535 && Port!=Draft.OpenConnectPort)
                        CommitNow([Port](P& Prefs){Prefs.OpenConnectPort=Port;},ConnectionSaveError);
                    else
                        bRebuildPending=true;
                }));
        AddChoice(Rows,TEXT("Allow connectors on other PCs"),TEXT("On listens on every network interface for a connector on another computer on your local network; Windows may ask to allow it through the firewall. Connectors are not authenticated, so use this only on a trusted home network. Connections from outside private network ranges are refused."),
            {TEXT("Off (this PC only)"),TEXT("On (local network)")},{0,1},[this]{return Draft.OpenConnectLan;},[this](int32 V){CommitNow([V](P& Prefs){Prefs.OpenConnectLan=V;},ConnectionSaveError);});
        const FString ConnectorHelp=FString::Printf(TEXT("Any connector that sends Open Connect v1 shots works, for example the Rapsodo MLM2PRO and FlightScope Mevo+ connector, SkyTrak (OpenSkyPlus2), the Garmin R10 connector and PiTrac.\nPoint the connector at %s:%d."),
            Draft.OpenConnectLan?TEXT("this PC's network address"):TEXT("127.0.0.1"),Draft.OpenConnectPort);
        if(Matches(TEXT("Compatible connectors"),ConnectorHelp))
            Rows->AddSlot().AutoHeight().Padding(0,0,0,MenuStyle::RowGap)[SNew(SBorder).BorderImage(&Style.Section).Padding(MenuStyle::RowPadding)
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
