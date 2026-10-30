// SPDX-License-Identifier: MIT
#include "UI/PlayModeBar.h"

#include "Engine/World.h"
#include "UI/HudLayers.h"
#include "UI/HudScale.h"
#include "OpenFairwayBall.h"
#include "Play/OpenGolfPlayRules.h"
#include "Styling/CoreStyle.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
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
    constexpr float YardsPerMeter = 1.0936133f;
    constexpr float MphPerMps = 2.2369363f;
    /** The hole-out banner fades over this last part of the wait before the new hole. */
    constexpr float BannerFadeSeconds = 1.0f;
} // namespace

void SPlayModeBar::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SPlayModeBar::GetBarVisibility));
    Refresh();
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
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().AutoWidth()
                    [
                        MakeIconButton(SHudIconButton::EIcon::Flag, TAttribute<bool>::CreateLambda([this] { return bHoleMode; }),
                            FSimpleDelegate::CreateLambda([this] { if (Controller.IsValid()) Controller->TogglePlayMode(); }),
                            TAttribute<FText>::CreateLambda([this] { return FText::FromString(bHoleMode ? TEXT("Range [G]") : TEXT("Play hole [G]")); }))
                    ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(10, 0, 0, 0)
                    [
                        SNew(SBox).Visibility_Lambda(HoleOnly)
                        [
                            MakeIconButton(SHudIconButton::EIcon::Ball, TAttribute<bool>::CreateLambda([this] { return bPlacing; }),
                                FSimpleDelegate::CreateLambda([this] { if (Controller.IsValid()) Controller->TogglePlacingBall(); }),
                                FText::FromString(TEXT("Place ball [B]")))
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
                [
                    SNew(SBox).Visibility_Lambda(HoleOnly)
                    [
                        SNew(SVerticalBox)
                        + SVerticalBox::Slot().AutoHeight().Padding(TileGap)
                        [
                            SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(8, 4)).Visibility(EVisibility::HitTestInvisible)
                            [
                                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)).ColorAndOpacity(FLinearColor::White)
                                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor)
                                .Text_Lambda([this] { return HoleText; })
                            ]
                        ]
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox).Visibility(EVisibility::HitTestInvisible)
                            + SHorizontalBox::Slot().AutoWidth().Padding(TileGap)[ MakeTile(TAttribute<FText>::CreateLambda([this] { return StrokeText; }), TEXT("Stroke")) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(TileGap)[ MakeTile(TAttribute<FText>::CreateLambda([this] { return ToPinText; }), TEXT("To Pin")) ]
                            + SHorizontalBox::Slot().AutoWidth().Padding(TileGap)[ MakeTile(TAttribute<FText>::CreateLambda([this] { return PuttsText; }), TEXT("Putts")) ]
                        ]
                        + SVerticalBox::Slot().AutoHeight().Padding(TileGap, 6, TileGap, 0)
                        [
                            SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(8, 4)).Visibility_Lambda([this]
                                { return bPlacing ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
                            [
                                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor())
                                .Text(FText::FromString(TEXT("Click the ground to place the ball  ·  Right-click to cancel")))
                            ]
                        ]
                        // A putt that crossed the cup too fast to drop says so, with the speed that would have.
                        + SVerticalBox::Slot().AutoHeight().Padding(TileGap, 6, TileGap, 0)
                        [
                            SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(8, 4)).Visibility_Lambda([this]
                                { return OverCupText.IsEmpty() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
                            [
                                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor())
                                .Text_Lambda([this] { return OverCupText; })
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
                        SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 30)).ColorAndOpacity(FLinearColor::White)
                        .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor)
                        .Text_Lambda([this] { return BannerTitle; })
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 2, 0, 0)
                    [
                        SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 11)).ColorAndOpacity(HudStyle::LabelColor())
                        .Text_Lambda([this] { return BannerDetail; })
                    ]
                ]
            ]
        ]
    ];
    // clang-format on
}

TSharedRef<SWidget> SPlayModeBar::MakeIconButton(SHudIconButton::EIcon Icon, TAttribute<bool> IsActive, FSimpleDelegate OnClicked, TAttribute<FText> Caption)
{
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
        [
            SNew(SHudIconButton).Icon(Icon).IsActive(IsActive).OnClicked(OnClicked)
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 3, 0, 0)
        [
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(HudStyle::LabelColor())
            .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor).Text(Caption).Visibility(EVisibility::HitTestInvisible)
        ];
    // clang-format on
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
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 18)).ColorAndOpacity(FLinearColor::White)
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor).Text(Value)
            ]
            + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).ColorAndOpacity(HudStyle::LabelColor()).Text(FText::FromString(Label))
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
    OverCupText = FText::GetEmpty();
    if (!bHoleMode || !Ball)
        return;

    const FOpenGolfHoleRound& Round = Mode->GetHoleRound();
    FOpenFairwayShotResult Result;
    Ball->GetShotResult(Result);
    HoleText = FText::FromString(FString::Printf(TEXT("Hole 1  ·  Par %d  ·  %d yd%s"), Round.Par,
        FMath::RoundToInt(AUOpenGolfRange::HoleDistanceM * YardsPerMeter), Round.bPractice ? TEXT("  ·  Practice") : TEXT("")));
    // The stroke about to be played, or the final count once holed.
    StrokeText = FText::AsNumber(Round.bHoled ? Round.Strokes : Round.Strokes + 1);
    ToPinText = FText::FromString(Ball->IsHoled() ? FString(TEXT("In")) : OpenGolfPlay::DistanceToPinLabel(Mode->GetDistanceToCupM(), Result.Surface == EOpenFairwaySurface::Green));
    PuttsText = FText::AsNumber(Round.Putts);
    if (Round.bPractice) { StrokeText = FText::FromString(TEXT("?")); PuttsText = StrokeText; }
    if (Result.bOverCup && !Ball->IsHoled())
        OverCupText = FText::FromString(FString::Printf(TEXT("Over the cup at %.1f mph  ·  drops under %.1f mph"), Result.OverCupSpeedMps * MphPerMps,
            Result.OverCupMaxSpeedMps * MphPerMps));

    const float HoledSeconds = Mode->GetHoledSeconds();
    if (Round.bHoled && HoledSeconds >= 0.0f)
    {
        BannerOpacity = FMath::Clamp((AUOpenGolfGameMode::HoleOutDelayS - HoledSeconds) / BannerFadeSeconds, 0.0f, 1.0f);
        BannerTitle = FText::FromString(Round.ResultName());
        BannerDetail = FText::FromString(FString::Printf(TEXT("%d %s  ·  %d %s"), Round.Strokes, Round.Strokes == 1 ? TEXT("stroke") : TEXT("strokes"),
            Round.Putts, Round.Putts == 1 ? TEXT("putt") : TEXT("putts")));
    }
}

EVisibility SPlayModeBar::GetBarVisibility() const
{
    // Only the buttons take clicks; the rest of the screen still reaches the game.
    return HudLayers::IsShown(Controller.Get()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed;
}
