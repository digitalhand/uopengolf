// SPDX-License-Identifier: MIT
#include "UI/Hud/RangeHud.h"
#include "UI/Hud/BallReadyGlow.h"
#include "UI/Club/ClubPicker.h"
#include "UI/Common/HudStatusGlyph.h"
#include "OpenGolfOnlineSubsystem.h"
#include "Widgets/Input/SButton.h"
#include "RangePerformance.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/Common/HudLayers.h"
#include "UI/Common/HudScale.h"
#include "UI/Common/HudStyle.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
#include "Play/OpenGolfPlayRules.h"
#include "Styling/CoreStyle.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "OpenGolfWindIndicator.h"
#include "UI/Wind/WindDisplay.h"
#include "Widgets/SNullWidget.h"

namespace
{
    // Slate units before the readable-DPI scale.
    constexpr float HudTileWidth = 112.0f;
    constexpr float HudTileHeight = 48.0f;
    constexpr float HudTileGap = 2.0f;
    constexpr float HudGroupGap = 6.0f;
    /** The tile grid's width: the column never grows past it, whatever the heading or club picker says. */
    constexpr float HudColumnWidth = 2.0f * (HudTileWidth + 2.0f * HudTileGap);
    constexpr float HudEdge = HudStyle::Edge;
    /** Space kept under the tiles so they never cover the status box. */
    constexpr float HudStatusReserve = 56.0f + HudScale::BottomPanelGap;
    /** Version label distance from the bottom edge, inside HudScale::BottomPanelGap. */
    constexpr float HudVersionBottom = 6.0f;
    /** Shot and status text is rebuilt this often; the wind arrow still turns with the camera every frame. */
    constexpr double HudTextRefreshSeconds = 0.1;
    const FLinearColor HudShadowColor = HudStyle::ShadowColor;

    FLinearColor HudLabelColor()
    {
        return HudStyle::LabelColor();
    }

} // namespace

void SRangeHud::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    SetTag(TEXT("TrainingHud"));
    WindIndicator = Args._WindIndicator;
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SRangeHud::GetHudVisibility));
    // Status buttons: no background until hovered, like the tiles when they are.
    const FMargin CellPadding(6, 2);
    CellStyle = FButtonStyle()
        .SetNormal(FSlateNoResource())
        .SetHovered(FSlateRoundedBoxBrush(HudStyle::HoverTileColor(), HudStyle::TileRadius))
        .SetPressed(FSlateRoundedBoxBrush(HudStyle::LaunchTileColor(), HudStyle::TileRadius))
        .SetDisabled(FSlateNoResource())
        .SetNormalPadding(CellPadding)
        .SetPressedPadding(CellPadding);
    Refresh(true);

    // Only the club picker and the status buttons take clicks; everything else passes them to the game (HudLayers).
    // clang-format off
    ChildSlot
    [
        SNew(SDPIScaler).Visibility(EVisibility::SelfHitTestInvisible)
        .DPIScale_Lambda([this] { return HudScale::Readable(Controller.IsValid() ? Controller->GetWorld() : nullptr); })
        [
            SNew(SOverlay).Visibility(EVisibility::SelfHitTestInvisible)
            // Behind everything else: the glow around the ball at address.
            + SOverlay::Slot()
            [
                SNew(SBallReadyGlow).Controller(Controller).Ready_Lambda([this] { return bBallReady; })
            ]
            + SOverlay::Slot()
            [
                SNew(SBox).Visibility(EVisibility::HitTestInvisible)
                [
                    Args._WindContent.IsValid() ? Args._WindContent.ToSharedRef() : SNullWidget::NullWidget
                ]
            ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(HudEdge, HudEdge, HudEdge, HudStatusReserve)
            [
                // Shrinks, never grows, to fit short viewports.
                SNew(SScaleBox).Visibility(EVisibility::SelfHitTestInvisible).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Right).VAlign(VAlign_Top)
                [
                    SNew(SBox).Visibility(EVisibility::SelfHitTestInvisible).WidthOverride(HudColumnWidth)
                    [ MakeColumn() ]
                ]
            ]
            // Bottom padding matches the Test shots button in the opposite corner.
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(HudEdge, 0, HudEdge, HudScale::BottomPanelGap)
            [ MakeStatusBox() ]
            // Game version in the bottom-left corner, under the Test shots button.
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(HudEdge, 0, 0, HudVersionBottom)
            [
                SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Font(HudStyle::Font("Regular", 8)).ColorAndOpacity(HudLabelColor())
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                .Text(FText::FromString(ShotDisplay::VersionLabel(ShotDisplay::ProjectVersion())+TEXT("   ·   HUD [F6 / View]")))
            ]
        ]
    ];
    // clang-format on
}

TSharedRef<SWidget> SRangeHud::MakeColumn()
{
    // clang-format off
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible)
        + SVerticalBox::Slot().AutoHeight().Padding(HudTileGap)
        [
            SNew(SBorder).Visibility(EVisibility::SelfHitTestInvisible).BorderImage(&TileBrush).Padding(FMargin(8, 4)).Tag(TEXT("ShotHeading"))
            [
                SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [
                    // A long heading is cut short rather than widening the column.
                    SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Font(HudStyle::Font("Bold", 10)).ColorAndOpacity(HudStyle::TextColor())
                    .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                    .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                    .Text_Lambda([this] { return FText::FromString(Heading); })
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
                [
                    SNew(SClubPicker).Tag(TEXT("ClubPicker"))
                    .Monitor(Controller.IsValid() && Controller->GetGameInstance() ? Controller->GetGameInstance()->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr)
                ]
            ]
        ];
    // clang-format on

    // One two-column grid per group, in the order BuildTiles returns them.
    for (int32 Index = 0; Index < Tiles.Num();)
    {
        const int32 First = Index;
        const TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).Visibility(EVisibility::HitTestInvisible).SlotPadding(FMargin(HudTileGap));
        for (int32 Cell = 0; Index < Tiles.Num() && Tiles[Index].Group == Tiles[First].Group; ++Cell, ++Index)
        {
            Grid->AddSlot(Cell % 2, Cell / 2)[MakeTile(Index)];
        }
        Column->AddSlot().AutoHeight().Padding(0.0f, First == 0 ? 0.0f : HudGroupGap, 0.0f, 0.0f)[Grid];
    }
    return Column;
}

TSharedRef<SWidget> SRangeHud::MakeStatusBox()
{
    // Aim, then the launch monitor (opens its settings) and recording (toggles, as F9 does), then any error.
    // clang-format off
    return SNew(SBorder).Visibility(EVisibility::SelfHitTestInvisible).BorderImage(&TileBrush).Padding(FMargin(12, 4))
        [
            SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 10, 0)
            [
                SNew(SBox).Visibility(EVisibility::HitTestInvisible)
                [
                    MakeStatusCell(TAttribute<FText>::CreateLambda([this] { return FText::FromString(AimText); }),
                        FSlateColor(FLinearColor::White), TEXT("Aim [A/D · Shift fine]"))
                ]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ MakeLaunchMonitorButton() ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)[ MakeRecordButton() ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(10, 0, 0, 0)
            [
                SNew(STextBlock).Font(HudStyle::Font("Regular", 9)).ColorAndOpacity(HudLabelColor())
                .Visibility_Lambda([this] { return ErrorText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                .Text_Lambda([this] { return FText::FromString(ErrorText); })
            ]
        ];
    // clang-format on
}

TSharedRef<SWidget> SRangeHud::MakeLaunchMonitorButton()
{
    // clang-format off
    const TSharedRef<SWidget> Icons = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::LaunchMonitor)
            .Color_Lambda([this] { return HudStyle::StateColor(ConnectionState); })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(6, 0, 0, 0)
        [
            SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Battery).Color(HudStyle::TextColor())
            .Visibility_Lambda([this] { return BatteryPercent < 0 ? EVisibility::Collapsed : EVisibility::Visible; })
            .Level_Lambda([this] { return BatteryPercent / 100.0f; })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
        [
            SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).ColorAndOpacity(FLinearColor::White)
            .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
            .Text_Lambda([this] { return FText::FromString(ShotDisplay::BatteryLabel(BatteryPercent)); })
        ];
    return SNew(SButton).ButtonStyle(&CellStyle).ContentPadding(FMargin(0)).Tag(TEXT("LaunchMonitorStatus"))
#if WITH_ACCESSIBILITY
        // Read by screen readers only; the game shows no tooltips.
        .AccessibleText_Lambda([this] { return FText::FromString(ShotDisplay::LaunchMonitorTip(ConnectionState, BatteryPercent)); })
#endif
        .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->OpenLaunchMonitorSettings(); return FReply::Handled(); })
        [
            MakeIconCell(Icons, TAttribute<FText>::CreateLambda([this] { return FText::FromString(ShotDisplay::LaunchMonitorCaption(ConnectionState)); }))
        ];
    // clang-format on
}

TSharedRef<SWidget> SRangeHud::MakeRecordButton()
{
    auto Color = [this] { return HudStyle::RecordColor(bRecording, FailedCount); };
    // clang-format off
    const TSharedRef<SWidget> Icons = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Record).Color_Lambda(Color).Filled_Lambda([this] { return bRecording; })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(4, 0, 0, 0)
        [
            SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).ColorAndOpacity_Lambda([Color] { return FSlateColor(Color()); })
            .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
            .Visibility_Lambda([this] { return bRecording || FailedCount > 0 ? EVisibility::Visible : EVisibility::Collapsed; })
            .Text_Lambda([this] { return FText::FromString(ShotDisplay::RecordCount(bRecording, RecordedCount, FailedCount)); })
        ];
    // Recording is a range control: the hole never records, so it offers no toggle.
    return SNew(SButton).ButtonStyle(&CellStyle).ContentPadding(FMargin(0)).Tag(TEXT("RecordToggle"))
        .Visibility_Lambda([this] { return bRangeMode ? EVisibility::Visible : EVisibility::Collapsed; })
#if WITH_ACCESSIBILITY
        .AccessibleText_Lambda([this] { return FText::FromString(ShotDisplay::RecordTip(bRecording, bSignedIn)); })
#endif
        .OnClicked_Lambda([this] { if (Controller.IsValid()) Controller->ToggleRecordShots(); Refresh(true); return FReply::Handled(); })
        [
            MakeIconCell(Icons, FText::FromString(TEXT("Record [F9]")))
        ];
    // clang-format on
}

TSharedRef<SWidget> SRangeHud::MakeIconCell(const TSharedRef<SWidget>& Icons, const TAttribute<FText>& Caption) const
{
    // Laid out as the text cells: the value line, then the caption.
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[ Icons ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            SNew(STextBlock).Font(HudStyle::Font("Regular", 8)).ColorAndOpacity(HudLabelColor()).Text(Caption)
        ];
    // clang-format on
}

void SRangeHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    // Formatting every tile cost about as much as the ball's physics per frame, for text that changes far less often.
    const bool bText = InCurrentTime >= NextTextRefreshTime;
    if (bText)
    {
        NextTextRefreshTime = InCurrentTime + HudTextRefreshSeconds;
    }
    Refresh(bText);
}

void SRangeHud::Refresh(bool bText)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(UOpenGolf_HudRefresh);
    RangePerformance::FScope Timing(RangePerformance::Hud);
    const AUOpenGolfPlayerController* PlayerController = Controller.Get();
    const UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (UOpenGolfWindIndicator* Indicator = WindIndicator.Get())
    {
        Indicator->SetWindState(WindDisplay::Read(Ball, Mode ? Mode->GetRange() : nullptr,
            PlayerController && PlayerController->PlayerCameraManager ? PlayerController->PlayerCameraManager->GetCameraRotation().Yaw : 0));
    }
    if (!bText)
    {
        return;
    }
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    const UOpenLaunchMonitorSubsystem* LaunchMonitor = GameInstance ? GameInstance->GetSubsystem<UOpenLaunchMonitorSubsystem>() : nullptr;

    FOpenFairwayShotResult Result;
    FLaunchMonitorShot Shot;
    FLaunchMonitorClubData Club;
    const bool bHasResult = Ball && Ball->GetShotResult(Result);
    const bool bHasShot = LaunchMonitor && LaunchMonitor->GetLastShot(Shot);
    const bool bHasClub = LaunchMonitor && LaunchMonitor->GetLastClubData(Club);
    Tiles = ShotDisplay::BuildTiles(bHasResult ? &Result : nullptr, bHasShot ? &Shot : nullptr, bHasClub ? &Club : nullptr);
    TileValues.SetNum(Tiles.Num());TileLabels.SetNum(Tiles.Num());
    for(int32 I=0;I<Tiles.Num();++I)
    {
        if(TileValues[I].ToString()!=Tiles[I].Value)TileValues[I]=FText::FromString(Tiles[I].Value);
        if(TileLabels[I].ToString()!=Tiles[I].Label)TileLabels[I]=FText::FromString(Tiles[I].Label);
    }
    Heading = ShotDisplay::Heading(bHasResult ? &Result : nullptr);

    ConnectionState = LaunchMonitor ? LaunchMonitor->GetConnectionState() : ELaunchMonitorConnectionState::Disconnected;
    BatteryPercent = LaunchMonitor ? LaunchMonitor->GetBatteryPercent() : -1;
    bBallReady = LaunchMonitor && LaunchMonitor->IsBallReady();
    // Off the target line; on the hole, also how far wide of the cup the line passes.
    bRangeMode = Mode && Mode->GetPlayMode() == EOpenGolfPlayMode::Range;
    const bool bHole = Mode && Mode->GetPlayMode() == EOpenGolfPlayMode::Hole;
    AimText = OpenGolfPlay::AimOffsetLabel(PlayerController ? PlayerController->GetAimOffsetDeg() : 0.0f, 0.0f,
        bHole && PlayerController->IsShortGameView());
    if (bHole && PlayerController && FMath::Abs(PlayerController->GetAimOffsetDeg()) >= .05f)
        AimText += TEXT(" / ") + GolfSettings::Distance(Mode->GetDistanceToCupM() * FMath::Tan(FMath::DegreesToRadians(FMath::Abs(PlayerController->GetAimOffsetDeg()))), PlayerController->IsShortGameView());
    // Recording saves shots locally and, while signed in, to the online profile too.
    bRecording = Mode && Mode->IsRecordingShots();
    RecordedCount = Mode ? Mode->GetRecordedShotCount() : 0;
    FailedCount = Mode ? Mode->GetFailedRecordingCount() : 0;
    const UOpenGolfOnlineSubsystem* Online = GameInstance ? GameInstance->GetSubsystem<UOpenGolfOnlineSubsystem>() : nullptr;
    bSignedIn = Online && Online->HasSession();
    ErrorText = ShotDisplay::StatusError(LaunchMonitor ? LaunchMonitor->GetLastError() : FString(), FailedCount,
        FailedCount > 0 && Mode ? Mode->GetRecordingError() : FString());
}

EVisibility SRangeHud::GetHudVisibility() const
{
    // Hidden behind the open test-shot panel, as the Canvas HUD was, and by the showhud command.
    return HudLayers::IsShown(Controller.Get()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SRangeHud::MakeTile(int32 Index)
{
    const bool bLaunch = Tiles[Index].Group == ShotDisplay::ETileGroup::Launch;
    // clang-format off
    return SNew(SBox).WidthOverride(HudTileWidth).HeightOverride(Index<3?56.f:HudTileHeight).Tag(TEXT("ShotTile"))
    .Visibility_Lambda([Index]{ const int32 Detail = GolfSettings::Get().ShotData; return Detail == 2 || Index < 3 || (Detail == 1 && (Index < 5 || Index == 12)) ? EVisibility::Visible : EVisibility::Collapsed; })
    [
        SNew(SBorder).BorderImage(Index<3 ? &ResultBrush : bLaunch ? &LaunchBrush : &TileBrush).Padding(FMargin(6, 2)).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                // Long text such as "Soft Fairway" shrinks to fit the tile.
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [
                    SNew(STextBlock).Font(HudStyle::Font("Bold", Index<3?22:16)).ColorAndOpacity(Index<3?HudStyle::AccentColor():HudStyle::TextColor())
                    .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                    .Text_Lambda([this, Index] { return TileValues.IsValidIndex(Index) ? TileValues[Index] : FText::GetEmpty(); })
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(STextBlock).Font(HudStyle::Font("Regular", 8)).ColorAndOpacity(HudLabelColor())
                .Text_Lambda([this,Index]{return TileLabels.IsValidIndex(Index)?TileLabels[Index]:FText::GetEmpty();})
            ]
        ]
    ];
    // clang-format on
}

TSharedRef<SWidget> SRangeHud::MakeStatusCell(TAttribute<FText> Value, TAttribute<FSlateColor> Color, const TCHAR* Label) const
{
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            SNew(STextBlock).Font(HudStyle::Font("Bold", 11)).ColorAndOpacity(Color)
            .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor).Text(Value)
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            SNew(STextBlock).Font(HudStyle::Font("Regular", 8)).ColorAndOpacity(HudLabelColor())
            .Text(FText::FromString(Label))
        ];
    // clang-format on
}
