// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "GameUpdateTypes.h"
#include "OpenGolfAccountStyle.h"
#include "Styling/SlateTypes.h"
class UGameUpdateSubsystem;

/**
 * The update screen: what's new and the download size, then a progress bar while the update downloads and is checked,
 * then a note that the game is restarting. Built once; every part follows the updater's state as it changes.
 */
class SUpdatePanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SUpdatePanel) : _Updater(nullptr), _Style(nullptr) {}
    SLATE_ARGUMENT(UGameUpdateSubsystem*, Updater)
    /** The menu's style; it must outlive the panel. */
    SLATE_ARGUMENT(const FOpenGolfAccountStyle*, Style)
    /** Later or Close. */
    SLATE_EVENT(FSimpleDelegate, OnClose)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

private:
    TAttribute<EVisibility> VisibleWhen(TFunction<bool(const UGameUpdateSubsystem&)> Test) const;
    TSharedRef<SWidget> MakeButton(TAttribute<FText> Label, TFunction<void()> Action, bool bPrimary, TAttribute<EVisibility> Visible);

    TWeakObjectPtr<UGameUpdateSubsystem> Updater;
    const FOpenGolfAccountStyle* Style = nullptr;
    FProgressBarStyle BarStyle;
};

/** Short update texts shared by the update screen and Settings. */
namespace UpdateText
{
    /** "Update to v0.0.8". */
    FString Title(const UGameUpdateSubsystem& Updater);
    /** Settings' button: Check for updates, Checking..., or Update to v0.0.X. */
    FString ButtonLabel(const UGameUpdateSubsystem* Updater);
    /** Settings' status line, such as "Up to date (v0.0.X)." */
    FString Status(const UGameUpdateSubsystem* Updater);
    /** GitHub's Markdown release notes as plain lines: headings without #, bullets as •. */
    FString PlainNotes(const FString& Markdown);
}
