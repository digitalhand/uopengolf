// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "OpenGolfSettings.h"
#include "Styling/SlateTypes.h"
class AUOpenGolfPlayerController;
class SVerticalBox;
class SBox;

class SMainMenu : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMainMenu) {} SLATE_ARGUMENT(AUOpenGolfPlayerController*, Controller) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    virtual ~SMainMenu() override;
    void Open();
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
    void Rebuild();
    void BuildSettings(TSharedRef<SVerticalBox> Body);
    void AddChoice(TSharedRef<SVerticalBox> Rows, const FString& Label, const FString& Help,
        TArray<FString> Labels, TArray<int32> Values, TFunction<int32()> Get, TFunction<void(int32)> Set, bool Enabled = true);
    void AddPreference(TSharedRef<SVerticalBox> Rows, const TCHAR* Label, const TCHAR* Help,
        int32 GolfSettings::FPreferences::*Member, TArray<FString> Labels, TArray<int32> Values);
    void Disabled(TSharedRef<SVerticalBox> Rows, const TCHAR* Label, const TCHAR* Reason);
    /** A settings row: label and help on the left, Control on the right. */
    void AddRow(TSharedRef<SVerticalBox> Rows, const FString& Label, const FString& Help, TSharedRef<SWidget> Control, bool Enabled = true);
    /** Whether a row is shown under the current search. */
    bool Matches(const FString& Label, const FString& Help) const;
    /** Settings → Simulator: the live connection, the provider choice and the Open Connect connector. */
    void BuildSimulator(TSharedRef<SVerticalBox> Rows);
    FString LiveStatus() const;
    /** Saves and applies a launch-monitor connection change at once (no Apply), keeping other pending edits. */
    void CommitConnection(TFunction<void(GolfSettings::FPreferences&)> Change);
    TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool Primary = false, int32 Size = 18);
    TSharedRef<SWidget> Text(const FString& Value, int32 Size = 18, bool Muted = false);
    /** Settings opened at a category, with drafts taken from the saved preferences. */
    void OpenSettings(int32 InCategory);
    class UOpenLaunchMonitorSubsystem* Monitor() const;
    FString ConnectionSummary() const;
    static constexpr int32 SimulatorCategory = 1;
    void Back();
    void Apply();
    void Commit();
    void RevertDisplay();
    void Resume();
    bool Dirty() const;
    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TSharedPtr<SBox> Content;
    FButtonStyle ButtonStyle, PrimaryStyle;
    GolfSettings::FPreferences Draft, Saved;
    FVideo Video, SavedVideo;
    /** bStarted: training has been entered once, so the home screen acts as the pause menu. */
    bool bOpen = false, bSettings = false, bConfirmDisplay = false, bStarted = false;
    /** Rebuild on the next tick: set from callbacks that run inside widgets the rebuild would replace. */
    bool bRebuildPending = false;
    double ConfirmDeadline = 0;
    FString Dialog, Search, SaveError;
    int32 Category = 0;
};
