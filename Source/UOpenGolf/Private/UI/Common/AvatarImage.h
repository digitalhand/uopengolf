// SPDX-License-Identifier: MIT
#pragma once

#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateBrush.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SCompoundWidget.h"

class UTexture2D;

/**
 * A round player avatar, a built-in avatar or an account picture, on a soft disc. It keeps the texture it shows alive:
 * a brush's resource object is not a garbage-collection root.
 */
class SAvatarImage : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SAvatarImage) : _Size(36.0f), _BackingColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.08f)) {}
    /** Read every frame, so a picture that downloads later appears without a rebuild. Null shows the disc alone. */
    SLATE_ATTRIBUTE(UTexture2D*, Texture)
    /** The circle's diameter in Slate units. */
    SLATE_ARGUMENT(float, Size)
    SLATE_ARGUMENT(FLinearColor, BackingColor)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
    UTexture2D* GetShownTexture() const { return Held.Get(); }

private:
    void Show(UTexture2D* Texture);

    TAttribute<UTexture2D*> Texture;
    TStrongObjectPtr<UTexture2D> Held;
    FSlateBrush Brush;
    FSlateRoundedBoxBrush Backing{FLinearColor::Transparent};
    float Size = 36.0f;
};
