// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "OpenGolfAccountStyle.h"
class UOpenGolfOnlineSubsystem;
class FJsonObject;
class SBox;
class SVerticalBox;
class SScrollBox;
class SEditableTextBox;
template<typename ItemType> class SListView;

/** Settings owns this draft so changing categories or rebuilding a page preserves an unsaved name. */
struct OPENGOLFONLINE_API FOpenGolfDisplayNameDraft : public TSharedFromThis<FOpenGolfDisplayNameDraft>
{
    FString Name, AccountKey, Error;
    bool bEditing = false, bSaving = false;
    void Synchronize(const UOpenGolfOnlineSubsystem* Online);
    bool IsDirty(const UOpenGolfOnlineSubsystem* Online) const;
    FString ValidationError(const UOpenGolfOnlineSubsystem* Online) const;
    void Discard(const UOpenGolfOnlineSubsystem* Online);
    void Apply(UOpenGolfOnlineSubsystem* Online);
};

/** Which part of the account an SOpenGolfAccountPanel shows. */
enum class EOpenGolfAccountPage : uint8
{
    /**
     * Sign In and Create Account while signed out (the QR code is SOpenGolfQrSignIn, placed by the host); the profile
     * (member since, display name) while signed in. Laid out at its
     * content height, without a scroll box of its own, so a host can place it among its own rows.
     */
    Account,
    /** Club averages, then one club's shot history; a sign-in hint while signed out. Fills and scrolls. */
    Shots
};

/**
 * One part of the player's account (Page). A host that draws its own page header passes ShowHeading(false) and uses
 * GetTitle and HandleBack. Shot times show in TimeZone.
 */
class OPENGOLFONLINE_API SOpenGolfAccountPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SOpenGolfAccountPanel) : _Online(nullptr), _ShowHeading(true), _Page(EOpenGolfAccountPage::Account) {}
    SLATE_ARGUMENT(UOpenGolfOnlineSubsystem*, Online)
    SLATE_ARGUMENT(FOpenGolfAccountStyle, Style)
    /** Draws the title row (back while in shot history, title, sign out). False when the host draws it. */
    SLATE_ARGUMENT(bool, ShowHeading)
    SLATE_ARGUMENT(EOpenGolfAccountPage, Page)
    /** IANA time zone for shot times and the member-since date, such as "America/Los_Angeles"; empty is this computer's. */
    SLATE_ARGUMENT(FString, TimeZone)
    /** Short name of TimeZone for the time column's caption, such as "PT"; empty reads "local". */
    SLATE_ARGUMENT(FString, TimeZoneName)
    /** Shared with the host's Apply/Discard controls. A standalone panel owns its own draft. */
    SLATE_ARGUMENT(TSharedPtr<FOpenGolfDisplayNameDraft>, DisplayNameDraft)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry&, double, float) override;
    /** "Account", "Sign in to <application>", "Shot history" or "<club> — shot history". Changes without the host rebuilding. */
    FText GetTitle() const;
    /** Leaves one club's shot history for the club list. False when there is no inner page to leave. */
    bool HandleBack();
private:
    void Rebuild();
    /** Club statistics are requested only for the Shots page, once the profile has synced. */
    void LoadCarryIfReady();
    TSharedRef<SWidget> BuildHeading(UOpenGolfOnlineSubsystem& Service);
    void BuildSignedOut(const TSharedRef<SVerticalBox>& Page, UOpenGolfOnlineSubsystem& Service);
    void BuildProfile(const TSharedRef<SVerticalBox>& Page, UOpenGolfOnlineSubsystem& Service);
    void BuildClubs(const TSharedRef<SVerticalBox>& Page, UOpenGolfOnlineSubsystem& Service);
    void BuildHistory(const TSharedRef<SVerticalBox>& Page, UOpenGolfOnlineSubsystem& Service);
    TSharedRef<SWidget> ClubRow(UOpenGolfOnlineSubsystem& Service, const TSharedPtr<FJsonObject>& Row);
    TSharedRef<SWidget> HistoryLine(UOpenGolfOnlineSubsystem& Service, const TSharedPtr<FJsonObject>& Row);
    /** A grouped block of the page with an optional caption; returns the box its rows go in. */
    TSharedRef<SVerticalBox> AddSection(const TSharedRef<SVerticalBox>& Page, const FString& Caption);
    /** Size 0 is the body size; muted text uses the label color. */
    TSharedRef<SWidget> Text(const FString& Value, int32 Size = 0, bool bMuted = false) const;
    TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool bEnabled = true, bool bPrimary = false, int32 Size = 0);
    TWeakObjectPtr<UOpenGolfOnlineSubsystem> Online;
    /** Buttons and borders point at this copy's styles and brushes. */
    FOpenGolfAccountStyle Style;
    TSharedPtr<SBox> Content;
    TSharedPtr<SBox> PageContent;
    TSharedPtr<SScrollBox> Scroll;
    TSharedPtr<FOpenGolfDisplayNameDraft> DisplayNameDraft;
    TSharedPtr<SEditableTextBox> NameField;
    TSharedPtr<SListView<TSharedPtr<FJsonObject>>> HistoryList;
    TArray<TSharedPtr<FJsonObject>> HistoryRows;
    int32 SeenShape = -1;
    uint64 SeenHistoryRevision = MAX_uint64;
    FString ConfirmDeleteId, LastAccountKey, TimeZone, TimeZoneName;
    EOpenGolfAccountPage PageType = EOpenGolfAccountPage::Account;
    bool bShowHeading = true, bLoadedCarry = false, bWasSignedIn = false, bHistory = false, bRebuild = false;
};
