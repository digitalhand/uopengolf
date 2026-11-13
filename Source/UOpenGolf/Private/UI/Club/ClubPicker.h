// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "OpenGolfAccountStyle.h"
#include "LaunchMonitorTypes.h"
class UOpenLaunchMonitorSubsystem;

/**
 * The range HUD's club choice: the club face and its standard abbreviation (PW, 7i), opening a list of every club.
 * Its size never changes with the club, so the HUD column beside it stays put.
 */
class SClubPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClubPicker) : _Monitor(nullptr) {} SLATE_ARGUMENT(UOpenLaunchMonitorSubsystem*, Monitor) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
private:
    TSharedPtr<class SComboButton> Picker;
    FOpenGolfAccountStyle Style;
    TWeakObjectPtr<UOpenLaunchMonitorSubsystem> Monitor;

};
