// SPDX-License-Identifier: MIT
#include "OpenGolfAccountStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"

namespace OpenGolfAccountStyleDetail
{
    constexpr float Radius = 6.0f;
    constexpr float IconRadius = 8.0f;
    const FMargin ButtonPadding(16, 10);

    FLinearColor White(float Alpha) { return FLinearColor(1, 1, 1, Alpha); }

    FButtonStyle MakeButton(const FSlateBrush& Normal, const FSlateBrush& Hovered, const FSlateBrush& Pressed, const FSlateBrush& Disabled)
    {
        return FButtonStyle().SetNormal(Normal).SetHovered(Hovered).SetPressed(Pressed).SetDisabled(Disabled)
            .SetNormalPadding(ButtonPadding).SetPressedPadding(ButtonPadding);
    }
} // namespace OpenGolfAccountStyleDetail

FOpenGolfAccountStyle::FOpenGolfAccountStyle()
    : Font([](const FName& Weight, int32 Size) { return FCoreStyle::GetDefaultFontStyle(Weight, Size); })
    , TextColor(0.92f, 0.95f, 0.94f)
    , LabelColor(0.62f, 0.67f, 0.65f)
    , AccentColor(0.45f, 0.80f, 0.60f)
    , PrimaryLabelColor(0.95f, 0.97f, 0.96f)
    , IconColor(0.92f, 0.95f, 0.94f)
{
    using namespace OpenGolfAccountStyleDetail;
    const FSlateRoundedBoxBrush Disabled(White(0.02f), Radius, White(0.06f), 1);
    Button = MakeButton(FSlateRoundedBoxBrush(White(0.035f), Radius, White(0.12f), 1), FSlateRoundedBoxBrush(White(0.09f), Radius, White(0.20f), 1),
        FSlateRoundedBoxBrush(White(0.15f), Radius, White(0.20f), 1), Disabled);
    PrimaryButton = MakeButton(FSlateRoundedBoxBrush(FLinearColor(0.17f, 0.46f, 0.31f), Radius), FSlateRoundedBoxBrush(FLinearColor(0.22f, 0.57f, 0.38f), Radius),
        FSlateRoundedBoxBrush(FLinearColor(0.14f, 0.38f, 0.26f), Radius), Disabled);
    IconButton = MakeButton(FSlateRoundedBoxBrush(White(0.035f), IconRadius, White(0.12f), 1), FSlateRoundedBoxBrush(White(0.10f), IconRadius, White(0.25f), 1),
        FSlateRoundedBoxBrush(White(0.17f), IconRadius, White(0.35f), 1), FSlateRoundedBoxBrush(White(0.02f), IconRadius, White(0.06f), 1));
    Section = FSlateRoundedBoxBrush(White(0.025f), Radius);
}

FSlateFontInfo FOpenGolfAccountStyle::GetFont(int32 Size) const
{
    return Font ? Font(TEXT("Regular"), Size) : FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
}
