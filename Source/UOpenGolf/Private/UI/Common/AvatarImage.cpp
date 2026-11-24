// SPDX-License-Identifier: MIT
#include "UI/Common/AvatarImage.h"
#include "Engine/Texture2D.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SOverlay.h"

void SAvatarImage::Construct(const FArguments& Args)
{
    Texture = Args._Texture;
    Size = Args._Size;
    Backing = FSlateRoundedBoxBrush(Args._BackingColor, Size * 0.5f);
    Show(Texture.Get());
    // clang-format off
    ChildSlot
    [
        SNew(SBox).WidthOverride(Size).HeightOverride(Size)
        [
            SNew(SOverlay)
            + SOverlay::Slot()[SNew(SImage).Image(&Backing)]
            + SOverlay::Slot()[SNew(SImage).Image(&Brush)]
        ]
    ];
    // clang-format on
}

void SAvatarImage::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
    SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
    UTexture2D* Next = Texture.Get();
    if (Next != Held.Get())
        Show(Next);
}

void SAvatarImage::Show(UTexture2D* Next)
{
    Held.Reset(Next);
    Brush = FSlateBrush();
    // The image keeps pointing at Brush, so it is told its contents changed.
    Invalidate(EInvalidateWidgetReason::Paint);
    if (!Next)
    {
        Brush.DrawAs = ESlateBrushDrawType::NoDrawType;
        return;
    }
    // A rounded box with a texture draws the texture clipped to the circle, for square photos and emoji alike.
    Brush.SetResourceObject(Next);
    Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
    Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::HalfHeightRadius;
    Brush.OutlineSettings.Width = 0.0f;
    Brush.ImageSize = FVector2D(Size, Size);
    Brush.TintColor = FSlateColor(FLinearColor::White);
}
