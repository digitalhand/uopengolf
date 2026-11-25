// SPDX-License-Identifier: MIT
#include "UI/Menu/RoundSetup.h"
#include "Course/RoundConditions.h"
#include "GolfCourseDefinition.h"
#include "Engine/Texture2D.h"
#include "Widgets/Colors/SSimpleGradient.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SOverlay.h"
#include "OpenGolfAccountStyle.h"
#include "OpenGolfOnlineSubsystem.h"
#include "OpenGolfSettings.h"
#include "Profile/AvatarCatalog.h"
#include "UI/Common/AvatarImage.h"
#include "UI/Common/ChoiceChips.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/HudSwitch.h"
#include "UI/Common/MenuStyle.h"
#include "UI/Common/PageHeader.h"
#include "UI/Menu/MainMenuInternal.h"
#include "UI/Menu/RoundSetupPanels.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"

using namespace OpenGolfPlay;
using MainMenuDetail::MakeText;

const FName SRoundSetup::TabTag(TEXT("RoundTab"));
const FName SRoundSetup::PlayerTag(TEXT("RoundPlayer"));
const FName SRoundSetup::PlayerAvatarTag(TEXT("RoundPlayerAvatar"));
const FName SRoundSetup::HoleSetTag(TEXT("RoundHoles"));
const FName SRoundSetup::GimmieTag(TEXT("RoundGimmie"));
const FName SRoundSetup::GimmieSwitchTag(TEXT("RoundGimmieSwitch"));
const FName SRoundSetup::TeesTag(TEXT("RoundTees"));
const FName SRoundSetup::FasterTag(TEXT("RoundStimpFaster"));
const FName SRoundSetup::SlowerTag(TEXT("RoundStimpSlower"));
const FName SRoundSetup::StartTag(TEXT("RoundStart"));
const FName SRoundSetup::ResetTag(TEXT("RoundReset"));

namespace RoundSetupDetail
{
    /** Controls sit under their label, up to this wide; chips and hole boxes wrap inside it on a narrow page. */
    constexpr float ControlWidth = 320.0f;
    constexpr float HoleBoxWidth = 44.0f;
    constexpr float HoleBoxGap = 4.0f;
    /** Minimum space for settings alongside the course facts. */
    constexpr float MainWidth = 560.0f;
    constexpr float SidebarWidth = 320.0f;
    constexpr float ColumnGap = 24.0f;
    /** Below this, the sidebar wraps under the tabs and both take the full width. */
    constexpr float SideBySideWidth = MainWidth + ColumnGap + SidebarWidth;
    const FLinearColor Pine = FLinearColor::FromSRGBColor(FColor(19, 44, 37));
    constexpr float AvatarSize = 64.0f;
} // namespace RoundSetupDetail

FName SRoundSetup::HoleTag(int32 Number)
{
    return FName(*FString::Printf(TEXT("RoundHole.%d"), Number));
}

void SRoundSetup::Construct(const FArguments& Args)
{
    using namespace RoundSetupDetail;
    checkf(Args._Style && Args._Course, TEXT("SRoundSetup needs the menu's style and a course"));
    Style = Args._Style;
    Course = Args._Course;
    Online = Args._Online;
    OnStart = Args._OnStart;
    OnBack = Args._OnBack;
    HoleCount = RoundConditions::HoleCount(*Args._Course);
    TeeCount = RoundConditions::TeeCount(*Args._Course);
    Setup = Args._Setup;
    Setup.Validate(HoleCount, TeeCount);
    Defaults = Args._Defaults;
    Defaults.Validate(HoleCount, TeeCount);

    // clang-format off
    const TSharedRef<SWidget> Settings = SNew(SBorder).Tag(TEXT("RoundSettings"))
        .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Pine).Padding(20)
        [
            SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return static_cast<int32>(Tab); })
            + SWidgetSwitcher::Slot()[MakePlayers()]
            + SWidgetSwitcher::Slot()[MakeMatch()]
            + SWidgetSwitcher::Slot()[MakeCourse()]
        ];
    SAssignNew(Columns, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(ColumnGap, 12))
        + SWrapBox::Slot().VAlign(VAlign_Top).FillLineWhenSizeLessThan(SideBySideWidth)
        [
            SNew(SBox).WidthOverride_Lambda([this]
            {
                const float Width = Columns ? Columns->GetTickSpaceGeometry().GetLocalSize().X : 0.f;
                return FOptionalSize(FMath::Max(MainWidth, FMath::FloorToFloat(Width - ColumnGap - SidebarWidth)));
            })[Settings]
        ]
        + SWrapBox::Slot().VAlign(VAlign_Top).FillLineWhenSizeLessThan(SideBySideWidth)
        [
            SNew(SBox).WidthOverride(SidebarWidth)[MakeSidebar()]
        ];
    ChildSlot.Padding(TAttribute<FMargin>::CreateLambda([this]
    {
        const float Margin = FMath::Max(0.f, (float(GetTickSpaceGeometry().GetLocalSize().X) - 1440.f) * 0.5f);
        return FMargin(Margin, 0);
    }))
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[MakeHeader(*Args._Course)]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 12)[MakeTabs()]
        + SVerticalBox::Slot().FillHeight(1)
        [
            SAssignNew(Scroll, SScrollBox).Clipping(EWidgetClipping::ClipToBounds)
            .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
            + SScrollBox::Slot()[Columns.ToSharedRef()]
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[MakeFooter()]
    ];
    // clang-format on
}

TSharedRef<SWidget> SRoundSetup::MakeHeader(const UGolfCourseDefinition& Picked)
{
    using namespace RoundSetupDetail;
    const TSharedRef<SOverlay> Banner = SNew(SOverlay).Clipping(EWidgetClipping::ClipToBounds);
    if (UTexture2D* Texture = Picked.Preview.Get())
    {
        PreviewBrush.SetResourceObject(Texture);
        PreviewBrush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
        Banner->AddSlot()
        [
            SNew(SBox).HeightOverride(0).WidthOverride(0)
            [
                SNew(SScaleBox).Stretch(EStretch::ScaleToFill)
                [ SNew(SImage).Image(&PreviewBrush) ]
            ]
        ];
    }
    const FLinearColor Shade = FLinearColor::FromSRGBColor(FColor(10, 30, 23, 246));
    Banner->AddSlot()
    [
        SNew(SSimpleGradient).Orientation(Orient_Vertical).StartColor(Shade)
        .EndColor(GolfSettings::Get().HighContrast ? Shade : FLinearColor(0, 0, 0, 0.05f))
        .Visibility(EVisibility::HitTestInvisible)
    ];
    const TSharedRef<SButton> Back = MainMenuDetail::MakeButton(*Style, FText::FromString(TEXT("Back to courses")),
        [this] { OnBack.ExecuteIfBound(); }, false, MenuStyle::SmallSize);
    Back->SetTag(PageHeader::BackTag());
    Banner->AddSlot().VAlign(VAlign_Bottom).Padding(20, 14)
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left)[Back]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
        [
            SNew(STextBlock).Text(Picked.DisplayName).AutoWrapText(true)
            .Font_Lambda([] { return HudStyle::Font("Bold", 36); }).ColorAndOpacity(HudStyle::TextColor())
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock).Text(Picked.Location).AutoWrapText(true)
            .Visibility(Picked.Location.IsEmptyOrWhitespace() ? EVisibility::Collapsed : EVisibility::Visible)
            .Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); }).ColorAndOpacity(HudStyle::TextColor())
        ]
    ];
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(Pine).Padding(0)
        [
            SNew(SBox).MinDesiredHeight_Lambda([this]
            {
                return FOptionalSize(FMath::Clamp(float(GetTickSpaceGeometry().GetLocalSize().Y) * 0.24f, 96.f, 230.f));
            })[Banner]
        ];
}

bool SRoundSetup::CanStart() const
{
    return HoleCount == 0 || !Setup.Holes.IsEmpty();
}

void SRoundSetup::AddRow(const TSharedRef<SVerticalBox>& Rows, const FString& Label, const FString& Help, const TSharedRef<SWidget>& Control)
{
    // clang-format off
    Rows->AddSlot().AutoHeight()[SNew(SSeparator).ColorAndOpacity(HudStyle::MapEdgeColor())];
    Rows->AddSlot().AutoHeight().Padding(0, 10)
    [
        SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(16, 8))
        + SWrapBox::Slot().VAlign(VAlign_Center).FillLineWhenSizeLessThan(548.f)
        [
            SNew(SBox).WidthOverride(212)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[MakeText(FText::FromString(Label), MenuStyle::BodySize)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 2, 0, 0)[MakeText(FText::FromString(Help), MenuStyle::HelpSize, true)]
            ]
        ]
        + SWrapBox::Slot().VAlign(VAlign_Center).FillEmptySpace(true).FillLineWhenSizeLessThan(548.f)
        [ SNew(SBox).WidthOverride(RoundSetupDetail::ControlWidth)[Control] ]
    ];
    // clang-format on
}

void SRoundSetup::AddHeading(const TSharedRef<SVerticalBox>& Rows, const FString& Heading)
{
    Rows->AddSlot().AutoHeight().Padding(2, 4, 0, 8)[SNew(STextBlock).Text(FText::FromString(Heading))
        .Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); }).ColorAndOpacity(HudStyle::TextColor())];
}

TSharedRef<SWidget> SRoundSetup::MakeTabs()
{
    return SNew(SChoiceChips).Style(Style).ChipTag(TabTag)
        .Labels({FText::FromString(TEXT("Players")), FText::FromString(TEXT("Match")), FText::FromString(TEXT("Course"))})
        .Selected_Lambda([this] { return static_cast<int32>(Tab); })
        .OnSelected_Lambda([this](int32 Index)
        {
            Tab = static_cast<ETab>(Index);
            Scroll->ScrollToStart();
        });
}

TSharedRef<SWidget> SRoundSetup::MakePlayers()
{
    const auto SignedIn = [this] { return Online.IsValid() && Online->HasSession(); };
    const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    AddHeading(Rows, TEXT("Playing this round"));
    Rows->AddSlot().AutoHeight().Padding(0, 8)
    [
        SNew(SHorizontalBox).Tag(PlayerTag)
        .Visibility_Lambda([SignedIn] { return SignedIn() ? EVisibility::Visible : EVisibility::Collapsed; })
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(SAvatarImage).Tag(PlayerAvatarTag).Size(RoundSetupDetail::AvatarSize)
            .Texture_Lambda([this] { return PlayerAvatar::Texture(GolfSettings::Get(), Online.Get()); })
        ]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(16, 0, 0, 0)
        [
            SNew(STextBlock).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); }).AutoWrapText(true)
            .ColorAndOpacity(HudStyle::TextColor())
            .Text_Lambda([this] { return FText::FromString(Online.IsValid() ? Online->GetDisplayName() : FString()); })
        ]
    ];
    Rows->AddSlot().AutoHeight()
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("You can play this round without signing in."))).AutoWrapText(true)
        .Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); }).ColorAndOpacity(HudStyle::TextColor())
        .Visibility_Lambda([SignedIn] { return SignedIn() ? EVisibility::Collapsed : EVisibility::Visible; })
    ];
    return Rows;
}

TSharedRef<SWidget> SRoundSetup::MakeMatch()
{
    const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    AddHeading(Rows, TEXT("Putting"));
    AddRow(Rows, TEXT("Gimmie"), TEXT("On the green, a ball inside the ring adds one conceded putt."), MakeGimmieSwitch());
    AddRow(Rows, TEXT("Gimmie radius"), TEXT("How close to the cup counts as conceded."), MakeGimmieRadius());
    return Rows;
}

TSharedRef<SWidget> SRoundSetup::MakeCourse()
{
    const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    AddHeading(Rows, TEXT("Course options"));
    AddRow(Rows, TEXT("Green speed"), TEXT("Stimp for every green this round; fairways keep Game Defaults."), MakeGreenSpeed());
    if (TeeCount > 1)
        AddRow(Rows, TEXT("Tees"), TEXT("The tee set every hole plays from."), MakeTees());
    if (HoleCount > 0)
    {
        AddHeading(Rows, TEXT("Holes"));
        Rows->AddSlot().AutoHeight()[MakeHoles()];
    }
    return Rows;
}

FName SRoundSetup::TeeSet() const
{
    const UGolfCourseDefinition* Picked = Course.Get();
    return Picked ? RoundConditions::TeeSetName(*Picked, Setup.TeeIndex) : NAME_None;
}

TSharedRef<SWidget> SRoundSetup::MakeTees()
{
    // One chip per tee set, in the scorecard's order; the sidebar's Total distance follows the pick.
    TArray<FText> Labels;
    if (const UGolfCourseDefinition* Picked = Course.Get())
        for (const FGolfCourseTee& Tee : Picked->Layout[0].Tees)
            Labels.Add(FText::FromName(Tee.SetName));
    return SNew(SChoiceChips).Style(Style).Labels(Labels).ChipTag(TeesTag)
        .Selected_Lambda([this] { return Setup.TeeIndex; })
        .OnSelected_Lambda([this](int32 Index) { Setup.TeeIndex = FMath::Clamp(Index, 0, FMath::Max(TeeCount - 1, 0)); });
}

TSharedRef<SWidget> SRoundSetup::MakeGreenSpeed()
{
    auto Step = [this](int32 Feet) { Setup.GreenStimpFeet = FMath::Clamp(Setup.GreenStimpFeet + Feet, MinRoundStimpFeet, MaxRoundStimpFeet); };
    const TSharedRef<SButton> Slower = MainMenuDetail::MakeButton(*Style, FText::FromString(TEXT("Slower")), [Step] { Step(-1); }, false, MenuStyle::NavSize);
    const TSharedRef<SButton> Faster = MainMenuDetail::MakeButton(*Style, FText::FromString(TEXT("Faster")), [Step] { Step(1); }, false, MenuStyle::NavSize);
    Slower->SetTag(SlowerTag);
    Faster->SetTag(FasterTag);
    // clang-format off
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth()[Slower]
        + SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(STextBlock).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); }).ColorAndOpacity(HudStyle::TextColor())
            .Text_Lambda([this] { return FText::FromString(GreenSpeedLabel(Setup.GreenStimpFeet)); })
        ]
        + SHorizontalBox::Slot().AutoWidth()[Faster];
    // clang-format on
}

TSharedRef<SWidget> SRoundSetup::MakeGimmieSwitch()
{
    // clang-format off
    return SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
        [
            SNew(SHudSwitch).Tag(GimmieSwitchTag).IsOn_Lambda([this] { return Setup.bGimmie; }).OnToggled_Lambda([this] { Setup.bGimmie = !Setup.bGimmie; })
        ]
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
        [
            SNew(STextBlock).Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); }).ColorAndOpacity(HudStyle::LabelColor())
            .Text_Lambda([this] { return FText::FromString(Setup.bGimmie ? TEXT("On") : TEXT("Off")); })
        ];
    // clang-format on
}

TSharedRef<SWidget> SRoundSetup::MakeGimmieRadius()
{
    return SNew(SChoiceChips).Style(Style).ChipTag(GimmieTag).IsEnabled_Lambda([this] { return Setup.bGimmie; })
        .Labels({FText::FromString(FString::Printf(TEXT("%d ft"), ShortGimmieFeet)), FText::FromString(FString::Printf(TEXT("%d ft"), LongGimmieFeet))})
        .Selected_Lambda([this] { return Setup.GimmieFeet == LongGimmieFeet ? 1 : 0; })
        .OnSelected_Lambda([this](int32 Index) { Setup.GimmieFeet = Index == 1 ? LongGimmieFeet : ShortGimmieFeet; });
}

int32 SRoundSetup::HoleSetIndex() const
{
    // The chips are All, then Front and Back where the course has nines, then Custom.
    const EOpenGolfHoleSet Set = DetectHoleSet(Setup.Holes, HoleCount);
    if (!HasNines(HoleCount))
        return Set == EOpenGolfHoleSet::All ? 0 : 1;
    return static_cast<int32>(Set);
}

void SRoundSetup::ToggleHole(int32 Number)
{
    if (Setup.Holes.Remove(Number) == 0)
    {
        Setup.Holes.Add(Number);
        Setup.Holes.Sort();
    }
}

TSharedRef<SWidget> SRoundSetup::MakeHoles()
{
    using namespace RoundSetupDetail;
    TArray<FText> Labels = {FText::FromString(FString::Printf(TEXT("All %d"), HoleCount))};
    TArray<EOpenGolfHoleSet> Sets = {EOpenGolfHoleSet::All};
    if (HasNines(HoleCount))
    {
        Labels.Append({FText::FromString(TEXT("Front 9")), FText::FromString(TEXT("Back 9"))});
        Sets.Append({EOpenGolfHoleSet::Front, EOpenGolfHoleSet::Back});
    }
    Labels.Add(FText::FromString(TEXT("Custom")));
    Sets.Add(EOpenGolfHoleSet::Custom);

    // Nine boxes to a line where the page is wide enough, fewer on a narrow one; each keeps its own width.
    const TSharedRef<SWrapBox> Grid = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(HoleBoxGap, HoleBoxGap));
    const float CellWidth = HoleBoxWidth * GolfSettings::Get().TextPercent / 100.f;
    for (int32 Number = 1; Number <= HoleCount; ++Number)
    {
        // clang-format off
        Grid->AddSlot()
        [
            SNew(SBox).WidthOverride(CellWidth)
            [
                SNew(SCheckBox).Tag(HoleTag(Number))
                .IsChecked_Lambda([this, Number] { return Setup.Holes.Contains(Number) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([this, Number](ECheckBoxState) { ToggleHole(Number); })
                [
                    SNew(STextBlock).Text(FText::AsNumber(Number)).Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::SmallSize); })
                    .ColorAndOpacity(HudStyle::TextColor())
                ]
            ]
        ];
        // clang-format on
    }
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SChoiceChips).Style(Style).Labels(Labels).ChipTag(HoleSetTag)
            .Selected_Lambda([this] { return HoleSetIndex(); })
            // Custom keeps the ticks; the grid below changes them.
            .OnSelected_Lambda([this, Sets](int32 Index) { if (Sets[Index] != EOpenGolfHoleSet::Custom) Setup.Holes = HolesFor(Sets[Index], HoleCount); })
        ]
        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0, 8, 0, 0)
        [
            SNew(SBox).WidthOverride(HolesPerNine * CellWidth + (HolesPerNine - 1) * HoleBoxGap)[Grid]
        ];
    // clang-format on
}

FText SRoundSetup::HoleSummary() const
{
    const UGolfCourseDefinition* Picked = Course.Get();
    if (!Picked)
        return FText::GetEmpty();
    if (!CanStart())
        return FText::FromString(TEXT("Pick at least one hole"));
    if (HoleCount == 0)
        return FText::FromString(FString::Printf(TEXT("1 hole · Par %d · %s"), RoundConditions::CoursePar(*Picked),
            *GolfSettings::RoundDistance(RoundConditions::CourseLengthM(*Picked, TeeSet()))));
    const int32 Count = Setup.Holes.Num();
    return FText::FromString(FString::Printf(TEXT("%d %s · Par %d · %s"), Count, Count == 1 ? TEXT("hole") : TEXT("holes"),
        RoundConditions::Par(*Picked, Setup.Holes), *GolfSettings::RoundDistance(RoundConditions::Yards(*Picked, Setup.Holes, TeeSet()) * RoundConditions::MetersPerYard)));
}

TSharedRef<SWidget> SRoundSetup::MakeSidebar()
{
    const UGolfCourseDefinition* Picked = Course.Get();
    // clang-format off
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, MenuStyle::RowGap)
        [
            SNew(SCourseInformation).Tag(TEXT("RoundFacts")).Style(Style).Course(Picked).TeeSet_Lambda([this] { return TeeSet(); })
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SCourseConditions).Style(Style).Course(Picked).Setup_Lambda([this] { return Setup; })
        ];
    // clang-format on
}

TSharedRef<SWidget> SRoundSetup::MakeFooter()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()[SNew(SSeparator).ColorAndOpacity(HudStyle::MapEdgeColor())]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center).Padding(0, 0, 8, 0)
            [
                SNew(STextBlock).AutoWrapText(true).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::NavSize); })
                .ColorAndOpacity(HudStyle::TextColor()).Text(this, &SRoundSetup::HoleSummary)
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 8, 0)
            [
                SNew(SButton).ButtonStyle(&Style->Button).HAlign(HAlign_Center).Tag(ResetTag)
                .OnClicked_Lambda([this] { Setup = Defaults; return FReply::Handled(); })
                [MakeText(FText::FromString(TEXT("Reset to defaults")), MenuStyle::NavSize)]
            ]
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SAssignNew(StartButton, SButton).ButtonStyle(&Style->PrimaryButton).HAlign(HAlign_Center).Tag(StartTag)
                .IsEnabled_Lambda([this] { return CanStart(); })
                .OnClicked_Lambda([this]
                {
                    if (CanStart())
                    {
                        FOpenGolfRoundSetup Picked = Setup;
                        Picked.Validate(HoleCount, TeeCount);
                        OnStart.ExecuteIfBound(Picked);
                    }
                    return FReply::Handled();
                })
                [
                    SNew(STextBlock).Text(FText::FromString(TEXT("Start round"))).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); })
                    .ColorAndOpacity(Style->PrimaryLabelColor)
                ]
            ]
        ];
}
