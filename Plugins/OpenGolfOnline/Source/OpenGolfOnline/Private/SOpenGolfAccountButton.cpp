// SPDX-License-Identifier: MIT
#include "SOpenGolfAccountButton.h"
#include "OpenGolfOnlineSubsystem.h"

EOpenGolfAccountIcon SOpenGolfAccountButton::GetIcon() const
{
    return bSignOut?EOpenGolfAccountIcon::SignOut:EOpenGolfAccountIcon::SignIn;
}
FText SOpenGolfAccountButton::ActionText() const
{
    return FText::FromString(bSignOut?TEXT("Sign out"):TEXT("Sign in"));
}
EVisibility SOpenGolfAccountButton::GetButtonVisibility() const
{
    const bool bSignedIn=Online.IsValid()&&Online->HasSession();
    return bSignedIn==bSignOut?EVisibility::Visible:EVisibility::Collapsed;
}
void SOpenGolfAccountButton::Construct(const FArguments& Args)
{
    Online=Args._Online;bSignOut=Args._SignOut;
    // Only this outer widget carries the tag, so a search by tag finds one control.
    SetTag(bSignOut?TEXT("AccountSignOut"):TEXT("AccountSignIn"));
    SetVisibility(TAttribute<EVisibility>::CreateSP(this,&SOpenGolfAccountButton::GetButtonVisibility));
    ChildSlot[SNew(SOpenGolfIconButton).Style(Args._Style)
        .Icon(GetIcon())
        .Label(ActionText())
        .IsEnabled_Lambda([this]{return Online.IsValid()&&Online->IsConfigured();})
        .OnClicked(Args._OnClicked)];
}
