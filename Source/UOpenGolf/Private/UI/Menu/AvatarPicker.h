// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "OpenGolfAccountStyle.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class UOpenGolfOnlineSubsystem;
namespace GolfSettings { struct FPreferences; }

/**
 * Settings › Player › PROFILE: the signed-in player's avatar. The account picture comes first when the account has
 * one, then the built-in avatars; the one shown on the course has an accent ring. A pick edits the host's draft, so it
 * shares Settings' Apply and Discard.
 */
class SAvatarPicker : public SCompoundWidget
{
public:
    /** The built-in avatar's id, or empty for the account picture. */
    DECLARE_DELEGATE_OneParam(FOnPicked, const FString&);

    SLATE_BEGIN_ARGS(SAvatarPicker) : _Online(nullptr) {}
    SLATE_ARGUMENT(UOpenGolfOnlineSubsystem*, Online)
    SLATE_ARGUMENT(FOpenGolfAccountStyle, Style)
    /** The preferences being edited, read every frame so Discard and Reset All show at once. */
    SLATE_ARGUMENT(TFunction<const GolfSettings::FPreferences&()>, Preferences)
    SLATE_EVENT(FOnPicked, OnPicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    /** Each tile's diameter, in Slate units. */
    static constexpr float TileSize = 48.0f;

private:
    TSharedRef<SWidget> MakeTile(const FString& Id, const FString& Name, TAttribute<class UTexture2D*> Texture);
    /** Whether the course shows Id (empty: the account picture) for the edited preferences. */
    bool IsShown(const FString& Id) const;
    /** The name of the avatar the course would show. */
    FText ShownName() const;

    TWeakObjectPtr<UOpenGolfOnlineSubsystem> Online;
    FOpenGolfAccountStyle Style;
    TFunction<const GolfSettings::FPreferences&()> Preferences;
    FOnPicked OnPicked;
    FButtonStyle TileStyle;
    FSlateRoundedBoxBrush Ring{FLinearColor::Transparent};
};
