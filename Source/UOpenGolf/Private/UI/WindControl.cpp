// SPDX-License-Identifier: MIT
#include "UI/WindControl.h"

#include "OpenGolfWindIndicator.h"
#include "Play/OpenGolfWindSetting.h"
#include "Styling/CoreStyle.h"
#include "Styling/StyleDefaults.h"
#include "UI/HudLayers.h"
#include "UI/HudScale.h"
#include "UI/HudSlider.h"
#include "UI/HudSwitch.h"
#include "UI/WindIndicator.h"
#include "UOpenGolfPlayerController.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace
{
    /** Where WBP_WindIndicator places the indicator (top centre, this far down) until it has been drawn. */
    constexpr float DefaultIndicatorTop = 20.0f;
    constexpr float LabelWidth = 64.0f;
    constexpr float ValueWidth = 52.0f;
    /** The panel starts this far up, under the indicator, as it slides out. */
    constexpr float SlideDistance = 10.0f;
} // namespace

void SWindControl::Construct(const FArguments& Args)
{
    Controller = Args._Controller;
    WindIndicator = Args._WindIndicator;
    // Only the indicator's click area and the open panel take clicks; the rest of the screen still reaches the game.
    SetVisibility(TAttribute<EVisibility>::CreateLambda([this]
        { return HudLayers::IsShown(Controller.Get()) ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; }));
    auto IsOpen = [this] { return Controller.IsValid() && Controller->IsWindPanelOpen(); };
    // The indicator's own scale, should the Blueprint draw it larger or smaller.
    auto IndicatorScale = [this] { return IndicatorWidth > 0.0f ? IndicatorWidth / SWindIndicator::PanelWidth : 1.0f; };
    // clang-format off
    ChildSlot
    [
        SNew(SDPIScaler).DPIScale_Lambda([this] { return HudScale::Readable(Controller.IsValid() ? Controller->GetWorld() : nullptr); })
        [
            SAssignNew(Layer, SOverlay)
            + SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(TAttribute<FMargin>::CreateSP(this, &SWindControl::GetColumnPadding))
            [
                SNew(SBox).WidthOverride(PanelWidth)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
                    [
                        // Over the indicator: an outline while hovered or open, and the click that opens the panel.
                        SAssignNew(ClickArea, SBorder).BorderImage(FStyleDefaults::GetNoBrush()).Padding(0).Cursor(EMouseCursor::Hand)
                        .OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent& Event)
                        {
                            if (Event.GetEffectingButton() != EKeys::LeftMouseButton || !Controller.IsValid())
                                return FReply::Unhandled();
                            Controller->ToggleWindPanel();
                            return FReply::Handled();
                        })
                        [
                            SNew(SBox).WidthOverride_Lambda([this] { return IndicatorWidth; })
                            .HeightOverride_Lambda([IndicatorScale] { return SWindIndicator::PanelHeight * IndicatorScale(); })
                            [
                                SNew(SBorder).Visibility(EVisibility::HitTestInvisible)
                                .BorderImage_Lambda([this, IsOpen]
                                {
                                    const bool bHovered = ClickArea.IsValid() && ClickArea->IsHovered();
                                    return bHovered || IsOpen() ? static_cast<const FSlateBrush*>(&HoverOutline) : FStyleDefaults::GetNoBrush();
                                })
                            ]
                        ]
                    ]
                    + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0, 3, 0, 0)
                    [
                        SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 8)).Visibility(EVisibility::HitTestInvisible)
                        .ColorAndOpacity_Lambda([IsOpen] { return FSlateColor(IsOpen() ? HudStyle::AccentColor() : HudStyle::LabelColor()); })
                        .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor)
                        .Text(FText::FromString(TEXT("Wind [W]")))
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
                    [
                        // The panel takes its own clicks, so none fall through to the ground under it.
                        SNew(SBorder).BorderImage(&TileBrush).Padding(FMargin(12, 8))
                        .Visibility_Lambda([this] { return Openness > 0.0f ? EVisibility::Visible : EVisibility::Collapsed; })
                        .ColorAndOpacity_Lambda([this] { return FLinearColor(1, 1, 1, Openness); })
                        .BorderBackgroundColor_Lambda([this] { return FSlateColor(FLinearColor(1, 1, 1, Openness)); })
                        .RenderTransform_Lambda([this]
                        {
                            return TOptional<FSlateRenderTransform>(FSlateRenderTransform(FVector2f(0.0f, -SlideDistance * (1.0f - FMath::Sin(Openness * UE_HALF_PI)))));
                        })
                        .OnMouseButtonDown_Lambda([](const FGeometry&, const FPointerEvent&) { return FReply::Handled(); })
                        [
                            MakePanel()
                        ]
                    ]
                ]
            ]
        ]
    ];
    // clang-format on
}

TSharedRef<SWidget> SWindControl::MakePanel()
{
    using namespace OpenGolfPlay;
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 11)).ColorAndOpacity(FLinearColor::White)
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor).Text(FText::FromString(TEXT("Wind")))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor())
                .Text_Lambda([this] { return FText::FromString(IsWindOn() ? TEXT("On") : TEXT("Off")); })
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SHudSwitch).IsOn_Lambda([this] { return IsWindOn(); })
                .OnToggled_Lambda([this] { if (Controller.IsValid()) Controller->SetWindOn(!IsWindOn()); })
            ]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 8, 0, 0)
        [
            MakeSliderRow(TEXT("Speed"), MaxWindSettingMph, WindSpeedStepMph,
                TAttribute<float>::CreateLambda([this] { return Controller.IsValid() ? FMath::Min(Controller->GetWind().SpeedMph, MaxWindSettingMph) : 0.0f; }),
                FOnFloatValueChanged::CreateLambda([this](float Mph) { if (Controller.IsValid()) Controller->SetWindSpeedMph(Mph); }),
                TAttribute<FText>::CreateLambda([this]
                    { return FText::FromString(FString::Printf(TEXT("%d mph"), Controller.IsValid() ? FMath::RoundToInt(Controller->GetWind().SpeedMph) : 0)); }))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
        [
            MakeSliderRow(TEXT("Direction"), 360.0f - WindDirectionStepDeg, WindDirectionStepDeg,
                TAttribute<float>::CreateLambda([this] { return Controller.IsValid() ? Controller->GetWind().FromDeg : 0.0f; }),
                FOnFloatValueChanged::CreateLambda([this](float Deg) { if (Controller.IsValid()) Controller->SetWindFromDeg(Deg); }),
                TAttribute<FText>::CreateLambda([this]
                    { return FText::FromString(FString::Printf(TEXT("%d°"), Controller.IsValid() ? FMath::RoundToInt(Controller->GetWind().FromDeg) : 0)); }))
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(LabelWidth, 2, 0, 0)
        [
            SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor())
            .Text_Lambda([this] { return GetDirectionText(); })
        ];
    // clang-format on
}

TSharedRef<SWidget> SWindControl::MakeSliderRow(const TCHAR* Label, float MaxValue, float Step, TAttribute<float> Value, FOnFloatValueChanged OnChanged,
    TAttribute<FText> ValueText)
{
    // clang-format off
    return SNew(SHorizontalBox).IsEnabled_Lambda([this] { return IsWindOn(); })
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(LabelWidth)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 9)).ColorAndOpacity(HudStyle::LabelColor()).Text(FText::FromString(Label))
            ]
        ]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [
            SNew(SHudSlider).MinValue(0.0f).MaxValue(MaxValue).Step(Step).Value(Value).OnValueChanged(OnChanged)
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SBox).WidthOverride(ValueWidth).HAlign(HAlign_Right)
            [
                SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Bold", 10)).ColorAndOpacity(FLinearColor::White)
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor).Text(ValueText)
            ]
        ];
    // clang-format on
}

void SWindControl::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    TrackIndicator();
    const bool bOpen = Controller.IsValid() && Controller->IsWindPanelOpen();
    Openness = FMath::FInterpConstantTo(Openness, bOpen ? 1.0f : 0.0f, InDeltaTime, 1.0f / SlideSeconds);
}

void SWindControl::TrackIndicator()
{
    if (!Layer.IsValid())
        return;
    const FGeometry& LayerGeometry = Layer->GetTickSpaceGeometry();
    const UOpenGolfWindIndicator* Indicator = WindIndicator.Get();
    const TSharedPtr<SWidget> Drawn = Indicator ? Indicator->GetCachedWidget() : nullptr;
    if (Drawn.IsValid() && Drawn->GetTickSpaceGeometry().GetLocalSize().X > 0.0f && LayerGeometry.GetLocalSize().X > 0.0f)
    {
        const FGeometry& Shown = Drawn->GetTickSpaceGeometry();
        IndicatorPosition = LayerGeometry.AbsoluteToLocal(Shown.GetAbsolutePosition());
        IndicatorWidth = LayerGeometry.AbsoluteToLocal(Shown.LocalToAbsolute(FVector2D(Shown.GetLocalSize().X, 0.0))).X - IndicatorPosition.X;
        return;
    }
    IndicatorWidth = SWindIndicator::PanelWidth;
    IndicatorPosition = FVector2D((LayerGeometry.GetLocalSize().X - IndicatorWidth) * 0.5f, DefaultIndicatorTop);
}

FMargin SWindControl::GetColumnPadding() const
{
    // The column is centred on the indicator, so the click area lies over it and the panel hangs centred below.
    return FMargin(FMath::Max(0.0f, IndicatorPosition.X + IndicatorWidth * 0.5f - PanelWidth * 0.5f), FMath::Max(0.0f, IndicatorPosition.Y), 0.0f, 0.0f);
}

FText SWindControl::GetDirectionText() const
{
    if (!Controller.IsValid() || Controller->GetWind().AppliedMph() <= 0.0f)
        return FText::FromString(TEXT("Calm"));
    return FText::FromString(OpenGolfPlay::WindDirectionLabel(Controller->GetWind().FromDeg));
}

bool SWindControl::IsWindOn() const
{
    return Controller.IsValid() && Controller->GetWind().bOn;
}
