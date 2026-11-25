// SPDX-License-Identifier: MIT
#include "UI/Menu/AvatarPicker.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenGolfSettings.h"
#include "Profile/AvatarCatalog.h"
#include "UI/Common/AvatarImage.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace AvatarPickerDetail
{
    /** Between the ring and the avatar, and between tiles. */
    constexpr float RingGap = 3.0f;
    constexpr float RingWidth = 2.0f;
    constexpr float TileGap = 6.0f;
    const TCHAR* const AccountName = TEXT("Account picture");
} // namespace AvatarPickerDetail

void SAvatarPicker::Construct(const FArguments& Args)
{
    using namespace AvatarPickerDetail;
    Online = Args._Online;
    Style = Args._Style;
    Preferences = Args._Preferences;
    OnPicked = Args._OnPicked;
    const float Outer = TileSize + 2 * RingGap;
    TileStyle = FButtonStyle()
        .SetNormal(FSlateNoResource())
        .SetHovered(FSlateRoundedBoxBrush(HudStyle::HoverTileColor(), Outer * 0.5f))
        .SetPressed(FSlateRoundedBoxBrush(HudStyle::LaunchTileColor(), Outer * 0.5f))
        .SetDisabled(FSlateNoResource())
        .SetNormalPadding(FMargin(0))
        .SetPressedPadding(FMargin(0));
    Ring = FSlateRoundedBoxBrush(FLinearColor::Transparent, Outer * 0.5f, Style.AccentColor, RingWidth);

    const TSharedRef<SWrapBox> Tiles = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(AvatarPickerDetail::TileGap, AvatarPickerDetail::TileGap));
    // The account picture first; it shows only once the account has one.
    Tiles->AddSlot()
    [
        SNew(SBox).Visibility_Lambda([this] { return Online.IsValid() && Online->GetPicture() ? EVisibility::Visible : EVisibility::Collapsed; })
        [
            MakeTile(FString(), AccountName, TAttribute<UTexture2D*>::CreateLambda([this] { return Online.IsValid() ? Online->GetPicture() : nullptr; }))
        ]
    ];
    for (const AvatarCatalog::FAvatar& Avatar : AvatarCatalog::All())
        Tiles->AddSlot()[MakeTile(Avatar.Id, Avatar.Name, AvatarCatalog::LoadTexture(Avatar))];

    // clang-format off
    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom)
            [
                SNew(STextBlock).Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.LabelColor).Text(FText::FromString(TEXT("Avatar")))
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Bottom).Padding(8, 0, 0, 0)
            [
                SNew(STextBlock).Tag(TEXT("AvatarShownName")).Font(Style.GetFont(Style.SmallSize)).ColorAndOpacity(Style.TextColor)
                .Text_Lambda([this] { return ShownName(); })
            ]
        ]
        + SVerticalBox::Slot().AutoHeight()[Tiles]
    ];
    // clang-format on
}

TSharedRef<SWidget> SAvatarPicker::MakeTile(const FString& Id, const FString& Name, TAttribute<UTexture2D*> Texture)
{
    using namespace AvatarPickerDetail;
    // clang-format off
    return SNew(SButton).ButtonStyle(&TileStyle).ContentPadding(FMargin(0))
        .Tag(FName(*(TEXT("Avatar.") + (Id.IsEmpty() ? FString(TEXT("Account")) : Id))))
#if WITH_ACCESSIBILITY
        .AccessibleText(FText::FromString(Name))
#endif
        .OnClicked_Lambda([this, Id]
        {
            OnPicked.ExecuteIfBound(Id);
            return FReply::Handled();
        })
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SNew(SBorder).BorderImage(&Ring).Padding(RingGap)
                .BorderBackgroundColor_Lambda([this, Id] { return FSlateColor(FLinearColor(1, 1, 1, IsShown(Id) ? 1.0f : 0.0f)); })
                [
                    SNew(SAvatarImage).Size(TileSize).Texture(Texture)
                ]
            ]
        ];
    // clang-format on
}

bool SAvatarPicker::IsShown(const FString& Id) const
{
    if (!Preferences || !Online.IsValid())
        return false;
    const PlayerAvatar::FChoice Choice = PlayerAvatar::Resolve(Preferences(), Online->GetAccountKey(), Online->GetPicture() != nullptr);
    return Id.IsEmpty() ? Choice.IsAccountPicture() : Choice.BuiltIn && Id.Equals(Choice.BuiltIn->Id, ESearchCase::CaseSensitive);
}

FText SAvatarPicker::ShownName() const
{
    if (!Preferences || !Online.IsValid())
        return FText::GetEmpty();
    const PlayerAvatar::FChoice Choice = PlayerAvatar::Resolve(Preferences(), Online->GetAccountKey(), Online->GetPicture() != nullptr);
    return FText::FromString(Choice.IsAccountPicture() ? FString(AvatarPickerDetail::AccountName) : FString(Choice.BuiltIn->Name));
}
