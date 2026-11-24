// SPDX-License-Identifier: MIT
#include "UI/Hud/PlayerBadge.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenGolfSettings.h"
#include "Profile/AvatarCatalog.h"
#include "UI/Common/AvatarImage.h"
#include "UI/Common/HudStyle.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace PlayerBadgeDetail
{
    /** From the club button's label, which has its own right padding. */
    constexpr float LeftGap = 6.0f;
    constexpr float NameGap = 8.0f;
    /** To the toolbar tile's edge, as the club button leaves after its label. */
    constexpr float RightGap = 12.0f;
} // namespace PlayerBadgeDetail

void SPlayerBadge::Construct(const FArguments& Args)
{
    using namespace PlayerBadgeDetail;
    Online = Args._Online;
    SetTag(TEXT("PlayerBadge"));
    // clang-format off
    ChildSlot
    [
        SNew(SHorizontalBox).Visibility(EVisibility::HitTestInvisible)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(LeftGap, 0, 0, 0)
        [
            // The applied preferences: a pick still being edited in Settings shows here only once applied.
            SNew(SAvatarImage).Tag(TEXT("PlayerBadgeAvatar")).Size(AvatarSize)
            .Texture_Lambda([this] { return PlayerAvatar::Texture(GolfSettings::Get(), Online.Get()); })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(NameGap, 0, RightGap, 0)
        [
            SNew(SBox).MaxDesiredWidth(MaxNameWidth)
            .Visibility_Lambda([this] { return Online.IsValid() && !Online->GetDisplayName().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
            [
                SNew(STextBlock).Tag(TEXT("PlayerBadgeName")).Font(HudStyle::ValueFont()).ColorAndOpacity(HudStyle::TextColor())
                .ShadowOffset(FVector2D(1, 1)).ShadowColorAndOpacity(HudStyle::ShadowColor)
                .OverflowPolicy(ETextOverflowPolicy::Ellipsis)
                .Text_Lambda([this] { return FText::FromString(Online.IsValid() ? Online->GetDisplayName() : FString()); })
            ]
        ]
    ];
    // clang-format on
}
