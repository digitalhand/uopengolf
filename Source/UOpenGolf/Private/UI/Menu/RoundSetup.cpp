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
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SUniformWrapPanel.h"
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
const FName SRoundSetup::GreenSpeedTag(TEXT("RoundGreenSpeed"));
const FName SRoundSetup::StartTag(TEXT("RoundStart"));
const FName SRoundSetup::ResetTag(TEXT("RoundReset"));

namespace RoundSetupDetail
{
    /** Controls sit under their label, up to this wide; chips and hole boxes wrap inside it on a narrow page. */
    constexpr float ControlWidth = 320.0f;
    constexpr float HoleBoxWidth = 44.0f;
    constexpr float HoleBoxGap = 4.0f;
    constexpr float CardWidth = 224.0f;
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
    Style = *Args._Style;
    const FLinearColor Accent = Style.AccentColor;
    auto AccentFill = [Accent](float Alpha) { return Accent.CopyWithNewOpacity(Alpha); };
    PlayerBorder = FSlateRoundedBoxBrush(AccentFill(0.035f), MenuStyle::ControlRadius, AccentFill(0.8f), 1.0f);
    SeatBorder = FSlateRoundedBoxBrush(FLinearColor::Transparent, MenuStyle::ControlRadius, HudStyle::MapEdgeColor(), 1.0f);
    Style.Button.SetNormal(SeatBorder);
    Style.PrimaryButton = Style.Button;
    Style.PrimaryButton.SetNormal(FSlateRoundedBoxBrush(AccentFill(0.08f), MenuStyle::ControlRadius, Accent, 1.5f))
        .SetHovered(FSlateRoundedBoxBrush(AccentFill(0.18f), MenuStyle::ControlRadius, Accent, 1.5f))
        .SetPressed(FSlateRoundedBoxBrush(AccentFill(0.26f), MenuStyle::ControlRadius, Accent, 1.5f));
    Style.PrimaryLabelColor = Style.TextColor;
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
        .BorderImage(&Style.Section).Padding(16)
        [
            SNew(SWidgetSwitcher).WidgetIndex_Lambda([this] { return static_cast<int32>(Tab); })
            + SWidgetSwitcher::Slot()[MakePlayers()]
            + SWidgetSwitcher::Slot()[MakeMatch()]
            + SWidgetSwitcher::Slot()[MakeCourse()]
        ];
    const auto Columns = SNew(SClubHouseColumns, Settings, MakeSidebar()).Tag(TEXT("ClubHouseColumns"));
    const auto ScrollBar = SNew(SScrollBar).Thickness(FVector2D(5, 5));
    ChildSlot.HAlign(HAlign_Center)
    [
        SNew(SBox).WidthOverride(1440)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [PageHeader::Make(Style, FText::FromString(TEXT("Club House")), OnBack)]
            + SVerticalBox::Slot().FillHeight(0.30f).MinHeight(104).MaxHeight(224)[MakeHeader(*Args._Course)]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 12)[MakeTabs()]
            + SVerticalBox::Slot().FillHeight(1)
            [
                SNew(SHorizontalBox)
                + SHorizontalBox::Slot().FillWidth(1)
                [
                    SAssignNew(Scroll, SScrollBox).ExternalScrollbar(ScrollBar).Clipping(EWidgetClipping::ClipToBounds)
                    .ScrollWhenFocusChanges(EScrollWhenFocusChanges::InstantScroll)
                    + SScrollBox::Slot()[Columns]
                ]
                + SHorizontalBox::Slot().AutoWidth().Padding(4, 0, 0, 0)
                [SNew(SBox).WidthOverride(8)[ScrollBar]]
            ]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 0)[MakeFooter()]
        ]
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
    const float Cover = GolfSettings::Get().HighContrast ? 0.90f : 0.74f;
    Banner->AddSlot()
    [
        SNew(SSimpleGradient).Orientation(Orient_Horizontal).StartColor(FLinearColor(0.012f, 0.024f, 0.018f, Cover))
        .EndColor(FLinearColor(0, 0, 0, 0.08f)).Visibility(EVisibility::HitTestInvisible)
    ];
    Banner->AddSlot().VAlign(VAlign_Bottom).Padding(20, 12)
    [
        SNew(SVerticalBox)
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
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(HudStyle::MapPanelColor()).Padding(0)
        [
            SNew(SBox).MinDesiredHeight(96)[Banner]
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
    return SNew(SChoiceChips).Style(&Style).ChipTag(TabTag)
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
    using namespace RoundSetupDetail;
    const auto PlayerName = [this]
    {
        return Online.IsValid() && Online->HasSession() ? Online->GetDisplayName() : FString(TEXT("Guest"));
    };
    const auto Avatar = SNew(SAvatarImage).Tag(PlayerAvatarTag).Size(AvatarSize)
        .Texture_Lambda([this] { return PlayerAvatar::Texture(GolfSettings::Get(), Online.Get()); });
    const auto Details = SNew(SVerticalBox);
    Details->AddSlot().AutoHeight().HAlign(HAlign_Center)
    [
        SNew(SOverlay)
        + SOverlay::Slot()[Avatar]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)
        [
            SNew(STextBlock).ColorAndOpacity(Style.TextColor).Font_Lambda([] { return HudStyle::Font("Bold", 24); })
            .Visibility_Lambda([Avatar] { return Avatar->GetShownTexture() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
            .Text_Lambda([PlayerName] { return FText::FromString(PlayerName().Left(1).ToUpper()); })
        ]
    ];
    Details->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0, 8)
    [
        SNew(STextBlock).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); })
        .ColorAndOpacity(Style.TextColor).AutoWrapText(true).Justification(ETextJustify::Center)
        .Text_Lambda([PlayerName] { return FText::FromString(PlayerName()); })
    ];
    if (TeeCount > 0)
    {
        Details->AddSlot().AutoHeight().Padding(0, 6, 0, 4)[MakeText(FText::FromString(TEXT("Tees")), MenuStyle::SmallSize, true)];
        Details->AddSlot().AutoHeight()[MakeTees()];
    }
    return SNew(SUniformWrapPanel).SlotPadding(FMargin(0, 0, 10, 8)).MinDesiredSlotWidth(CardWidth).MaxDesiredSlotWidth(300)
        .HAlign(HAlign_Fill)
        + SUniformWrapPanel::Slot()
        [ SNew(SBorder).Tag(PlayerTag).BorderImage(&PlayerBorder).Padding(16)[Details] ]
        + SUniformWrapPanel::Slot()
        [
            SNew(SBorder).BorderImage(&SeatBorder).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(16)
            [
                SNew(STextBlock).Text(FText::FromString(TEXT("Open seat"))).ColorAndOpacity(Style.LabelColor)
                .Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); })
            ]
        ];
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
    AddRow(Rows, TEXT("Green speed"), TEXT("Distance a Stimp putt rolls on a level green."), MakeGreenSpeed());
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
    auto Label = [this](FName Set)
    {
        const UGolfCourseDefinition* Picked = Course.Get();
        if (!Picked)
            return FText::GetEmpty();
        return FText::FromString(Set.ToString() + TEXT(" · ") + GolfSettings::RoundDistance(RoundConditions::CourseLengthM(*Picked, Set)));
    };
    if (TeeCount == 1)
        return MakeText(Label(TeeSet()), MenuStyle::NavSize);
    for (int32 Index = 0; Index < TeeCount; ++Index)
        TeeOptions.Add(RoundConditions::TeeSetName(*Course.Get(), Index));
    return SAssignNew(TeePicker, SComboBox<FName>).Tag(TeesTag).OptionsSource(&TeeOptions)
        .InitiallySelectedItem(TeeOptions[Setup.TeeIndex]).ButtonStyle(&Style.Button).ForegroundColor(Style.TextColor)
        .OnGenerateWidget_Lambda([Label](FName Set) { return MakeText(Label(Set), MenuStyle::NavSize); })
        .OnSelectionChanged_Lambda([this](FName Set, ESelectInfo::Type)
        {
            const int32 Index = TeeOptions.Find(Set);
            if (Index != INDEX_NONE) Setup.TeeIndex = Index;
        })
        [
            SNew(STextBlock).Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); })
            .AutoWrapText(true).ColorAndOpacity(Style.TextColor).Text_Lambda([this, Label] { return Label(TeeSet()); })
        ];
}

TSharedRef<SWidget> SRoundSetup::MakeGreenSpeed()
{
    const FSlateRoundedBoxBrush Track(HudStyle::LabelColor().CopyWithNewOpacity(.55f), 2.f);
    const FSlateRoundedBoxBrush Thumb(Style.AccentColor, 8.f, FVector2f(16.f, 16.f));
    SliderStyle = FSliderStyle::GetDefault();
    SliderStyle.SetNormalBarImage(Track).SetHoveredBarImage(Track).SetBarThickness(4.f)
        .SetNormalThumbImage(Thumb).SetHoveredThumbImage(FSlateRoundedBoxBrush(Style.TextColor, 8.f, FVector2f(16.f, 16.f)));
    return SNew(SVerticalBox)
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(STextBlock).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); }).ColorAndOpacity(Style.TextColor)
            .Text_Lambda([this] { return FText::FromString(GreenSpeedLabel(Setup.GreenStimpFeet)); })
        ]
        + SVerticalBox::Slot().AutoHeight().Padding(0, 4)
        [
            SNew(SSlider).Tag(GreenSpeedTag).MinValue(MinRoundStimpFeet).MaxValue(MaxRoundStimpFeet)
            .Style(&SliderStyle).StepSize(1.f).RequiresControllerLock(false)
#if WITH_ACCESSIBILITY
            .AccessibleText(FText::FromString(TEXT("Green speed")))
#endif
            .Value_Lambda([this] { return float(Setup.GreenStimpFeet); })
            .OnValueChanged_Lambda([this](float Feet) { Setup.GreenStimpFeet = FMath::Clamp(FMath::RoundToInt(Feet), MinRoundStimpFeet, MaxRoundStimpFeet); })
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[MakeText(FText::FromString(FString::Printf(TEXT("%d ft"), MinRoundStimpFeet)), MenuStyle::SmallSize, true)]
            + SHorizontalBox::Slot().AutoWidth()[MakeText(FText::FromString(FString::Printf(TEXT("%d ft"), MaxRoundStimpFeet)), MenuStyle::SmallSize, true)]
        ];
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
    return SNew(SChoiceChips).Style(&Style).ChipTag(GimmieTag).IsEnabled_Lambda([this] { return Setup.bGimmie; })
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
            SNew(SChoiceChips).Style(&Style).Labels(Labels).ChipTag(HoleSetTag)
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
            SNew(SCourseInformation).Tag(TEXT("RoundFacts")).Style(&Style).Course(Picked).TeeSet_Lambda([this] { return TeeSet(); })
        ]
        + SVerticalBox::Slot().AutoHeight()
        [
            SNew(SCourseConditions).Style(&Style).Course(Picked).Setup_Lambda([this] { return Setup; })
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
            + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
            [
                SNew(SUniformGridPanel).SlotPadding(FMargin(4, 0))
                + SUniformGridPanel::Slot(0, 0)
                [
                    SNew(SButton).ButtonStyle(&Style.Button).HAlign(HAlign_Center).VAlign(VAlign_Center).Tag(ResetTag)
                    .OnClicked_Lambda([this]
                    {
                        Setup = Defaults;
                        if (TeePicker) TeePicker->SetSelectedItem(TeeOptions[Setup.TeeIndex]);
                        return FReply::Handled();
                    })
                    [MakeText(FText::FromString(TEXT("Reset")), MenuStyle::BodySize)]
                ]
                + SUniformGridPanel::Slot(1, 0)
                [
                    SAssignNew(StartButton, SButton).ButtonStyle(&Style.PrimaryButton).HAlign(HAlign_Center).VAlign(VAlign_Center).Tag(StartTag)
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
                        .ColorAndOpacity(Style.PrimaryLabelColor)
                    ]
                ]
            ]
        ];
}
