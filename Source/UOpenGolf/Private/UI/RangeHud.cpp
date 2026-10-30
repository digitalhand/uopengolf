// SPDX-License-Identifier: MIT
#include "UI/RangeHud.h"
#include "RangePerformance.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UI/HudLayers.h"
#include "UI/HudScale.h"
#include "UI/HudStyle.h"
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
#include "UI/WindDisplay.h"
#include "Widgets/SNullWidget.h"

namespace
{
    // Slate units before the readable-DPI scale.
    constexpr float HudTileWidth = 112.0f;
    constexpr float HudTileHeight = 48.0f;
    constexpr float HudTileGap = 2.0f;
    constexpr float HudGroupGap = 6.0f;
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

    FLinearColor HudStateColor(ELaunchMonitorConnectionState State)
    {
        switch (State)
        {
        case ELaunchMonitorConnectionState::Ready:
            return FLinearColor(0.3f, 1.0f, 0.4f);
        case ELaunchMonitorConnectionState::Disconnected:
            return FLinearColor(0.65f, 0.65f, 0.65f);
        default:
            return FLinearColor(1.0f, 0.85f, 0.3f);
        }
    }
} // namespace

void SRangeHud::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    WindIndicator = Args._WindIndicator;
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SRangeHud::GetHudVisibility));
    Refresh(true);

    // Preserve Slate's layout syntax, which clang-format otherwise collapses.
    // clang-format off
    const TSharedRef<SVerticalBox> Column = SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(HudTileGap)
        [
            SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(8, 4))
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
                [
                    SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)).ColorAndOpacity(FLinearColor::White)
                    .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                    .Text_Lambda([this] { return FText::FromString(Heading); })
                ]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(8, 0, 0, 0)
                [
                    SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 10)).ColorAndOpacity(HudLabelColor())
                    .Text_Lambda([this] { return FText::FromString(ClubName); })
                ]
            ]
        ];
    // clang-format on

    // One two-column grid per group, in the order BuildTiles returns them.
    for (int32 Index = 0; Index < Tiles.Num();)
    {
        const int32 First = Index;
        const TSharedRef<SUniformGridPanel> Grid = SNew(SUniformGridPanel).SlotPadding(FMargin(HudTileGap));
        for (int32 Cell = 0; Index < Tiles.Num() && Tiles[Index].Group == Tiles[First].Group; ++Cell, ++Index)
        {
            Grid->AddSlot(Cell % 2, Cell / 2)[MakeTile(Index)];
        }
        Column->AddSlot().AutoHeight().Padding(0.0f, First == 0 ? 0.0f : HudGroupGap, 0.0f, 0.0f)[Grid];
    }

    // clang-format off
    ChildSlot
    [
        SNew(SDPIScaler).DPIScale_Lambda([this] { return HudScale::Readable(Controller.IsValid() ? Controller->GetWorld() : nullptr); })
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                Args._WindContent.IsValid() ? Args._WindContent.ToSharedRef() : SNullWidget::NullWidget
            ]
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(HudEdge, HudEdge, HudEdge, HudStatusReserve)
            [
                // Shrinks, never grows, to fit short viewports.
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Right).VAlign(VAlign_Top)
                [ Column ]
            ]
            // Bottom padding matches the Test shots button in the opposite corner.
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(HudEdge, 0, HudEdge, HudScale::BottomPanelGap)
            [
                SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(12, 4))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        MakeStatusCell(TAttribute<FText>::CreateLambda([this] { return ConnectionText; }),
                            TAttribute<FSlateColor>::CreateLambda([this] { return FSlateColor(ConnectionColor); }), TEXT("Launch Monitor"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 0, 0)
                    [
                        MakeStatusCell(TAttribute<FText>::CreateLambda([this] { return FText::FromString(BatteryText); }),
                            FSlateColor(FLinearColor::White), TEXT("Battery"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 0, 0)
                    [
                        MakeStatusCell(TAttribute<FText>::CreateLambda([this] { return FText::FromString(AimText); }),
                            FSlateColor(FLinearColor::White), TEXT("Aim [A/D · Shift fine]"))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(16, 0, 0, 0)
                    [
                        SNew(SBox).Visibility_Lambda([this] { return bShowRecord ? EVisibility::Visible : EVisibility::Collapsed; })
                        [
                            MakeStatusCell(TAttribute<FText>::CreateLambda([this] { return FText::FromString(RecordText); }),
                                TAttribute<FSlateColor>::CreateLambda([this] { return FSlateColor(RecordColor); }), TEXT("Record Shots"))
                        ]
                    ]
                    + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(16, 0, 0, 0)
                    [
                        SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(HudLabelColor())
                        .Text_Lambda([this] { return FText::FromString(KeyTip); })
                    ]
                ]
            ]
            // Game version in the bottom-left corner, under the Test shots button.
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(HudEdge, 0, 0, HudVersionBottom)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(HudLabelColor())
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                .Text(FText::FromString(ShotDisplay::VersionLabel(ShotDisplay::ProjectVersion())))
            ]
        ]
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
    Heading = ShotDisplay::Heading(bHasResult ? &Result : nullptr);

    const ELaunchMonitorConnectionState State = LaunchMonitor ? LaunchMonitor->GetConnectionState() : ELaunchMonitorConnectionState::Disconnected;
    const int32 Battery = LaunchMonitor ? LaunchMonitor->GetBatteryPercent() : -1;
    ClubName = LaunchMonitor ? UOpenLaunchMonitorSubsystem::GetClubDisplayName(LaunchMonitor->GetClub()).ToString() : FString();
    ConnectionText = StaticEnum<ELaunchMonitorConnectionState>()->GetDisplayNameTextByValue(static_cast<int64>(State));
    ConnectionColor = HudStateColor(State);
    BatteryText = Battery < 0 ? FString(TEXT("—")) : FString::Printf(TEXT("%d%%"), Battery);
    // Off the target line; on the hole, also how far wide of the cup the line passes.
    const bool bHole = Mode && Mode->GetPlayMode() == EOpenGolfPlayMode::Hole;
    AimText = OpenGolfPlay::AimOffsetLabel(PlayerController ? PlayerController->GetAimOffsetDeg() : 0.0f, bHole ? Mode->GetDistanceToCupM() : 0.0f,
        bHole && PlayerController->IsShortGameView());
    const bool bRecording = Mode && Mode->IsRecordingShots();
    const int32 Failed = Mode ? Mode->GetFailedRecordingCount() : 0;
    bShowRecord = State != ELaunchMonitorConnectionState::Disconnected || Failed > 0;
    RecordText = ShotDisplay::RecordingStatus(bRecording, Mode ? Mode->GetRecordedShotCount() : 0);
    RecordColor = bRecording ? FLinearColor(1.0f, 0.25f, 0.2f) : FLinearColor(0.65f, 0.65f, 0.65f);
    if (Failed > 0)
    {
        RecordText += FString::Printf(TEXT(" | %d unsaved"), Failed);
        RecordColor = FLinearColor(1.0f, 0.6f, 0.15f);
    }
    KeyTip = ShotDisplay::StatusTip(State, LaunchMonitor ? LaunchMonitor->GetLastError() : FString(), Failed,
        Failed > 0 ? Mode->GetRecordingError() : FString());
}

EVisibility SRangeHud::GetHudVisibility() const
{
    // Hidden behind the open test-shot panel, as the Canvas HUD was, and by the showhud command.
    return HudLayers::IsShown(Controller.Get()) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
}

TSharedRef<SWidget> SRangeHud::MakeTile(int32 Index)
{
    const bool bLaunch = Tiles[Index].Group == ShotDisplay::ETileGroup::Launch;
    // clang-format off
    return SNew(SBox).WidthOverride(HudTileWidth).HeightOverride(HudTileHeight)
    [
        SNew(SBorder).BorderImage(bLaunch ? &LaunchBrush : &TileBrush).Padding(FMargin(6, 2)).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                // Long text such as "Soft Fairway" shrinks to fit the tile.
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                [
                    SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18)).ColorAndOpacity(FLinearColor::White)
                    .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor)
                    .Text_Lambda([this, Index] { return FText::FromString(Tiles.IsValidIndex(Index) ? Tiles[Index].Value : FString()); })
                ]
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(HudLabelColor())
                .Text(FText::FromString(Tiles[Index].Label))
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
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).ColorAndOpacity(Color)
            .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudShadowColor).Text(Value)
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(HudLabelColor())
            .Text(FText::FromString(Label))
        ];
    // clang-format on
}
