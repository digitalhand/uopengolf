// SPDX-License-Identifier: MIT
#include "RangeHud.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/HUD.h"
#include "HudScale.h"
#include "OpenFairwayBall.h"
#include "OpenLaunchMonitorSubsystem.h"
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

namespace
{
    // Slate units before the readable-DPI scale.
    constexpr float HudTileWidth = 112.0f;
    constexpr float HudTileHeight = 48.0f;
    constexpr float HudTileGap = 2.0f;
    constexpr float HudGroupGap = 6.0f;
    constexpr float HudEdge = 20.0f;
    /** Space kept under the tiles so they never cover the status box. */
    constexpr float HudStatusReserve = 64.0f;
    const FLinearColor HudShadowColor(0.0f, 0.0f, 0.0f, 0.6f);

    FLinearColor HudLabelColor()
    {
        return FLinearColor::FromSRGBColor(FColor(200, 200, 204));
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
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SRangeHud::GetHudVisibility));
    Refresh();

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
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(HudEdge, HudEdge, HudEdge, HudStatusReserve)
            [
                // Shrinks, never grows, to fit short viewports.
                SNew(SScaleBox).Stretch(EStretch::ScaleToFit).StretchDirection(EStretchDirection::DownOnly)
                .HAlign(HAlign_Right).VAlign(VAlign_Top)
                [ Column ]
            ]
            // Bottom padding matches the Test shots button in the opposite corner.
            + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(HudEdge, 0, HudEdge, 8)
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
        ]
    ];
    // clang-format on
}

void SRangeHud::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    Refresh();
}

void SRangeHud::Refresh()
{
    const AUOpenGolfPlayerController* PlayerController = Controller.Get();
    const UWorld* World = PlayerController ? PlayerController->GetWorld() : nullptr;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
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
    const bool bRecording = Mode && Mode->IsRecordingShots();
    bShowRecord = State != ELaunchMonitorConnectionState::Disconnected;
    RecordText = ShotDisplay::RecordingStatus(bRecording, Mode ? Mode->GetRecordedShotCount() : 0);
    RecordColor = bRecording ? FLinearColor(1.0f, 0.25f, 0.2f) : FLinearColor(0.65f, 0.65f, 0.65f);
    KeyTip = State == ELaunchMonitorConnectionState::Disconnected ? TEXT("Connect [C]") : TEXT("Disconnect [D]  ·  Record [F9]");
}

EVisibility SRangeHud::GetHudVisibility() const
{
    const AUOpenGolfPlayerController* PlayerController = Controller.Get();
    const AHUD* Hud = PlayerController ? PlayerController->GetHUD() : nullptr;
    // Hidden behind the open test-shot panel, as the Canvas HUD was, and by the showhud command.
    if (!PlayerController || PlayerController->IsReplayPanelOpen() || (Hud && !Hud->bShowHUD))
    {
        return EVisibility::Collapsed;
    }
    return EVisibility::HitTestInvisible;
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
