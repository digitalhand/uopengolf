// SPDX-License-Identifier: MIT
#include "UI/Common/HudPointer.h"

#include "UI/Common/HudStatusGlyph.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SDPIScaler.h"
#include "Widgets/SOverlay.h"

namespace
{
    /** The 16 px glyph drawn this much larger: about a system pointer's size, far smaller than the hand. */
    constexpr float BallScale = 1.5f;
    const FLinearColor Shadow(0.f, 0.f, 0.f, .6f);
} // namespace

TSharedRef<SWidget> HudPointer::MakeBall()
{
    // Slate centres a software pointer on the pointer position, so the ball sits where it will be placed.
    // clang-format off
    return SNew(SBox).Tag(BallTag).Visibility(EVisibility::HitTestInvisible)
    [
        SNew(SDPIScaler).DPIScale(BallScale)
        [
            SNew(SOverlay)
            + SOverlay::Slot().Padding(FMargin(1, 1, 0, 0))[ SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Ball).Color(Shadow) ]
            + SOverlay::Slot().Padding(FMargin(0, 0, 1, 1))[ SNew(SHudStatusGlyph).Kind(SHudStatusGlyph::EKind::Ball) ]
        ]
    ];
    // clang-format on
}
