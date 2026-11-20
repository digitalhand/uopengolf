// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/HudIconButton.h"
#include "Widgets/SCompoundWidget.h"

class AUOpenGolfPlayerController;
class UOpenGolfOnlineSubsystem;

/**
 * Top-left club selector, Training recording and placement controls, or course stroke and putt tiles.
 * The hole-out banner remains visible until the final scorecard opens.
 */
class SPlayModeBar : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPlayModeBar) {}
    SLATE_ARGUMENT(TWeakObjectPtr<AUOpenGolfPlayerController>, Controller)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
    void Refresh();
    EVisibility GetBarVisibility() const;
    TSharedRef<SWidget> MakeRecordButton();
    TSharedRef<SWidget> MakeTile(TAttribute<FText> Value, const TCHAR* Label);
    /** Where recorded shots go, the saved and unsaved counts, and the save error while retrying. */
    FText GetRecordTip() const;

    TWeakObjectPtr<AUOpenGolfPlayerController> Controller;
    TWeakObjectPtr<UOpenGolfOnlineSubsystem> Online;
    bool bHoleMode = false;
    bool bPlacing = false;
    bool bRangeMode = false;
    bool bRecording = false;
    bool bSignedIn = false;
    int32 RecordedCount = 0;
    int32 FailedCount = 0;
    FText StrokeText;
    FText ToPinText;
    FText PuttsText;
    FText OverCupText;
    FText BannerTitle;
    FText BannerDetail;
    float BannerOpacity = 0.0f;

    FButtonStyle RecordStyle;
    FSlateRoundedBoxBrush TileBrush{HudStyle::TileColor(), HudStyle::TileRadius};
    FSlateRoundedBoxBrush BannerBrush{HudStyle::LaunchTileColor(), 8.0f};
};
