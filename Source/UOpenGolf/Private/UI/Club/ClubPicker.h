// SPDX-License-Identifier: MIT
#pragma once
#include "Animation/CurveSequence.h"
#include "Widgets/SCompoundWidget.h"
#include "LaunchMonitorTypes.h"
#include "Styling/SlateTypes.h"
class UOpenLaunchMonitorSubsystem;

/**
 * The toolbar's club selector: the club icon with the selected club's short label beside it ("7i"), and a scrollable drawer
 * beneath it. The button is as wide as the widest label, so the toolbar never shifts when the club changes.
 */
class SClubPicker : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SClubPicker) : _Monitor(nullptr) {} SLATE_ARGUMENT(UOpenLaunchMonitorSubsystem*, Monitor) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
private:
    TSharedPtr<class SComboButton> Picker;
    FCurveSequence DrawerSlide{0.0f, 0.14f, ECurveEaseFunction::QuadOut};
    FSlateBrush DrawerBrush;
    FComboButtonStyle ComboStyle;
    FButtonStyle RowStyle;
    FButtonStyle ToggleStyle;
    TWeakObjectPtr<UOpenLaunchMonitorSubsystem> Monitor;
};
