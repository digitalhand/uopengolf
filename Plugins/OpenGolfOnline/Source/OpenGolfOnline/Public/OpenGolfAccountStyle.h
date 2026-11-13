// SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"

/**
 * Fonts, colors and button styles for the account widgets. The default is the plugin's own look;
 * a host passes its own copy so the account page and header controls match the rest of its menus.
 * Widgets copy the style they are given, so a temporary is fine.
 */
struct OPENGOLFONLINE_API FOpenGolfAccountStyle
{
    FOpenGolfAccountStyle();

    /** Font for a weight ("Regular", "Bold") and nominal size. A host can apply its text-size preference here. */
    TFunction<FSlateFontInfo(const FName& Weight, int32 Size)> Font;
    FLinearColor TextColor;
    /** Captions, hints and secondary values. */
    FLinearColor LabelColor;
    /** Highlighted text, such as a club row's "View". */
    FLinearColor AccentColor;
    /** Label on PrimaryButton. */
    FLinearColor PrimaryLabelColor;
    /** Stroke of the drawn icons. */
    FLinearColor IconColor;
    float IconStroke = 1.8f;
    int32 TitleSize = 26;
    int32 BodySize = 16;
    int32 SmallSize = 13;
    FButtonStyle Button;
    FButtonStyle PrimaryButton;
    /** Square icon buttons; their glyph is centered, so padding is ignored. */
    FButtonStyle IconButton;
    /** Background of a grouped section of the page. */
    FSlateBrush Section;
    /** Text fields such as the display name; the engine's default field when unset. */
    TOptional<FEditableTextBoxStyle> TextField;

    /** Regular-weight font at Size, from Font or the engine default when Font is unset. */
    FSlateFontInfo GetFont(int32 Size) const;
};
