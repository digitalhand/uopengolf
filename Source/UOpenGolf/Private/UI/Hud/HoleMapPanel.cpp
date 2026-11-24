// SPDX-License-Identifier: MIT
#include "UI/Hud/HoleMapPanel.h"
#include "Engine/World.h"
#include "HoleMap/HoleMapLabels.h"
#include "OpenFairwayBall.h"
#include "UI/Hud/HoleMinimap.h"
#include "UOpenGolfGameMode.h"
#include "UOpenGolfPlayerController.h"
#include "UOpenGolfRange.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace HoleMapPanelDetail
{
    constexpr float HeaderHeight = 28.0f;
    constexpr float FooterHeight = 30.0f;
    /** A ball this close (cm) to the tee is on the tee box. */
    constexpr double TeeRadiusCm = 300.0;

    void SetIfChanged(FText& Text, const FString& Value)
    {
        if (!Text.ToString().Equals(Value, ESearchCase::CaseSensitive))
            Text = FText::FromString(Value);
    }
} // namespace HoleMapPanelDetail

void SHoleMapPanel::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    SetTag(TEXT("HoleMapPanel"));
    SetVisibility(TAttribute<EVisibility>::CreateSP(this, &SHoleMapPanel::GetPanelVisibility));
    Map = SNew(SHoleMinimap).Controller(Controller);
    Refresh();

    // clang-format off
    ChildSlot
    [
        // A soft band under the panel lifts it off the game view.
        SNew(SBorder).BorderImage(&ShadowBrush).Padding(FMargin(0, 0, 0, 2))
        [
            SNew(SBorder).BorderImage(&PanelBrush).Padding(1)
            [
                SNew(SBox).WidthOverride(SHoleMinimap::Width)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()[ MakeHeader() ]
                    + SVerticalBox::Slot().AutoHeight()[ Map.ToSharedRef() ]
                    + SVerticalBox::Slot().AutoHeight()[ MakeFooter() ]
                ]
            ]
        ]
    ];
    // clang-format on
}

TSharedRef<SWidget> SHoleMapPanel::MakeBar(FName Tag, float Height, const TSharedRef<SWidget>& Left, const TSharedRef<SWidget>& Right)
{
    // clang-format off
    return SNew(SBox).Tag(Tag).HeightOverride(Height).Padding(FMargin(9, 0))
        [
            SNew(SHorizontalBox).Visibility(EVisibility::HitTestInvisible)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[ Left ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ Right ]
        ];
    // clang-format on
}

TSharedRef<SWidget> SHoleMapPanel::MakeHeader()
{
    return MakeBar(TEXT("HoleMapHeader"), HoleMapPanelDetail::HeaderHeight,
        SNew(STextBlock).Tag(TEXT("HoleMapLie")).Font(HudStyle::Font("Bold", 11)).ColorAndOpacity(HudStyle::TextColor()).Text_Lambda([this] { return Lie; }),
        SNew(STextBlock).Font(HudStyle::Font("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor()).Text_Lambda([this] { return Hole; }));
}

TSharedRef<SWidget> SHoleMapPanel::MakeFooter()
{
    return MakeBar(TEXT("HoleMapFooter"), HoleMapPanelDetail::FooterHeight,
        SNew(STextBlock).Font(HudStyle::Font("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor()).Text(FText::FromString(TEXT("Elevation to pin"))),
        SNew(STextBlock).Font(HudStyle::Font("Bold", 12)).ColorAndOpacity(HudStyle::TextColor()).Text_Lambda([this] { return Elevation; }));
}

EVisibility SHoleMapPanel::GetPanelVisibility() const
{
    return Map.IsValid() ? Map->GetMapVisibility() : EVisibility::Hidden;
}

void SHoleMapPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    Refresh();
}

FReply SHoleMapPanel::OnMouseButtonDown(const FGeometry&, const FPointerEvent& Event)
{
    // The map handles its own clicks; a left click on the header or footer stops here rather than placing the ball.
    return Event.GetEffectingButton() == EKeys::LeftMouseButton ? FReply::Handled() : FReply::Unhandled();
}

void SHoleMapPanel::Refresh()
{
    using namespace HoleMapPanelDetail;
    const UWorld* World = Controller.IsValid() ? Controller->GetWorld() : nullptr;
    const AUOpenGolfGameMode* Mode = World ? World->GetAuthGameMode<AUOpenGolfGameMode>() : nullptr;
    const AUOpenGolfRange* Range = Mode ? Mode->GetRange() : nullptr;
    const AOpenFairwayBall* Ball = Mode ? Mode->GetBall() : nullptr;
    if (!Range || !Ball)
        return;

    SetIfChanged(Hole, Mode->IsCourseRound() ? FString::Printf(TEXT("Hole %d  ·  Par %d"), Mode->GetHoleNumber(), Mode->GetPar()) : FString(TEXT("Training")));
    const FVector Location = Ball->GetActorLocation();
    // In the air the header keeps the lie the shot was hit from; it names the surface again once the ball is down.
    if (Ball->GetBallState() != EOpenFairwayBallState::Flight)
    {
        const bool bOnTee = FVector::Dist2D(Location, Range->GetTeeLocation()) <= TeeRadiusCm;
        SetIfChanged(Lie, HoleMapLabels::LieText(Ball->IsHoled(), bOnTee, Ball->GetResult().Surface));
    }

    // Measured on the sampled ground at both ends, so a flat green reads level rather than a stray inch.
    const FHoleMapField& Heights = Range->GetHoleMapHeights();
    const double DeltaM = (Heights.At(FVector2D(Mode->GetCupLocation())) - Heights.At(FVector2D(Location))) / 100.0;
    SetIfChanged(Elevation, Ball->IsMoving() || Ball->IsHoled() || !Heights.IsValid() ? FString(TEXT("—"))
        : HoleMapLabels::ElevationText(DeltaM, HoleMapLabels::MetricHeights()));
}
