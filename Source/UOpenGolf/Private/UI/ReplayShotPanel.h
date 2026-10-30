// SPDX-License-Identifier: MIT
#pragma once

#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
template <typename OptionType>
class SComboBox;

/** Local range controls; presets still enter through the launch-monitor boundary. */
class SReplayShotPanel : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SReplayShotPanel) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
    FText SelectedName() const;
    bool IsLaunchMonitorConnected() const;
    FReply HitShot();
    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TArray<TSharedPtr<FString>> Options;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ShotList;
};
