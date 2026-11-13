// SPDX-License-Identifier: MIT
#include "SOpenGolfAccountPanel.h"
#include "SOpenGolfAccountButton.h"
#include "SOpenGolfIconButton.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OnlineFormat.h"
#include "OnlineJson.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

using namespace OpenGolfOnline;
namespace OpenGolfAccountPanelDetail
{
    constexpr float NameFieldWidth = 320, SectionPadding = 12, SectionGap = 8, ButtonGap = 8;
    /** The Account page keeps this height signed out, loading and signed in, so the host's rows below do not jump. */
    constexpr float AccountMinHeight = 170;
    constexpr float ClubColumn = 80, AverageColumn = 200, ShotsColumn = 120;
    constexpr float DeleteColumn = 110, DeleteIconSize = 32, TimeColumn = 230, ValueColumn = 125;
    const TCHAR* BackTag = TEXT("PageBack");
    const TCHAR* HistoryFields[] = {TEXT("shot_at"),TEXT("carry_yd"),TEXT("roll_yd"),TEXT("ball_speed_mph"),TEXT("launch_angle_deg"),TEXT("launch_direction_deg"),TEXT("total_spin_rpm"),TEXT("spin_axis_deg"),TEXT("back_spin_rpm"),TEXT("side_spin_rpm"),TEXT("face_angle_deg"),TEXT("club_path_deg"),TEXT("face_to_path_deg"),TEXT("attack_angle_deg"),TEXT("dynamic_loft_deg")};
    const TCHAR* HistoryCaptions[] = {TEXT("Time"),TEXT("Carry"),TEXT("Roll"),TEXT("Ball speed"),TEXT("Launch angle"),TEXT("Launch direction"),TEXT("Total spin"),TEXT("Spin axis"),TEXT("Backspin"),TEXT("Sidespin"),TEXT("Face angle"),TEXT("Club path"),TEXT("Face to path"),TEXT("Attack angle"),TEXT("Dynamic loft")};
    static_assert(UE_ARRAY_COUNT(HistoryFields) == UE_ARRAY_COUNT(HistoryCaptions), "Every history field has a caption.");

    /** One history cell: the shot time in TimeZone, or a number to one decimal. Face to path is derived. */
    FString HistoryValue(const TSharedPtr<FJsonObject>& Row,int32 Field,const FString& TimeZone)
    {
        if(Field==0)return ShotTime(OpenGolfOnline::Text(Row,HistoryFields[0]),TimeZone);
        double Value;
        if(FString(HistoryFields[Field])==TEXT("face_to_path_deg"))
        {
            double Face,Path;
            return Row->TryGetNumberField(TEXT("face_angle_deg"),Face)&&Row->TryGetNumberField(TEXT("club_path_deg"),Path)?FString::Printf(TEXT("%.1f"),Face-Path):FString();
        }
        return Row->TryGetNumberField(HistoryFields[Field],Value)?FString::Printf(TEXT("%.1f"),Value):FString();
    }
} // namespace OpenGolfAccountPanelDetail
using namespace OpenGolfAccountPanelDetail;

void FOpenGolfDisplayNameDraft::Synchronize(const UOpenGolfOnlineSubsystem* Online)
{
    const FString Key = Online && Online->HasSession() ? Online->GetAccountKey() : FString();
    if (AccountKey != Key)
    {
        AccountKey = Key; Name.Empty(); Error.Empty(); bEditing = bSaving = false;
    }
    if (!bEditing && !bSaving)
        Name = Online && !Key.IsEmpty() ? OpenGolfOnline::Text(Online->GetProfile(), TEXT("display_name")) : FString();
}
bool FOpenGolfDisplayNameDraft::IsDirty(const UOpenGolfOnlineSubsystem* Online) const
{
    return Online && Online->HasSession() && Online->GetProfile().IsValid() && AccountKey == Online->GetAccountKey()
        && bEditing && !Name.TrimStartAndEnd().Equals(OpenGolfOnline::Text(Online->GetProfile(), TEXT("display_name")), ESearchCase::CaseSensitive);
}
FString FOpenGolfDisplayNameDraft::ValidationError(const UOpenGolfOnlineSubsystem* Online) const
{
    if (!IsDirty(Online)) return {};
    if (Name.TrimStartAndEnd().IsEmpty()) return TEXT("Enter a display name.");
    if (Name.Len() > 40) return TEXT("Display name must be 40 characters or fewer.");
    for (TCHAR C : Name) if (C < 32 || C == 127) return TEXT("Display name cannot contain control characters.");
    return {};
}
void FOpenGolfDisplayNameDraft::Discard(const UOpenGolfOnlineSubsystem* Online)
{
    if (bSaving) return;
    bEditing = false; Error.Empty(); Synchronize(Online);
}
void FOpenGolfDisplayNameDraft::Apply(UOpenGolfOnlineSubsystem* Online)
{
    Synchronize(Online);
    if (bSaving) return;
    Error = ValidationError(Online);
    if (!Error.IsEmpty()) return;
    if (!IsDirty(Online)) { Discard(Online); return; }
    if (Online->IsBusy()) { Error = TEXT("Account is busy. Try Apply again when it finishes."); return; }
    bSaving = true;
    const FString Key = AccountKey;
    const TWeakPtr<FOpenGolfDisplayNameDraft> WeakDraft = AsShared();
    const TWeakObjectPtr<UOpenGolfOnlineSubsystem> Service = Online;
    Online->UpdateDisplayName(Name.TrimStartAndEnd(), [WeakDraft, Service, Key](bool bSaved)
    {
        const auto Draft = WeakDraft.Pin();
        if (!Draft) return;
        Draft->Synchronize(Service.Get());
        if (Draft->AccountKey != Key) return;
        Draft->bSaving = false;
        if (bSaved) Draft->Discard(Service.Get());
        else Draft->Error = TEXT("Could not save the display name. Try Apply again.");
    });
}

TSharedRef<SWidget> SOpenGolfAccountPanel::Text(const FString& Value,int32 Size,bool bMuted) const
{
    return SNew(STextBlock).Text(FText::FromString(Value)).Font(Style.GetFont(Size>0?Size:Style.BodySize))
        .ColorAndOpacity(bMuted?Style.LabelColor:Style.TextColor).AutoWrapText(true);
}
TSharedRef<SWidget> SOpenGolfAccountPanel::Button(const FString& Label,TFunction<void()> Action,bool bEnabled,bool bPrimary,int32 Size)
{
    return SNew(SButton).ButtonStyle(bPrimary?&Style.PrimaryButton:&Style.Button).IsEnabled_Lambda([this,bEnabled]{return bEnabled && Online.IsValid() && !Online->IsBusy();})
        .OnClicked_Lambda([Action]{Action();return FReply::Handled();})
        [SNew(STextBlock).Text(FText::FromString(Label)).Font(Style.GetFont(Size>0?Size:Style.BodySize))
            .ColorAndOpacity(bPrimary?Style.PrimaryLabelColor:Style.TextColor)];
}
TSharedRef<SVerticalBox> SOpenGolfAccountPanel::AddSection(const TSharedRef<SVerticalBox>& Page,const FString& Caption)
{
    auto Rows=SNew(SVerticalBox);
    if(!Caption.IsEmpty())Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[Text(Caption,Style.SmallSize,true)];
    Page->AddSlot().AutoHeight().Padding(0,0,0,SectionGap)[SNew(SBorder).BorderImage(&Style.Section).Padding(SectionPadding)[Rows]];
    return Rows;
}
void SOpenGolfAccountPanel::Construct(const FArguments& Args)
{
    Online=Args._Online;Style=Args._Style;bShowHeading=Args._ShowHeading;PageType=Args._Page;
    DisplayNameDraft=Args._DisplayNameDraft.IsValid()?Args._DisplayNameDraft:MakeShared<FOpenGolfDisplayNameDraft>();
    DisplayNameDraft->Synchronize(Online.Get());
    TimeZone=Args._TimeZone;TimeZoneName=Args._TimeZoneName.IsEmpty()?TEXT("local"):Args._TimeZoneName;
    bWasSignedIn=Online.IsValid()&&Online->HasSession();
    // The account this panel starts with is not a change: otherwise the first Rebuild would reset bLoadedCarry and
    // request club averages a second time.
    LastAccountKey=Online.IsValid()?Online->GetAccountKey():FString();
    LoadCarryIfReady();
    auto Root=SNew(SVerticalBox);
    if(bShowHeading && Online.IsValid())Root->AddSlot().AutoHeight().Padding(0,0,0,16)[BuildHeading(*Online)];
    Root->AddSlot().AutoHeight()[SNew(SBox).MinDesiredHeight(44)[SNew(STextBlock)
        .Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.LabelColor).AutoWrapText(true)
        .Text_Lambda([this]{return FText::FromString(Online.IsValid()?Online->GetStatus():TEXT("Online service unavailable."));})]];
    Root->AddSlot().AutoHeight()[SNew(STextBlock).Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.LabelColor).AutoWrapText(true)
        .Text_Lambda([this]{return FText::FromString(Online.IsValid()?Online->GetStorageWarning():FString());})
        .Visibility_Lambda([this]{return Online.IsValid()&&!Online->GetStorageWarning().IsEmpty()?EVisibility::Visible:EVisibility::Collapsed;})];
    if(PageType==EOpenGolfAccountPage::Account)Root->AddSlot().AutoHeight()[SAssignNew(PageContent,SBox).MinDesiredHeight(AccountMinHeight)];
    else Root->AddSlot().FillHeight(1)[SAssignNew(PageContent,SBox)];
    ChildSlot[SAssignNew(Content,SBox)[Root]]; Rebuild();
}
void SOpenGolfAccountPanel::LoadCarryIfReady()
{
    if(PageType!=EOpenGolfAccountPage::Shots||bLoadedCarry||!Online.IsValid()||!Online->HasSession()||!Online->GetProfile().IsValid()||Online->IsBusy())return;
    bLoadedCarry=true;Online->LoadCarry();
}
void SOpenGolfAccountPanel::Tick(const FGeometry& G,double T,float D)
{
    SCompoundWidget::Tick(G,T,D);
    DisplayNameDraft->Synchronize(Online.Get());
    if(Online.IsValid()&&Online->HasSession()!=bWasSignedIn)
    {
        bWasSignedIn=Online->HasSession();bHistory=false;bLoadedCarry=false;ConfirmDeleteId.Empty();
        bRebuild=true;
    }
    LoadCarryIfReady();
    if(!Online.IsValid())return;
    const bool Ready=Online->HasSession() && Online->GetProfile().IsValid();
    const auto State=Online->GetAccountState();
    const bool Loading=State==EOpenGolfAccountState::Restoring || (Online->HasSession()&&!Ready);
    const int32 Shape=Ready?3:Loading?2:Online->IsSigningIn()?1:0;
    const bool Changed=SeenShape!=Shape || LastAccountKey!=Online->GetAccountKey();
    if(bRebuild || Changed || (PageType==EOpenGolfAccountPage::Shots && SeenHistoryRevision!=Online->GetHistoryRevision()))
    {bRebuild=false;SeenShape=Shape;SeenHistoryRevision=Online->GetHistoryRevision();Rebuild();}
}
FText SOpenGolfAccountPanel::GetTitle() const
{
    const bool bSignedIn=Online.IsValid()&&Online->HasSession();
    if(PageType==EOpenGolfAccountPage::Shots)
        return FText::FromString(bSignedIn&&bHistory?ClubAbbreviation(Online->GetHistoryClub())+TEXT(" — shot history"):TEXT("Shot history"));
    if(Online.IsValid()&&!bSignedIn)return FText::FromString(TEXT("Sign in to ")+Online->GetApplicationName());
    return FText::FromString(TEXT("Account"));
}
bool SOpenGolfAccountPanel::HandleBack()
{
    if(!bHistory)return false;
    bHistory=false;ConfirmDeleteId.Empty();
    if(Online.IsValid())Online->LoadCarry();
    bRebuild=true;return true;
}
void SOpenGolfAccountPanel::Rebuild()
{
    if(!Online.IsValid())return;
    auto& Service=*Online.Get();
    const bool HadFocus=PageContent->HasFocusedDescendants();
    const float Offset=Scroll.IsValid()?Scroll->GetScrollOffset():0;
    DisplayNameDraft->Synchronize(&Service);
    if(LastAccountKey!=Service.GetAccountKey()){LastAccountKey=Service.GetAccountKey();bLoadedCarry=false;ConfirmDeleteId.Empty();}
    auto Page=SNew(SVerticalBox);
    const bool Ready=Service.HasSession() && Service.GetProfile().IsValid();
    if(PageType==EOpenGolfAccountPage::Account)
    {
        if(Ready)BuildProfile(Page,Service);
        else if(Service.GetAccountState()==EOpenGolfAccountState::Restoring || Service.HasSession())
            Page->AddSlot().AutoHeight().Padding(0,24)[Text(TEXT("Loading your account…"))];
        else BuildSignedOut(Page,Service);
        PageContent->SetContent(Page);
    }
    else
    {
        if(!Service.HasSession())Page->AddSlot().AutoHeight()[Text(TEXT("Sign in to see your saved shots."))];
        else if(bHistory)BuildHistory(Page,Service);
        else BuildClubs(Page,Service);
        if(bHistory)PageContent->SetContent(Page);
        else {PageContent->SetContent(SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[Page]);Scroll->SetScrollOffset(Offset);}
    }
    if(HadFocus)
    {
        TFunction<TSharedPtr<SWidget>(TSharedRef<SWidget>)> First=[&First](TSharedRef<SWidget> W)->TSharedPtr<SWidget>
        {
            if(!W->GetVisibility().IsVisible()||!W->IsEnabled())return nullptr;
            if(W->SupportsKeyboardFocus())return W;
            for(int32 I=0;I<W->GetChildren()->Num();++I)if(auto F=First(W->GetChildren()->GetChildAt(I)))return F;
            return nullptr;
        };
        if(auto Target=First(Page))FSlateApplication::Get().SetKeyboardFocus(Target,EFocusCause::Navigation);
    }
}
TSharedRef<SWidget> SOpenGolfAccountPanel::BuildHeading(UOpenGolfOnlineSubsystem& Service)
{
    // Built once in Construct, so the back icon, title and logout follow the page and session through attributes.
    // The logout button shows itself only while signed in.
    return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,16,0)
        [SNew(SOpenGolfIconButton).Style(Style).Icon(EOpenGolfAccountIcon::Back).Tag(BackTag).Label(FText::FromString(TEXT("Back to clubs")))
            .Visibility_Lambda([this]{return bHistory&&Online.IsValid()&&Online->HasSession()?EVisibility::Visible:EVisibility::Collapsed;})
            .OnClicked_Lambda([this]{HandleBack();})]
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [SNew(STextBlock).Font(Style.GetFont(Style.TitleSize)).ColorAndOpacity(Style.TextColor).Text_Lambda([this]{return GetTitle();})]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [SNew(SOpenGolfAccountButton).Online(&Service).SignOut(true).Style(Style)
            .OnClicked_Lambda([this]{if(Online.IsValid())Online->SignOut();DisplayNameDraft->Synchronize(Online.Get());bHistory=false;ConfirmDeleteId.Empty();bRebuild=true;})];
}
void SOpenGolfAccountPanel::BuildSignedOut(const TSharedRef<SVerticalBox>& Page,UOpenGolfOnlineSubsystem& Service)
{
    auto* S=&Service;
    const TSharedRef<SVerticalBox> Rows=AddSection(Page,TEXT("ACCOUNT"));
    // A sign-in in progress (the browser is open, or an approved one is completing) offers only its cancel; the
    // status line above says what it is waiting for.
    if(Service.IsSigningIn())
    {
        Rows->AddSlot().AutoHeight().HAlign(HAlign_Left)[Button(TEXT("Cancel sign-in"),[S]{S->CancelSignIn();})];
        return;
    }
    // Both open the login page in this computer's browser. The QR code is a separate widget, SOpenGolfQrSignIn.
    const bool bConfigured=Service.IsConfigured();
    Rows->AddSlot().AutoHeight().Padding(0,0,0,16)[Text(TEXT("Keep your range shots and club history together."))];
    Rows->AddSlot().AutoHeight().HAlign(HAlign_Left)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,ButtonGap,0)[Button(TEXT("Sign In"),[S]{S->SignInWithBrowser();},bConfigured,true)]
        +SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Create Account"),[S]{S->SignInWithBrowser(true);},bConfigured)]];
}
void SOpenGolfAccountPanel::BuildProfile(const TSharedRef<SVerticalBox>& Page,UOpenGolfOnlineSubsystem& Service)
{
    auto* S=&Service;
    auto Profile=AddSection(Page,TEXT("PROFILE"));
    Profile->AddSlot().AutoHeight()[SNew(STextBlock).Font(Style.GetFont(Style.BodySize)).ColorAndOpacity(Style.TextColor)
        .Text_Lambda([this,S]{return FText::FromString(TEXT("Member since ")+ShotDate(OpenGolfOnline::Text(S->GetProfile(),TEXT("created_at")),TimeZone));})];
    Profile->AddSlot().AutoHeight().Padding(0,6,0,0)[SNew(STextBlock).Tag(TEXT("AccountAdmin"))
        .Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.AccentColor).Text(FText::FromString(TEXT("admin")))
        .Visibility_Lambda([S]{return S->HasPermission(TEXT("ogolf:admin"))?EVisibility::Visible:EVisibility::Collapsed;})];
    Profile->AddSlot().AutoHeight().Padding(0,12,0,6)[Text(TEXT("Display name"),Style.SmallSize,true)];
    Profile->AddSlot().AutoHeight().HAlign(HAlign_Left)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(NameFieldWidth)
            [SAssignNew(NameField,SEditableTextBox).Tag(TEXT("DisplayNameField"))
                .Style(Style.TextField.IsSet()?&Style.TextField.GetValue():&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
                .Font(Style.GetFont(Style.BodySize)).Text_Lambda([this]{return FText::FromString(DisplayNameDraft->Name);})
                .IsReadOnly_Lambda([this]{return !DisplayNameDraft->bEditing || DisplayNameDraft->bSaving;})
                .OnTextChanged_Lambda([this](const FText& Value){if(DisplayNameDraft->bEditing&&!DisplayNameDraft->bSaving){DisplayNameDraft->Name=Value.ToString();DisplayNameDraft->Error.Empty();}})]]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(ButtonGap,0,0,0)
            [SNew(SOpenGolfIconButton).Style(Style).Icon(EOpenGolfAccountIcon::Edit).Tag(TEXT("EditDisplayName"))
                .Label(FText::FromString(TEXT("Edit display name")))
                .IsEnabled_Lambda([this,S]{return !DisplayNameDraft->bEditing&&!DisplayNameDraft->bSaving&&!S->IsBusy();})
                .OnClicked_Lambda([this]{DisplayNameDraft->bEditing=true;DisplayNameDraft->Error.Empty();if(NameField.IsValid())FSlateApplication::Get().SetKeyboardFocus(NameField,EFocusCause::SetDirectly);})]];
    Profile->AddSlot().AutoHeight()[SNew(STextBlock).Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.LabelColor).AutoWrapText(true)
        .Text_Lambda([this]{return FText::FromString(DisplayNameDraft->Error);})
        .Visibility_Lambda([this]{return DisplayNameDraft->Error.IsEmpty()?EVisibility::Collapsed:EVisibility::Visible;})];
}
void SOpenGolfAccountPanel::BuildClubs(const TSharedRef<SVerticalBox>& Page,UOpenGolfOnlineSubsystem& Service)
{
    auto* S=&Service;
    auto Clubs=AddSection(Page,TEXT("CLUB AVERAGES"));
    Clubs->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("Last 90 days. Select a club to view its shots."),Style.SmallSize,true)]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(ButtonGap,0,0,0)[Button(TEXT("Refresh"),[S]{S->LoadCarry();},true)]];
    if(Service.GetCarry().IsEmpty())Clubs->AddSlot().AutoHeight()[Text(TEXT("No saved club statistics yet."))];
    for(const auto& Value:Service.GetCarry())
        if(Value->Type==EJson::Object)Clubs->AddSlot().AutoHeight().Padding(0,0,0,4)[ClubRow(Service,Value->AsObject())];
}
TSharedRef<SWidget> SOpenGolfAccountPanel::ClubRow(UOpenGolfOnlineSubsystem& Service,const TSharedPtr<FJsonObject>& Row)
{
    auto* S=&Service;
    const FString Club=OpenGolfOnline::Text(Row,TEXT("club"));
    const double Shots=Number(Row,TEXT("shots"));
    const FString ShotCount=FString::Printf(TEXT("%.0f %s"),Shots,FMath::IsNearlyEqual(Shots,1.0)?TEXT("shot"):TEXT("shots"));
    // One button per club, tagged ClubRow:<club code>, laid out as table columns.
    return SNew(SButton).ButtonStyle(&Style.Button).IsEnabled_Lambda([S]{return !S->IsBusy();}).Tag(FName(*(TEXT("ClubRow:")+Club)))
#if WITH_ACCESSIBILITY
        .AccessibleText(FText::FromString(FString::Printf(TEXT("View %s shots"),*ClubName(Club))))
#endif
        .OnClicked_Lambda([this,S,Club]{bHistory=true;ConfirmDeleteId.Empty();S->LoadHistory(Club);bRebuild=true;return FReply::Handled();})
        [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(ClubColumn)[Text(ClubAbbreviation(Club))]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(AverageColumn)[Text(FString::Printf(TEXT("%.1f yd average"),Number(Row,TEXT("avg_carry_yd"))))]]
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(ShotsColumn)[Text(ShotCount,Style.SmallSize,true)]]
            +SHorizontalBox::Slot().FillWidth(1)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                [SNew(STextBlock).Text(FText::FromString(TEXT("View"))).Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.AccentColor)]];
}
void SOpenGolfAccountPanel::BuildHistory(const TSharedRef<SVerticalBox>& Page,UOpenGolfOnlineSubsystem& Service)
{
    auto* S=&Service;
    Page->AddSlot().AutoHeight().Padding(0,0,0,SectionGap)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("Newest 200 shots from the last 90 days. Distances in yards; speed in mph; angles in degrees."),Style.SmallSize,true)]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(ButtonGap,0,0,0)[Button(TEXT("Export CSV"),[S]{S->ExportHistory();},true)]];
    auto Header=SNew(SHorizontalBox);
    Header->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(DeleteColumn)];
    for(int32 I=0;I<UE_ARRAY_COUNT(HistoryCaptions);++I)
        Header->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(I==0?TimeColumn:ValueColumn)[Text(I==0?FString::Printf(TEXT("%s (%s)"),HistoryCaptions[0],*TimeZoneName):FString(HistoryCaptions[I]),Style.SmallSize,true)]];
    const float Offset=HistoryList.IsValid()?HistoryList->GetScrollOffset():0;
    HistoryRows.Empty();
    for(const auto& Value:Service.GetHistory())if(Value->Type==EJson::Object)HistoryRows.Add(Value->AsObject());
    auto Table=SNew(SVerticalBox);
    Table->AddSlot().AutoHeight().Padding(0,0,0,6)[Header];
    Table->AddSlot().FillHeight(1)[SAssignNew(HistoryList,SListView<TSharedPtr<FJsonObject>>)
        .ListItemsSource(&HistoryRows).SelectionMode(ESelectionMode::Single)
        .OnGenerateRow_Lambda([this,S](TSharedPtr<FJsonObject> Row,const TSharedRef<STableViewBase>& Owner)
        {return SNew(STableRow<TSharedPtr<FJsonObject>>,Owner)[HistoryLine(*S,Row)];})];
    HistoryList->SetScrollOffset(Offset);
    Page->AddSlot().FillHeight(1)[SNew(SScrollBox).Orientation(Orient_Horizontal)
        +SScrollBox::Slot()[SNew(SBox).WidthOverride(DeleteColumn+TimeColumn+(UE_ARRAY_COUNT(HistoryFields)-1)*ValueColumn)[Table]]];
}
TSharedRef<SWidget> SOpenGolfAccountPanel::HistoryLine(UOpenGolfOnlineSubsystem& Service,const TSharedPtr<FJsonObject>& Row)
{
    auto* S=&Service;
    const FString Id=OpenGolfOnline::Text(Row,TEXT("id"));
    // The first press arms the delete; the Confirm button that replaces it deletes the shot.
    const TSharedRef<SWidget> Delete=ConfirmDeleteId==Id
        ?Button(TEXT("Confirm"),[this,S,Id]{S->DeleteShot(Id);ConfirmDeleteId.Empty();bRebuild=true;},true,true,Style.SmallSize)
        :StaticCastSharedRef<SWidget>(SNew(SOpenGolfIconButton).Style(Style).Icon(EOpenGolfAccountIcon::Close).Size(DeleteIconSize).Tag(TEXT("DeleteShot"))
            .Label(FText::FromString(TEXT("Delete shot"))).IsEnabled_Lambda([S]{return !S->IsBusy();}).OnClicked_Lambda([this,Id]{ConfirmDeleteId=Id;bRebuild=true;}));
    auto Line=SNew(SHorizontalBox);
    Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(DeleteColumn).HAlign(HAlign_Left)[Delete]];
    for(int32 I=0;I<UE_ARRAY_COUNT(HistoryFields);++I)
        Line->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(I==0?TimeColumn:ValueColumn)[Text(HistoryValue(Row,I,TimeZone),Style.SmallSize)]];
    return Line;
}
