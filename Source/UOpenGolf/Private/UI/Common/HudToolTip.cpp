// SPDX-License-Identifier: MIT
#include "UI/Common/HudToolTip.h"

#include "Brushes/SlateRoundedBoxBrush.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/SToolTip.h"
#include "Widgets/Text/STextBlock.h"

namespace HudToolTipDetail
{
    /** Wide enough for a short sentence per line; Slate's default wraps near 1000 px. */
    constexpr float WrapWidth = 300.0f;

    /**
     * Static so it outlives every tooltip: Slate can keep showing a tooltip for a frame after the HUD that made it is
     * rebuilt. Opaque, so the text stays readable over the course.
     */
    const FSlateBrush& Background()
    {
        static const FSlateRoundedBoxBrush Brush(FLinearColor::FromSRGBColor(FColor(18, 25, 23)), HudStyle::TileRadius,
            HudStyle::AccentColor().CopyWithNewOpacity(0.6f), 1.0f);
        return Brush;
    }
} // namespace HudToolTipDetail

TSharedRef<SToolTip> HudToolTip::Make(const TAttribute<FText>& Text)
{
    // Text is also set on the tooltip itself: Slate skips empty tooltips, and tests read it back.
    return SNew(SToolTip).Tag(Tag).Text(Text)
        .BorderImage(&HudToolTipDetail::Background())
        .TextMargin(FMargin(10, 6))
        [
            SNew(STextBlock).Text(Text).Font(HudStyle::Font("Regular", 10)).ColorAndOpacity(HudStyle::TextColor())
            .WrapTextAt(HudToolTipDetail::WrapWidth)
        ];
}
