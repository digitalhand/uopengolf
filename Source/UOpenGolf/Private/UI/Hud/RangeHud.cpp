// SPDX-License-Identifier: MIT
#include "UI/Hud/RangeHud.h"
#include "UI/Hud/BallReadyGlow.h"
#include "UI/Common/HudStatusGlyph.h"
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
#include "UOpenGolfRange.h"
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
    /** The tile grid's width: the column never grows past it, whatever the heading says. */
    constexpr float HudColumnWidth = 2.0f * (HudTileWidth + 2.0f * HudTileGap);
    constexpr float HudEdge = HudStyle::Edge;
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
    CellStyle = HudStyle::ToolbarButton();
    Refresh(true);

    // The menu button takes clicks; the rest of this layer passes them to the game.
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
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(HudEdge)
            [
                // Shrinks, never grows, to fit short viewports.
                SNew(SScaleBox).Visibility(EVisibility::SelfHitTestInvisible).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Right).VAlign(VAlign_Top)
                [
                    SNew(SBox).Visibility(EVisibility::SelfHitTestInvisible).WidthOverride(HudColumnWidth)
                    [ MakeColumn() ]
                ]
            ]
            // Game version in the bottom-left corner, under the Test shots button.
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(HudEdge, 0, 0, HudVersionBottom)
            [
                SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Font(HudStyle::Font("Regular", 8)).ColorAndOpacity(HudLabelColor())
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                .Text(FText::FromString(ShotDisplay::VersionLabel(ShotDisplay::ProjectVersion())))
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
                SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible)
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible)
                    + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                    [
                        SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Font(HudStyle::Font("Bold", 12))
                        .ColorAndOpacity(HudStyle::TextColor()).OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                        .Text_Lambda([this] { return Heading; })
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
                    [
                        SNew(SButton).ButtonStyle(&CellStyle).ContentPadding(FMargin(8, 6)).Tag(TEXT("HudSettings"))
#if WITH_ACCESSIBILITY
                        .AccessibleText(FText::FromString(TEXT("Open settings")))
#endif
                        .OnClicked_Lambda([this]
                        {
                            if (Controller.IsValid()) Controller->OpenGeneralSettings();
                            return FReply::Handled();
                        })
                        [SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Settings)]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 2)
                [
                    SNew(STextBlock).Font(HudStyle::Font("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor())
                    .Visibility_Lambda([this] { return HoleDetails.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                    .Text_Lambda([this] { return HoleDetails; })
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
    bBallReady = LaunchMonitor && LaunchMonitor->IsBallReady();
    const bool bHole = Mode && Mode->GetPlayMode() == EOpenGolfPlayMode::Hole;
    const bool bBallPlacement = bHole && (PlayerController->IsPlacingBall() || Mode->GetHoleRound().bPractice);
    // Fixed headings are shared texts; the hole details are rebuilt only when they change, as the tiles are.
    static const FText TrainingHeading = FText::FromString(TEXT("Training"));
    static const FText PlacementHeading = FText::FromString(TEXT("Training · Ball Placement"));
    static const FText HoleHeading = FText::FromString(TEXT("Airways"));
    Heading = bBallPlacement ? PlacementHeading : bHole ? HoleHeading : TrainingHeading;
    const FString Details = bHole && !bBallPlacement
        ? ShotDisplay::HoleDetails(Mode->GetHoleRound().Par, GolfSettings::RoundDistance(AUOpenGolfRange::HoleDistanceM))
        : FString();
    if (HoleDetails.ToString() != Details)
        HoleDetails = FText::FromString(Details);
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
    // Total, Carry and Roll (the first three) stand out by color and outline only: every tile has the same size and
    // value font, so each label fits under its value.
    return SNew(SBox).WidthOverride(HudTileWidth).HeightOverride(HudTileHeight).Tag(TEXT("ShotTile"))
    .Clipping(EWidgetClipping::ClipToBounds)
    .Visibility_Lambda([Index]{ const int32 Detail = GolfSettings::Get().ShotData; return Detail == 2 || Index < 3 || (Detail == 1 && (Index < 5 || Index == 12)) ? EVisibility::Visible : EVisibility::Collapsed; })
    [
        SNew(SBorder).BorderImage(Index<3 ? &ResultBrush : bLaunch ? &LaunchBrush : &TileBrush).Padding(FMargin(6, 3))
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().FillHeight(2)
            [
                // Long text such as "Soft Fairway" shrinks to fit the tile.
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Font(HudStyle::Font("Bold", 16)).ColorAndOpacity(Index<3?HudStyle::AccentColor():HudStyle::TextColor())
                    .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                    .Text_Lambda([this, Index] { return TileValues.IsValidIndex(Index) ? TileValues[Index] : FText::GetEmpty(); })
                ]
            ]
            + SVerticalBox::Slot().FillHeight(1)
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Center).VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Font(HudStyle::Font("Regular", 8)).ColorAndOpacity(HudLabelColor())
                    .Text_Lambda([this,Index]{return TileLabels.IsValidIndex(Index)?TileLabels[Index]:FText::GetEmpty();})
                ]
            ]
        ]
    ];
    // clang-format on
}
