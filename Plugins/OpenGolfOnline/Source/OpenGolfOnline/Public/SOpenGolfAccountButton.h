// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "SOpenGolfIconButton.h"
class UOpenGolfOnlineSubsystem;

/**
 * The header's account control. By default the sign-in icon, shown only while signed out (disabled when the service
 * is not configured) and tagged AccountSignIn; with SignOut the logout icon, shown only while signed in and tagged
 * AccountSignOut. Each sets its own visibility, so a host places both side by side and one shows at a time. The host
 * decides what a click does.
 */
class OPENGOLFONLINE_API SOpenGolfAccountButton : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SOpenGolfAccountButton) : _Online(nullptr), _SignOut(false) {}
    SLATE_ARGUMENT(UOpenGolfOnlineSubsystem*, Online)
    SLATE_ARGUMENT(bool, SignOut)
    SLATE_ARGUMENT(FOpenGolfAccountStyle, Style)
    SLATE_EVENT(FSimpleDelegate, OnClicked)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    EOpenGolfAccountIcon GetIcon() const;
private:
    FText ActionText() const;
    /** Shown while the session state matches this control: signed in for SignOut, signed out otherwise. */
    EVisibility GetButtonVisibility() const;
    TWeakObjectPtr<UOpenGolfOnlineSubsystem> Online;
    bool bSignOut = false;
};
