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
    /** Between the profile's name column and the host's ProfileAside content. */
    constexpr float ProfileAsideGap = 32;
    /** The Account page keeps this height signed out, loading and signed in, so the host's rows below do not jump. */
    constexpr float AccountMinHeight = 170;
    constexpr float ClubColumn = 80, SpeedColumn = 150, CarryColumn = 130, TotalColumn = 130, ShotsColumn = 120;
    /** Club rows' content padding, inside the button style's NormalPadding; the column captions are inset to match. */
    constexpr float ButtonContentPadding = 4, ButtonContentPaddingY = 2;
    const TCHAR* BackTag = TEXT("PageBack");
    const TCHAR* HistoryFields[] = {TEXT("shot_at"),TEXT("carry_yd"),TEXT("roll_yd"),TEXT("ball_speed_mph"),TEXT("launch_angle_deg"),TEXT("launch_direction_deg"),TEXT("total_spin_rpm"),TEXT("spin_axis_deg"),TEXT("back_spin_rpm"),TEXT("side_spin_rpm"),TEXT("face_angle_deg"),TEXT("club_path_deg"),TEXT("face_to_path_deg"),TEXT("attack_angle_deg"),TEXT("dynamic_loft_deg")};
    constexpr float ShotTimeWidth = 245, ShotMetricWidth = 154;
    const TCHAR* HistoryCaptions[] = {TEXT("Time (local)"), TEXT("Carry (yd)"), TEXT("Roll (yd)"), TEXT("Ball speed (mph)"),
        TEXT("Launch angle (deg)"), TEXT("Direction (deg)"), TEXT("Spin (rpm)"), TEXT("Spin axis (deg)"), TEXT("Backspin (rpm)"),
        TEXT("Sidespin (rpm)"), TEXT("Face angle (deg)"), TEXT("Club path (deg)"), TEXT("Face to path (deg)"), TEXT("Attack angle (deg)"), TEXT("Dynamic loft (deg)")};
    static_assert(UE_ARRAY_COUNT(HistoryFields) == UE_ARRAY_COUNT(HistoryCaptions), "Every history field has a caption.");

    TOptional<double> AverageCarry(const FJsonObject& Row)
    {
        double Carry = 0.0;
        if (Row.TryGetNumberField(TEXT("avg_carry_yd"), Carry) && FMath::IsFinite(Carry))
            return Carry;
        return {};
    }

    /** A live average cell's last text, and the service state it was made from. */
    struct FAverageCellCache
    {
        uint64 Revision = MAX_uint64;
        bool bLoading = false;
        FText Text;
    };

    /** A club's average to one decimal with its unit; "…" while that club is still loading, "—" when no shot measured it. */
    FString AverageCell(const UOpenGolfOnlineSubsystem* Online, const FString& Club, TOptional<double> FOpenGolfClubShotAverages::*Field, const TCHAR* Unit)
    {
        const FOpenGolfClubShotAverages* Averages = Online ? Online->FindClubAverages(Club) : nullptr;
        if (!Averages) return Online && Online->IsLoadingClubAverages() ? TEXT("…") : TEXT("—");
        const TOptional<double>& Value = Averages->*Field;
        return Value.IsSet() ? FString::Printf(TEXT("%.1f %s"), Value.GetValue(), Unit) : TEXT("—");
    }

    /** One history cell: the local shot time, or a number to one decimal. Face to path is derived. */
    FString HistoryValue(const TSharedPtr<FJsonObject>& Row,int32 Field)
    {
        if(Field==0)return ShotTime(OpenGolfOnline::Text(Row,HistoryFields[0]));
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
    Online=Args._Online;Style=Args._Style;bShowHeading=Args._ShowHeading;PageType=Args._Page;ProfileAside=Args._ProfileAside;
    ClubRowStyle = Style.Button;
    ClubRowStyle.SetNormal(FSlateNoResource());
    DisplayNameDraft=Args._DisplayNameDraft.IsValid()?Args._DisplayNameDraft:MakeShared<FOpenGolfDisplayNameDraft>();
    DisplayNameDraft->Synchronize(Online.Get());
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
        bWasSignedIn=Online->HasSession();bHistory=false;bLoadedCarry=false;DeleteAction=nullptr;SelectedId.Empty();
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
    if (DeleteAction) { DeleteAction = nullptr; bRebuild = true; return true; }
    if(!bHistory)return false;
    bHistory=false;DeleteAction=nullptr;SelectedId.Empty();
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
    if(LastAccountKey!=Service.GetAccountKey()){LastAccountKey=Service.GetAccountKey();bLoadedCarry=false;DeleteAction=nullptr;SelectedId.Empty();}
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
        else if (DeleteAction)
        {
            auto Section = AddSection(Page, DeletePrompt);
            CancelButton = Button(TEXT("Cancel"), [this] { DeleteAction = nullptr; bRebuild = true; });
            Section->AddSlot().AutoHeight()[SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth().Padding(0,0,ButtonGap,0)[CancelButton.ToSharedRef()]
                + SHorizontalBox::Slot().AutoWidth()[Button(DeleteLabel, [this]
                {
                    if (DeleteAction) { auto Action = MoveTemp(DeleteAction); DeleteAction = nullptr; Action(); }
                    bRebuild = true;
                })]];
        }
        else if(bHistory)BuildHistory(Page,Service);
        else BuildClubs(Page,Service);
        if(bHistory)PageContent->SetContent(Page);
        else {PageContent->SetContent(SAssignNew(Scroll,SScrollBox)+SScrollBox::Slot()[Page]);Scroll->SetScrollOffset(Offset);}
    }
    if (DeleteAction && CancelButton) FSlateApplication::Get().SetKeyboardFocus(CancelButton, EFocusCause::Navigation);
    else if(HadFocus)
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
            .OnClicked_Lambda([this]{if(Online.IsValid())Online->SignOut();DisplayNameDraft->Synchronize(Online.Get());bHistory=false;DeleteAction=nullptr;SelectedId.Empty();bRebuild=true;})];
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
    auto Section=AddSection(Page,TEXT("PROFILE"));
    // The name and membership on the left; host content, such as an avatar picker, fills the rest of the width.
    auto Profile=SNew(SVerticalBox);
    Section->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[Profile]
        +SHorizontalBox::Slot().FillWidth(1).Padding(ProfileAsideGap,0,0,0)
            [ProfileAside?ProfileAside():SNullWidget::NullWidget]];
    Profile->AddSlot().AutoHeight()[SNew(STextBlock).Font(Style.GetFont(Style.BodySize)).ColorAndOpacity(Style.TextColor)
        .Text_Lambda([S]{return FText::FromString(TEXT("Member since ")+ShotDate(OpenGolfOnline::Text(S->GetProfile(),TEXT("created_at"))));})];
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
    auto Clubs=AddSection(Page,TEXT(""));
    Clubs->AddSlot().AutoHeight().Padding(0,0,0,SectionGap)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("Club averages  /  Last 90 days"),Style.SmallSize,true)]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SOpenGolfIconButton).Style(Style).Icon(EOpenGolfAccountIcon::Refresh)
            .Tag(TEXT("RefreshHistory")).Label(FText::FromString(TEXT("Refresh club averages")))
            .IsEnabled_Lambda([S]{return !S->IsBusy();}).OnClicked_Lambda([S]{S->LoadCarry();})]
        +SHorizontalBox::Slot().AutoWidth().Padding(ButtonGap,0,0,0)[Button(TEXT("Clear all"), [this,S]
        {
            ConfirmDeletion(TEXT("Delete recent shots for every club? Includes pending shots and the newest 200 per club from the last 90 days."),
                TEXT("Delete all"), [S] { S->ClearAllHistory(); });
        },true,false,Style.SmallSize)]];
    TArray<TSharedPtr<FJsonObject>> SortedCarry;
    for (const TSharedPtr<FJsonValue>& Value : Service.GetCarry())
    {
        if (Value.IsValid() && Value->Type == EJson::Object && Value->AsObject().IsValid())
            SortedCarry.Add(Value->AsObject());
    }
    if (SortedCarry.IsEmpty())
        Clubs->AddSlot().AutoHeight()[Text(TEXT("No saved club statistics yet."))];
    else
    {
        // Column captions, inset by the club buttons' padding so they line up with the rows below.
        auto Header = SNew(SHorizontalBox);
        const TPair<const TCHAR*, float> Columns[] = {{TEXT("Club"), ClubColumn}, {TEXT("Ball speed"), SpeedColumn},
            {TEXT("Carry"), CarryColumn}, {TEXT("Total"), TotalColumn}, {TEXT("Shots"), ShotsColumn}};
        for (const auto& Column : Columns)
            Header->AddSlot().FillWidth(Column.Value)[Text(Column.Key, Style.SmallSize, true)];
        Header->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(24)];
        const float Inset = ButtonContentPadding + Style.Button.NormalPadding.Left;
        Clubs->AddSlot().AutoHeight().Padding(Inset, 0, Inset, 4)[Header];
    }
    SortedCarry.StableSort([](const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B)
    {
        const TOptional<double> Left = AverageCarry(*A);
        const TOptional<double> Right = AverageCarry(*B);
        return Left.IsSet() && (!Right.IsSet() || Left.GetValue() > Right.GetValue());
    });
    for (const TSharedPtr<FJsonObject>& Row : SortedCarry)
        Clubs->AddSlot().AutoHeight().Padding(0, 0, 0, 4)[ClubRow(Service, Row)];
}
TSharedRef<SWidget> SOpenGolfAccountPanel::ClubRow(UOpenGolfOnlineSubsystem& Service,const TSharedPtr<FJsonObject>& Row)
{
    auto* S=&Service;
    const FString Club=OpenGolfOnline::Text(Row,TEXT("club"));
    const double Shots=Number(Row,TEXT("shots"));
    const TOptional<double> Carry = AverageCarry(*Row);
    const FString CarryLabel = Carry.IsSet() ? FString::Printf(TEXT("%.1f yd"), Carry.GetValue()) : TEXT("—");
    const FString ShotCount=FString::Printf(TEXT("%.0f %s"),Shots,FMath::IsNearlyEqual(Shots,1.0)?TEXT("shot"):TEXT("shots"));
    // Ball speed and total are loaded per club after the list, so their cells follow the service as they arrive. Each
    // cell formats its text again only when the averages or the loading state change, not every frame.
    const TWeakObjectPtr<UOpenGolfOnlineSubsystem> Weak=S;
    auto Live=[this,Weak,Club](TOptional<double> FOpenGolfClubShotAverages::*Field,const TCHAR* Unit)
    {
        const TSharedRef<FAverageCellCache> Cache=MakeShared<FAverageCellCache>();
        return SNew(STextBlock).Font(Style.GetFont(Style.BodySize)).ColorAndOpacity(Style.TextColor)
            .Text_Lambda([Weak,Club,Field,Unit,Cache]
            {
                const UOpenGolfOnlineSubsystem* Online=Weak.Get();
                const uint64 Revision=Online?Online->GetAveragesRevision():0;
                const bool bLoading=Online&&Online->IsLoadingClubAverages();
                if (Cache->Revision!=Revision || Cache->bLoading!=bLoading)
                {
                    Cache->Revision=Revision; Cache->bLoading=bLoading;
                    Cache->Text=FText::FromString(AverageCell(Online,Club,Field,Unit));
                }
                return Cache->Text;
            });
    };
    // One button per club, tagged ClubRow:<club code>, laid out as table columns.
    return SNew(SButton).ButtonStyle(&ClubRowStyle).ContentPadding(FMargin(ButtonContentPadding,ButtonContentPaddingY))
        .IsEnabled_Lambda([S]{return !S->IsBusy();}).Tag(FName(*(TEXT("ClubRow:")+Club)))
#if WITH_ACCESSIBILITY
        .AccessibleText(FText::FromString(FString::Printf(TEXT("View %s shots"),*ClubName(Club))))
#endif
        .OnClicked_Lambda([this,S,Club]{bHistory=true;DeleteAction=nullptr;SelectedId.Empty();S->LoadHistory(Club);bRebuild=true;return FReply::Handled();})
        [SNew(SHorizontalBox)
            +SHorizontalBox::Slot().FillWidth(ClubColumn).VAlign(VAlign_Center)[Text(ClubAbbreviation(Club))]
            +SHorizontalBox::Slot().FillWidth(SpeedColumn).VAlign(VAlign_Center)[Live(&FOpenGolfClubShotAverages::BallSpeedMph,TEXT("mph"))]
            +SHorizontalBox::Slot().FillWidth(CarryColumn).VAlign(VAlign_Center)[Text(CarryLabel)]
            +SHorizontalBox::Slot().FillWidth(TotalColumn).VAlign(VAlign_Center)[Live(&FOpenGolfClubShotAverages::TotalYd,TEXT("yd"))]
            +SHorizontalBox::Slot().FillWidth(ShotsColumn).VAlign(VAlign_Center)[Text(ShotCount,Style.SmallSize,true)]
            +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(24).HAlign(HAlign_Right)
                [Text(TEXT("\u203a"),Style.BodySize,true)]]];
}
void SOpenGolfAccountPanel::ConfirmDeletion(FString Prompt, FString Label, TFunction<void()> Action)
{
    DeletePrompt = MoveTemp(Prompt);
    DeleteLabel = MoveTemp(Label);
    const FString Owner = Online->GetAccountKey();
    const FString Club = bHistory ? Online->GetHistoryClub() : FString();
    DeleteAction = [this, Owner, Club, Action = MoveTemp(Action)]
    {
        if (Online.IsValid() && Online->HasSession() && Online->GetAccountKey() == Owner
            && (!bHistory || Online->GetHistoryClub() == Club)) Action();
    };
    bRebuild = true;
}

void SOpenGolfAccountPanel::BuildHistory(const TSharedRef<SVerticalBox>& Page, UOpenGolfOnlineSubsystem& Service)
{
    auto* S = &Service;
    const FString Club = S->GetHistoryClub();
    Page->AddSlot().AutoHeight().Padding(0,0,0,SectionGap)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(TEXT("Last 90 days"), Style.SmallSize, true)]
        + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("CSV"), [S] { S->ExportHistory(); }, true, false, Style.SmallSize)]
        + SHorizontalBox::Slot().AutoWidth().Padding(ButtonGap,0,0,0)[Button(TEXT("Clear club"), [this,S,Club]
        {
            ConfirmDeletion(TEXT("Clear ") + ClubName(Club) + TEXT(" history? Includes pending shots and the newest 200 shots from the last 90 days."),
                TEXT("Clear club"), [S,Club] { S->ClearClubHistory(Club); });
        },true,false,Style.SmallSize)]];
    const float Offset = HistoryList ? HistoryList->GetScrollOffset() : 0;
    HistoryRows.Reset();
    TSharedPtr<FJsonObject> Selected;
    for (const auto& Value : Service.GetHistory())
    {
        if (!Value.IsValid() || Value->Type != EJson::Object) continue;
        auto Row = Value->AsObject();
        HistoryRows.Add(Row);
        if (OpenGolfOnline::Text(Row,TEXT("id")) == SelectedId) Selected = Row;
    }
    auto Table = SNew(SVerticalBox);
    Table->AddSlot().AutoHeight()[HistoryLine(nullptr)];
    if (HistoryRows.IsEmpty())
        Page->AddSlot().AutoHeight().Padding(0,8)[Text(Service.IsBusy() ? TEXT("Loading shots...") : TEXT("No shots in this period."), Style.SmallSize, true)];
    Table->AddSlot().FillHeight(1)[SAssignNew(HistoryList,SListView<TSharedPtr<FJsonObject>>)
        .ListItemsSource(&HistoryRows).SelectionMode(ESelectionMode::Single)
        .OnSelectionChanged_Lambda([this](TSharedPtr<FJsonObject> Row, ESelectInfo::Type) { ShowShotDetails(Row); })
        .OnGenerateRow_Lambda([this](TSharedPtr<FJsonObject> Row, const TSharedRef<STableViewBase>& Owner)
        { return SNew(STableRow<TSharedPtr<FJsonObject>>,Owner)[HistoryLine(Row)]; })];
    const float HorizontalOffset = ShotScroll ? ShotScroll->GetScrollOffset() : 0.f;
    constexpr float TableWidth = ShotTimeWidth + (UE_ARRAY_COUNT(HistoryFields) - 1) * ShotMetricWidth + 24;
    Page->AddSlot().FillHeight(1)[SAssignNew(ShotScroll,SScrollBox).Tag(TEXT("ShotTableScroll"))
        .Orientation(Orient_Horizontal).ScrollBarAlwaysVisible(true)
        +SScrollBox::Slot()[SNew(SBox).WidthOverride(TableWidth)[Table]]];
    ShotScroll->SetScrollOffset(HorizontalOffset);
    Page->AddSlot().AutoHeight()[SAssignNew(Details,SBox)];
    if (Selected) HistoryList->SetSelection(Selected);
    ShowShotDetails(Selected);
    HistoryList->SetScrollOffset(Offset);
}

TSharedRef<SWidget> SOpenGolfAccountPanel::HistoryLine(const TSharedPtr<FJsonObject>& Row)
{
    auto Line = SNew(SHorizontalBox);
    for (int32 I = 0; I < UE_ARRAY_COUNT(HistoryFields); ++I)
    {
        const FString Value = Row ? HistoryValue(Row, I) : FString(HistoryCaptions[I]);
        Line->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(I == 0 ? ShotTimeWidth : ShotMetricWidth)
            .HAlign(I == 0 ? HAlign_Left : HAlign_Right).Padding(6,4)
            [SNew(STextBlock).Text(FText::FromString(Value.IsEmpty() ? TEXT("\u2014") : Value))
                .Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Row ? Style.TextColor : Style.LabelColor)
                .AutoWrapText(false)]];
    }
    return Line;
}

void SOpenGolfAccountPanel::ShowShotDetails(TSharedPtr<FJsonObject> Row)
{
    SelectedId = OpenGolfOnline::Text(Row, TEXT("id"));
    if (!Details) return;
    if (!Row) { Details->SetContent(SNullWidget::NullWidget); return; }
    auto Body = SNew(SVerticalBox);
    const FString When = HistoryValue(Row, 0);
    auto* S = Online.Get();
    const FString Id = SelectedId;
    Body->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(ClubName(S->GetHistoryClub()) + TEXT("  /  ") + When)]
        + SHorizontalBox::Slot().AutoWidth()[Button(TEXT("Delete shot"), [this,S,Id,When]
        {
            ConfirmDeletion(TEXT("Delete the ") + ClubName(S->GetHistoryClub()) + TEXT(" shot from ") + When + TEXT("?"),
                TEXT("Delete shot"), [S,Id] { S->DeleteShot(Id); });
        }, !Id.IsEmpty(), false, Style.SmallSize)]];
    Details->SetContent(SNew(SBorder).BorderImage(&Style.Section).Padding(SectionPadding)[Body]);
}
