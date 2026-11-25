// SPDX-License-Identifier: MIT
#include "UI/Hud/PlayModeBar.h"
#include "UI/Hud/TrainingControls.h"
#include "Training/TrainingSession.h"
#include "UI/Common/HudStyle.h"
#include "UI/Hud/ShotDisplay.h"
#include "UI/Club/ClubPicker.h"
#include "UI/Hud/PlayerBadge.h"
#include "UI/Common/HudStatusGlyph.h"
#include "UI/Common/HudToolTip.h"
#include "Engine/GameInstance.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Widgets/Input/SButton.h"

#include "Engine/World.h"
#include "UI/Common/HudLayers.h"
#include "UI/Common/HudScale.h"
#include "OpenFairwayBall.h"
#include "Play/OpenGolfPlayRules.h"
#include "Styling/CoreStyle.h"
#include "UOpenGolfGameMode.h"
#include "GolfCourseDefinition.h"
#include "UOpenGolfPlayerController.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    constexpr float TileWidth = 84.0f;
    constexpr float TileHeight = 48.0f;
    constexpr float TileGap = 2.0f;
    /** The hole-out banner fades over this last part of the wait before the new hole. */
    constexpr float BannerFadeSeconds = 1.0f;
} // namespace

void SPlayModeBar::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    SetTag(TEXT("PlayControls"));
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SPlayModeBar::GetBarVisibility));
    // The game instance outlives the HUD, so the online service is looked up once rather than every frame.
    const UGameInstance* Instance = Controller.IsValid() ? Controller->GetGameInstance() : nullptr;
    UOpenLaunchMonitorSubsystem* LaunchMonitor = Instance ? Instance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;
    Online = Instance ? Instance->GetSubsystem<UOpenGolfOnlineSubsystem>() : nullptr;
    Refresh();
    RecordStyle = HudStyle::ToolbarButton();
    auto HoleOnly = [this] { return bHoleMode ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; };
    // clang-format off
    ChildSlot
    [
        SNew(SDPIScaler).DPIScale_Lambda([this] { return HudScale::Readable(Controller.IsValid() ? Controller->GetWorld() : nullptr); })
        [
            SNew(SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(HudStyle::Edge)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(3)).Visibility(EVisibility::SelfHitTestInvisible)
                    [
                        SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible).Tag(TEXT("PlayToolbar"))
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SHudIconButton).Icon(SHudIconButton::EIcon::Close).Tag(TEXT("TrainingBack"))
                            .Visibility_Lambda([this] { return bRangeMode ? EVisibility::Visible : EVisibility::Collapsed; })
                            .ToolTip(HudToolTip::Make(FText::FromString(TEXT("Back (Esc)"))))
#if WITH_ACCESSIBILITY
                            .AccessibleText(FText::FromString(TEXT("Back")))
#endif
                            .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->HandleEscape(); })
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SAssignNew(TrainingPicker, STrainingPicker).Player(Controller).Tag(TEXT("TrainingGames"))
                            .Visibility_Lambda([this] { return bRangeMode ? EVisibility::Visible : EVisibility::Collapsed; })
                        ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SClubPicker).Tag(TEXT("ClubPicker")).Monitor(LaunchMonitor)
                        ]
                        // The signed-in player, on the course only; nothing shows while signed out.
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SPlayerBadge).Online(Online)
                            .Visibility_Lambda([this] { return bHoleMode && bSignedIn ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                        ]
                        + SHorizontalBox::Slot().AutoWidth()[ MakeRecordButton() ]
                        + SHorizontalBox::Slot().AutoWidth()
                        [
                            SNew(SHudIconButton).Icon(SHudIconButton::EIcon::Ball).Tag(TEXT("PlaceBall"))
                            .Visibility_Lambda([this] { return bRangeMode ? EVisibility::Visible : EVisibility::Collapsed; })
                            .IsEnabled_Lambda([this] { return bCanPlace; })
                            .IsActive_Lambda([this] { return bPlacing; })
                            .ToolTip(HudToolTip::Make(FText::FromString(TEXT("Place ball (B)"))))
#if WITH_ACCESSIBILITY
                            .AccessibleText(FText::FromString(TEXT("Place ball")))
#endif
                            .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->TogglePlacingBall(); })
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SAssignNew(DrillSettings, SBox).Padding(FMargin(0, 5, 0, 0))
                    .Visibility_Lambda([this] {
                        const bool bPickerOpen = TrainingPicker.IsValid() && TrainingPicker->IsOpen();
                        return DisplayedDrill.IsValid() && !bPickerOpen ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
                    })
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(TileGap, 6, TileGap, 0)
                [
                    SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(8, 4)).Visibility_Lambda([this]
                        { return bPlacing ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                    [
                        SNew(STextBlock).Font(HudStyle::Font("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor())
                        .Text(FText::FromString(TEXT("Click the ground to place the ball  ·  Right-click to cancel")))
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                [
                    SNew(SBox).Visibility_Lambda(HoleOnly)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox).Tag(TEXT("HoleTiles"))
                            + SHorizontalBox::Slot().AutoWidth().Padding(TileGap)[ MakeTile(TAttribute<FText>::CreateLambda([this] { return StrokeText; }), TEXT("Stroke")) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(TileGap)[ MakeTile(TAttribute<FText>::CreateLambda([this] { return ToPinText; }), TEXT("To Pin")) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(TileGap)[ MakeTile(TAttribute<FText>::CreateLambda([this] { return PuttsText; }), TEXT("Putts")) ]
                        ]
                        // A putt that crossed the cup too fast to drop says so, with the speed that would have.
                        + SVerticalBox::Slot().AutoHeight().Padding(TileGap, 6, TileGap, 0)
                        [
                            SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(8, 4)).Visibility_Lambda([this]
                                { return StatusText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                            [
                                SNew(STextBlock).Font(HudStyle::Font("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor())
                                .Text_Lambda([this] { return StatusText; })
                            ]
                        ]
                    ]
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0, 0, 0, 160)
            [
                SNew(SBorder).BorderImage(&BannerBrush).Padding(FMargin(28, 12))
                .Visibility_Lambda([this] { return BannerOpacity > 0.0f ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                .ColorAndOpacity_Lambda([this] { return FLinearColor(1, 1, 1, BannerOpacity); })
                .BorderBackgroundColor_Lambda([this] { return FSlateColor(FLinearColor(1, 1, 1, BannerOpacity)); })
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        SNew(STextBlock).Font(HudStyle::Font("Bold", 30)).ColorAndOpacity(HudStyle::TextColor())
                        .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor)
                        .Text_Lambda([this] { return BannerTitle; })
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
                    [
                        SNew(STextBlock).Font(HudStyle::Font("Regular", 11)).ColorAndOpacity(HudStyle::LabelColor())
                        .Text_Lambda([this] { return BannerDetail; })
                    ]
                ]
            ]
        ]
    ];
    // clang-format on
}

TSharedRef<SWidget> SPlayModeBar::MakeRecordButton()
{
    auto Color = [this] { return HudStyle::RecordColor(bRecording, FailedCount); };
    // Bound weakly: the status can be read for a moment after a HUD rebuild has released this widget. Only screen
    // readers get the status; the tooltip names the button.
    const TAttribute<FText> Tip = TAttribute<FText>::CreateSP(this, &SPlayModeBar::GetRecordTip);
    return SNew(SButton).ButtonStyle(&RecordStyle).ContentPadding(FMargin(0)).Tag(TEXT("RecordToggle"))
        .Visibility_Lambda([this] { return bRangeMode ? EVisibility::Visible : EVisibility::Collapsed; })
        .ToolTip(HudToolTip::Make(NSLOCTEXT("PlayModeBar", "RecordShots", "Record Shots")))
#if WITH_ACCESSIBILITY
        .AccessibleText(Tip)
#endif
        .OnClicked_Lambda([this]
        {
            if (Controller.IsValid()) Controller->ToggleRecordShots();
            Refresh();
            return FReply::Handled();
        })
        [
            SNew(SBox).WidthOverride(HudStyle::ToolbarButtonSize).HeightOverride(HudStyle::ToolbarButtonSize)
            .HAlign(HAlign_Center).VAlign(VAlign_Center)
            [
                SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Record)
                .Color_Lambda(Color).Filled_Lambda([this] { return bRecording; })
            ]
        ];
}

FText SPlayModeBar::GetRecordTip() const
{
    FString Text = ShotDisplay::RecordTip(bRecording, bSignedIn);
    if (bRecording)
        Text += FString::Printf(TEXT("\n%d saved"), RecordedCount);
    if (FailedCount > 0)
    {
        Text += FString::Printf(TEXT("\n%d unsaved"), FailedCount);
        const UWorld* World = Controller.IsValid() ? Controller->GetWorld() : nullptr;
        if (const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr)
            Text += TEXT("\n") + Mode->GetRecordingError() + TEXT(" - retrying");
    }
    return FText::FromString(Text);
}

TSharedRef<SWidget> SPlayModeBar::MakeTile(TAttribute<FText> Value, const TCHAR* Label)
{
    // clang-format off
    return SNew(SBox).WidthOverride(TileWidth).HeightOverride(TileHeight)
    [
        SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(6, 2)).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(STextBlock).Font(HudStyle::ValueFont()).ColorAndOpacity(HudStyle::TextColor())
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor).Text(Value)
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(STextBlock).Font(HudStyle::Font("Regular", 8)).ColorAndOpacity(HudStyle::LabelColor()).Text(FText::FromString(Label))
            ]
        ]
    ];
    // clang-format on
}

void SPlayModeBar::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    Refresh();
}

void SPlayModeBar::Refresh()
{
    const AUOpenGolfPlayerController* PlayerController = Controller.Get();
    const UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    bHoleMode = Mode && Mode->GetPlayMode() == EOpenGolfPlayMode::Hole;
    bPlacing = PlayerController && PlayerController->IsPlacingBall();
    BannerOpacity = 0.0f;
    StatusText = FText::GetEmpty();
    bRangeMode = Mode && !bHoleMode;
    bCanPlace = Mode && Mode->CanPlaceTrainingBall();
    UTrainingDrill* Drill = bRangeMode && Mode->GetTrainingSession() ? Mode->GetTrainingSession()->Active() : nullptr;
    if (DrillSettings && (DisplayedDrill.Get() != Drill || DisplayedDrill.IsStale()))
    {
        DisplayedDrill = Drill;
        if (!Drill) DrillSettings->SetContent(SNullWidget::NullWidget);
        else DrillSettings->SetContent(SNew(SBorder).BorderImage(&TileBrush).Padding(8)[
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(HudStyle::Font("Bold", 12)).Text(Drill->Name())]
            // Each drill disables its own controls in flight, so its score stays readable.
            + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 0)[Drill->MakeSettings()]
        ]);
    }
    bRecording = Mode && Mode->IsRecordingShots();
    RecordedCount = Mode ? Mode->GetRecordedShotCount() : 0;
    FailedCount = Mode ? Mode->GetFailedRecordingCount() : 0;
    const UOpenGolfOnlineSubsystem* OnlineService = Online.Get();
    bSignedIn = OnlineService && OnlineService->HasSession();
    if (!bHoleMode || !Ball)
        return;

    const FOpenGolfHoleRound& Round = Mode->GetHoleRound();
    const FOpenFairwayShotResult& Result = Ball->GetResult();
    // The stroke about to be played, or the final count once holed.
    StrokeText = FText::AsNumber(Round.bHoled ? Round.Strokes : Round.Strokes + 1);
    ToPinText = FText::FromString(Ball->IsHoled() ? FString(TEXT("In")) : GolfSettings::RoundDistance(Mode->GetDistanceToCupM(), Result.Surface == EOpenFairwaySurface::Green));
    PuttsText = FText::AsNumber(Round.Putts);
    if (Result.bOverCup && !Round.bHoled && !Ball->IsHoled())
        StatusText = FText::FromString(FString::Printf(TEXT("Over the cup at %s  ·  drops under %s"), *GolfSettings::Speed(Result.OverCupSpeedMps),
            *GolfSettings::Speed(Result.OverCupMaxSpeedMps)));
    switch (Mode->GetRecentPenalty())
    {
    case EGolfPenalty::Water: StatusText = FText::FromString(TEXT("In the water  ·  penalty stroke  ·  replay from the last spot")); break;
    case EGolfPenalty::OutOfBounds: StatusText = FText::FromString(TEXT("Out of bounds  ·  penalty stroke  ·  replay from the last spot")); break;
    default: break;
    }

    const float HoledSeconds = Mode->GetHoledSeconds();
    if (Round.bHoled && HoledSeconds >= 0.0f)
    {
        BannerOpacity = FMath::Clamp((AUOpenGolfGameMode::HoleOutDelayS - HoledSeconds) / BannerFadeSeconds, 0.0f, 1.0f);
        BannerTitle = FText::FromString(Round.ResultName() + (Round.bConceded ? TEXT(" - Conceded") : TEXT("")));
        BannerDetail = FText::FromString(FString::Printf(TEXT("%d %s  ·  %d %s"), Round.Strokes, Round.Strokes == 1 ? TEXT("stroke") : TEXT("strokes"),
            Round.Putts, Round.Putts == 1 ? TEXT("putt") : TEXT("putts")));
    }
}

EVisibility SPlayModeBar::GetBarVisibility() const
{
    // Only the buttons take clicks; the rest of the screen still reaches the game.
    return HudLayers::IsShown(Controller.Get()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}
