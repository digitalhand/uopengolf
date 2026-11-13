// SPDX-License-Identifier: MIT
#include "UI/Common/MenuStyle.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "UI/Common/HudStyle.h"

namespace MenuStyleDetail
{
    const FMargin ButtonPadding(18, 10);
    const FMargin FieldPadding(12, 10);

    FLinearColor White(float Alpha) { return FLinearColor(1, 1, 1, Alpha); }

    /** Color scaled toward black (Amount < 1) or white (Amount > 1), alpha kept. */
    FLinearColor Shade(const FLinearColor& Color, float Amount)
    {
        const FLinearColor Target = Amount < 1 ? FLinearColor::Black : FLinearColor::White;
        FLinearColor Result = Color + (Target - Color) * FMath::Abs(Amount - 1);
        Result.A = Color.A;
        return Result;
    }

    FSlateRoundedBoxBrush Box(const FLinearColor& Fill, const FLinearColor& Outline)
    {
        return FSlateRoundedBoxBrush(Fill, MenuStyle::ControlRadius, Outline, MenuStyle::OutlineWidth);
    }

    FButtonStyle MakeButton(const FSlateBrush& Normal, const FSlateBrush& Hovered, const FSlateBrush& Pressed)
    {
        return FButtonStyle()
            .SetNormal(Normal)
            .SetHovered(Hovered)
            .SetPressed(Pressed)
            .SetDisabled(Box(White(0.02f), White(0.06f)))
            .SetNormalPadding(ButtonPadding)
            .SetPressedPadding(ButtonPadding);
    }

    FButtonStyle StandardButton()
    {
        const FLinearColor Hover = HudStyle::HoverTileColor();
        return MakeButton(Box(White(0.035f), White(0.12f)), Box(Hover, White(0.25f)), Box(Shade(Hover, 0.8f), White(0.35f)));
    }

    FButtonStyle PrimaryButton()
    {
        const FLinearColor Accent = HudStyle::AccentColor();
        return MakeButton(Box(Accent, Accent), Box(Shade(Accent, 1.15f), Shade(Accent, 1.15f)), Box(Shade(Accent, 0.85f), Shade(Accent, 0.85f)));
    }
} // namespace MenuStyleDetail

FOpenGolfAccountStyle MenuStyle::Make()
{
    using namespace MenuStyleDetail;
    FOpenGolfAccountStyle Result;
    Result.Font = [](const FName& Weight, int32 Size) { return HudStyle::Font(Weight, Size); };
    Result.TextColor = HudStyle::TextColor();
    Result.LabelColor = HudStyle::LabelColor();
    Result.AccentColor = HudStyle::AccentColor();
    // The tile color, opaque: dark text on the light accent.
    Result.PrimaryLabelColor = FLinearColor::FromSRGBColor(FColor(18, 25, 23));
    Result.IconColor = HudStyle::TextColor();
    Result.TitleSize = TitleSize;
    Result.BodySize = BodySize;
    Result.SmallSize = SmallSize;
    Result.Button = StandardButton();
    Result.PrimaryButton = MenuStyleDetail::PrimaryButton();
    Result.IconButton = StandardButton();
    Result.Section = FSlateRoundedBoxBrush(White(0.025f), ControlRadius);
    Result.TextField = TextField();
    return Result;
}

FButtonStyle MenuStyle::NavButton()
{
    using namespace MenuStyleDetail;
    const FLinearColor Hover = HudStyle::HoverTileColor();
    return MakeButton(Box(FLinearColor::Transparent, FLinearColor::Transparent), Box(Hover, White(0.25f)), Box(Shade(Hover, 0.8f), White(0.35f)));
}

FEditableTextBoxStyle MenuStyle::TextField()
{
    using namespace MenuStyleDetail;
    // Starts from the engine field so scroll bars and caret settings stay; the brushes are drawn as given, untinted.
    FEditableTextBoxStyle Field = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
    Field.SetBackgroundImageNormal(Box(White(0.035f), White(0.12f)))
        .SetBackgroundImageHovered(Box(White(0.05f), White(0.25f)))
        .SetBackgroundImageFocused(Box(White(0.05f), HudStyle::AccentColor()))
        .SetBackgroundImageReadOnly(Box(White(0.02f), White(0.06f)))
        .SetBackgroundColor(FLinearColor::White)
        .SetPadding(FieldPadding)
        .SetFont(HudStyle::Font("Regular", NavSize))
        .SetForegroundColor(HudStyle::TextColor())
        .SetFocusedForegroundColor(HudStyle::TextColor());
    return Field;
}

FSearchBoxStyle MenuStyle::SearchBox()
{
    FSearchBoxStyle Search = FCoreStyle::Get().GetWidgetStyle<FSearchBoxStyle>("SearchBox");
    const FEditableTextBoxStyle Field = TextField();
    Search.SetTextBoxStyle(Field).SetActiveFont(Field.TextStyle.Font);
    return Search;
}
