// SPDX-License-Identifier: MIT
#pragma once
#include "Widgets/SCompoundWidget.h"
#include "OpenGolfAccountStyle.h"

/** Line icons drawn by the account widgets, so header, back and delete controls read as one set. */
enum class EOpenGolfAccountIcon : uint8 { SignIn, Settings, SignOut, Back, Close, Edit };

/**
 * A square button with a drawn icon. Label names it for screen readers; it shows no tooltip. Use the standard
 * Tag, IsEnabled and Visibility arguments.
 */
class OPENGOLFONLINE_API SOpenGolfIconButton : public SCompoundWidget
{
public:
    static constexpr float DefaultSize = 44.0f;

    SLATE_BEGIN_ARGS(SOpenGolfIconButton) : _Icon(EOpenGolfAccountIcon::Back), _Size(DefaultSize) {}
    SLATE_ATTRIBUTE(EOpenGolfAccountIcon, Icon)
    /** The accessible name, such as "Back"; never shown. */
    SLATE_ATTRIBUTE(FText, Label)
    SLATE_ARGUMENT(FOpenGolfAccountStyle, Style)
    /** Width and height of the button; the icon stays 24 units. */
    SLATE_ARGUMENT(float, Size)
    SLATE_EVENT(FSimpleDelegate, OnClicked)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

private:
    /** The inner button points at this copy's IconButton style. */
    FOpenGolfAccountStyle Style;
};
