// SPDX-License-Identifier: MIT
#pragma once

#include "Widgets/SCompoundWidget.h"

class UOpenGolfOnlineSubsystem;

/**
 * The signed-in player beside the club button on the course: their avatar (a picked built-in avatar, else the account
 * picture, else the default avatar) and display name, in the course tiles' number font. The host decides when it shows.
 */
class SPlayerBadge : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPlayerBadge) {}
    SLATE_ARGUMENT(TWeakObjectPtr<UOpenGolfOnlineSubsystem>, Online)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    static constexpr float AvatarSize = 36.0f;
    /** A longer name ends in an ellipsis, so the toolbar stays compact. */
    static constexpr float MaxNameWidth = 220.0f;

private:
    TWeakObjectPtr<UOpenGolfOnlineSubsystem> Online;
};
