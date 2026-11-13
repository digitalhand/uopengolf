// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "OpenGolfSettings.h"
#include "UI/Menu/DisplayOptions.h"
#include "Styling/SlateTypes.h"
#include "OpenGolfAccountStyle.h"
class AUOpenGolfPlayerController;
class SOpenGolfAccountPanel;
class UOpenGolfOnlineSubsystem;
class SVerticalBox;
class SBox;
class SOverlay;

class SMainMenu : public SCompoundWidget
{
public:
    // Pointer arguments need a default: SLATE_ARGUMENT leaves them uninitialized when a caller omits them.
    SLATE_BEGIN_ARGS(SMainMenu) : _Controller(nullptr), _Online(nullptr) {}
    SLATE_ARGUMENT(AUOpenGolfPlayerController*, Controller)
    /** The online service; by default the game instance's. Tests pass a stubbed one. */
    SLATE_ARGUMENT(UOpenGolfOnlineSubsystem*, Online)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SMainMenu() override;
    void Open();
    void NavigateBack() { Back(); }
    /** Opens the menu, if closed, at Settings › Simulator, where the launch monitor is chosen and connected. */
    void OpenLaunchMonitorSettings();
    bool IsOpen() const { return bOpen; }
    /** True once Training has been entered: the home screen is then the pause menu. */
    bool IsPauseMenu() const { return bStarted; }
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
    virtual void Tick(const FGeometry&, double, float) override;
private:
    struct FVideo
    {
        FIntPoint Resolution;
        int32 Mode = 1, VSync = 0, FPS = 60, Scale = 100;
        int32 Quality[10] = {};
        bool operator==(const FVideo& Other) const;
        bool operator!=(const FVideo& Other) const { return !(*this == Other); }
    };
    static FVideo ReadVideo();
    /** The display as the engine and Slate report it: exclusive modes, desktop and work area. */
    static DisplayOptions::FFacts Facts();
    enum class ERoute : uint8 { Home, Pause, Settings, Shots };
    bool InSettings() const { return Route == ERoute::Settings || Route == ERoute::Shots; }
    bool InShots() const { return Route == ERoute::Shots; }
    void Rebuild();
    void RefreshSettings();
    void FocusPage();
    /**
     * On every page: the title, the launch-monitor status, the account icon (sign in while signed out, sign out while
     * signed in) and the cog, the only way into Settings.
     */
    TSharedRef<SWidget> BuildTopBar();
    /** Display confirmation, unsaved changes, return to main menu and exit. */
    void BuildDialog(TSharedRef<SVerticalBox> Body);
    /** Settings › Player › Shot history: club averages, then one club's shots, under this menu's page header. */
    void BuildShots(TSharedRef<SVerticalBox> Body);
    /** The home page, or the pause menu once Training has started. */
    void BuildHome(TSharedRef<SVerticalBox> Body);
    void BuildSettings(TSharedRef<SVerticalBox> Body);
    void AddChoice(TSharedRef<SVerticalBox> Rows, const FString& Label, const FString& Help,
        TArray<FString> Labels, TArray<int32> Values, TFunction<int32()> Get, TFunction<void(int32)> Set, bool Enabled = true);
    void AddPreference(TSharedRef<SVerticalBox> Rows, const TCHAR* Label, const TCHAR* Help,
        int32 GolfSettings::FPreferences::*Member, TArray<FString> Labels, TArray<int32> Values);
    void Disabled(TSharedRef<SVerticalBox> Rows, const TCHAR* Label, const TCHAR* Reason);
    /** Adds a settings row (label and help on the left, Control on the right) and returns it. */
    TSharedRef<SWidget> AddRow(TSharedRef<SVerticalBox> Rows, const FString& Label, const FString& Help, TSharedRef<SWidget> Control, bool Enabled = true);
    /** Whether a row is shown under the current search. */
    bool Matches(const FString& Label, const FString& Help) const;
    /** Settings → Player: the account (sign in, or the profile and Shot history), Time zone and player preferences. */
    void BuildPlayer(TSharedRef<SVerticalBox> Rows);
    /** Settings → Simulator: the live connection, the provider choice and the Open Connect connector. */
    void BuildSimulator(TSharedRef<SVerticalBox> Rows);
    FString LiveStatus() const;
    /** Saves and applies one change at once (no Apply), keeping other pending edits; Error shows if it cannot be saved. */
    void CommitNow(TFunction<void(GolfSettings::FPreferences&)> Change, const TCHAR* Error);
    TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool Primary = false, int32 Size = 18);
    /** A settings category; the selected one is highlighted. */
    TSharedRef<SWidget> NavButton(const FString& Label, TFunction<void()> Action, bool Selected);
    /** One of the stacked, equal-width buttons on the home, pause and confirmation pages. */
    void AddMenuButton(TSharedRef<SVerticalBox> Body, const FString& Label, TFunction<void()> Action, bool Primary = false);
    UOpenGolfOnlineSubsystem* OnlineService() const;
    /** Signed in with the profile loaded: the profile and Shot history are shown in Settings › Player. */
    bool HasProfile() const;
    TSharedRef<SWidget> Text(const FString& Value, int32 Size = 18, bool Muted = false);
    /** Settings opened at a category, with drafts taken from the saved preferences. From a top-bar click, defer the rebuild. */
    void OpenSettings(int32 InCategory, bool bDeferRebuild = false);
    /** The cog: opens Settings at Player, or leaves Shot history for Player; nothing while Settings is already open. */
    void OnSettingsIcon();
    /** The sign-in icon: Settings › Player, where the sign-in choices are, keeping pending edits. */
    void OnSignInIcon();
    class UOpenLaunchMonitorSubsystem* Monitor() const;
    FString ConnectionSummary() const;
    static constexpr int32 PlayerCategory = 0;
    static constexpr int32 SimulatorCategory = 1;
    void Back();
    void Apply();
    void Commit();
    void RevertDisplay();
    void Resume();
    bool Dirty() const;
    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TSharedPtr<SBox> Content, ModalContent;
    /**
     * Built pages, reused when the route returns to them, by key: "home", "pause", "shots" and
     * "settings:<category>:<search>". Cleared on entering Settings from outside it, by RefreshSettings (an applied or
     * discarded change) and by Shot history's View, so that page always opens on fresh club averages.
     */
    TMap<FString, TSharedPtr<SVerticalBox>> Pages;
    /** The widget that had keyboard focus when each page was left, restored on return. */
    TMap<FString, TWeakPtr<SWidget>> PageFocus;
    FString PageKey;
    ERoute Route = ERoute::Home;
    bool bReturnToTraining = false, bFocusPending = false;
    double TransitionStart = -1;
    /** Shared with the account widgets; buttons point at its styles, so it lives as long as the menu. */
    FOpenGolfAccountStyle Style;
    FButtonStyle NavStyle;
    /** Rebuilt with the Settings page so its font follows the Text size preference. */
    FSearchBoxStyle SearchStyle;
    /** Set only when a host passes Online. */
    TWeakObjectPtr<UOpenGolfOnlineSubsystem> OnlineOverride;
    /** The Shot history panel while that page is shown; it handles Back inside one club's shots. */
    TSharedPtr<SOpenGolfAccountPanel> ShotsPanel;
    GolfSettings::FPreferences Draft, Saved;
    FVideo Video, SavedVideo;
    /**
     * bStarted: Training has been entered, so the home page is the pause menu.
     * bWasSignedIn: the session as last seen; signing out leaves Shot history.
     */
    bool bOpen = false, bConfirmDisplay = false, bStarted = false, bWasSignedIn = false;
    /** Rebuild on the next tick: set from callbacks that run inside widgets the rebuild would replace. */
    bool bRebuildPending = false;
    double ConfirmDeadline = 0;
    FString Dialog, Search, SaveError;
    int32 Category = 0;
};
