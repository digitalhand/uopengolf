// SPDX-License-Identifier: MIT
#include "SOpenGolfIconButton.h"
#include "Rendering/DrawElements.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SLeafWidget.h"

namespace OpenGolfIconButtonDetail
{
    /** Icons are drawn in a square of this many units. */
    constexpr float GlyphSize = 24.0f;
    constexpr float DisabledAlpha = 0.4f;

    class SAccountGlyph : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SAccountGlyph) : _Color(FLinearColor::White), _Stroke(1.8f) {}
        SLATE_ATTRIBUTE(EOpenGolfAccountIcon, Icon)
        SLATE_ARGUMENT(FLinearColor, Color)
        SLATE_ARGUMENT(float, Stroke)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            Icon = Args._Icon;
            Color = Args._Color;
            Stroke = Args._Stroke;
        }
        virtual bool ComputeVolatility() const override { return true; }
        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(GlyphSize, GlyphSize); }
        virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
            const FWidgetStyle& WidgetStyle, bool bEnabled) const override
        {
            const FLinearColor Tint = FLinearColor(Color.R, Color.G, Color.B, Color.A * (bEnabled ? 1.0f : DisabledAlpha)) * WidgetStyle.GetColorAndOpacityTint();
            auto Line = [&](const TArray<FVector2f>& Points)
            { FSlateDrawElement::MakeLines(Elements, Layer, G.ToPaintGeometry(), Points, ESlateDrawEffect::None, Tint, true, Stroke); };
            auto Circle = [&](FVector2f Center, float Radius)
            {
                TArray<FVector2f> Points;
                for (int32 I = 0; I <= 32; ++I)
                {
                    const float A = I * 2 * PI / 32;
                    Points.Add(Center + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius);
                }
                Line(Points);
            };
            switch (Icon.Get(EOpenGolfAccountIcon::SignIn))
            {
            case EOpenGolfAccountIcon::Settings:
            {
                // A gear: alternating outer and inner radius, with an axle.
                TArray<FVector2f> Points;
                for (int32 I = 0; I <= 32; ++I)
                {
                    const float A = I * 2 * PI / 32, Radius = (I % 4 < 2) ? 10 : 8;
                    Points.Add(FVector2f(12, 12) + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius);
                }
                Line(Points);
                Circle(FVector2f(12, 12), 3.5f);
                break;
            }
            case EOpenGolfAccountIcon::SignOut:
                Line({{13, 4}, {4, 4}, {4, 20}, {13, 20}});
                Line({{10, 12}, {22, 12}});
                Line({{17, 7}, {22, 12}, {17, 17}});
                break;
            case EOpenGolfAccountIcon::Back:
                // The sign-out arrow's head, mirrored, on a shaft as wide as the gear.
                Line({{20, 12}, {4, 12}});
                Line({{10, 6}, {4, 12}, {10, 18}});
                break;
            case EOpenGolfAccountIcon::Close:
                Line({{6, 6}, {18, 18}});
                Line({{18, 6}, {6, 18}});
                break;
            default:
                Circle(FVector2f(8, 7), 3.5f);
                Line({{2, 21}, {2, 19}, {3, 16}, {6, 14}, {10, 14}, {12, 15}});
                Line({{14, 12}, {22, 12}});
                Line({{18, 8}, {22, 12}, {18, 16}});
                break;
            }
            return Layer;
        }

    private:
        TAttribute<EOpenGolfAccountIcon> Icon;
        FLinearColor Color = FLinearColor::White;
        float Stroke = 1.8f;
    };
} // namespace OpenGolfIconButtonDetail

void SOpenGolfIconButton::Construct(const FArguments& Args)
{
    using namespace OpenGolfIconButtonDetail;
    Style = Args._Style;
    // The fixed-size box and centered alignment place the glyph; padding would only squeeze it.
    Style.IconButton.SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
    const FSimpleDelegate Action = Args._OnClicked;
    ChildSlot
    [
        SNew(SBox).WidthOverride(Args._Size).HeightOverride(Args._Size)
        [
            SNew(SButton).ButtonStyle(&Style.IconButton).ContentPadding(FMargin(0)).HAlign(HAlign_Center).VAlign(VAlign_Center)
                .IsEnabled(Args._IsEnabled)
#if WITH_ACCESSIBILITY
                .AccessibleText(Args._Label)
#endif
                .OnClicked_Lambda([Action] { Action.ExecuteIfBound(); return FReply::Handled(); })
            [
                SNew(SAccountGlyph).Icon(Args._Icon).Color(Style.IconColor).Stroke(Style.IconStroke)
            ]
        ]
    ];
}
