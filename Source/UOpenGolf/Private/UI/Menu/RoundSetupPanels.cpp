// SPDX-License-Identifier: MIT
#include "UI/Menu/RoundSetupPanels.h"
#include "Course/RoundConditions.h"
#include "GolfCourseDefinition.h"
#include "OpenGolfAccountStyle.h"
#include "OpenGolfSettings.h"
#include "Layout/ArrangedChildren.h"
#include "Play/OpenGolfWindSetting.h"
#include "UI/Common/HudStyle.h"
#include "UI/Common/MenuStyle.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

using namespace OpenGolfPlay;

const FName SCourseConditions::SpeedTag(TEXT("CourseConditions.Speed"));
const FName SCourseConditions::WindTag(TEXT("CourseConditions.Wind"));

namespace RoundSetupPanelsDetail
{
    constexpr float ArrowSize = 32.0f;
    constexpr float RowGap = 6.0f;

    /** An arrow the way the wind blows with the hole ahead (up), drawn so it needs no arrow glyphs in the font. */
    class SWindArrow : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SWindArrow) : _FromDeg(0.0f), _Color(FLinearColor::White) {}
        /** Where the wind comes from against the hole: 0 headwind, 90 from the right. */
        SLATE_ATTRIBUTE(float, FromDeg)
        SLATE_ARGUMENT(FLinearColor, Color)
        SLATE_END_ARGS()

        void Construct(const FArguments& Args)
        {
            FromDeg = Args._FromDeg;
            Color = Args._Color;
        }

        virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(ArrowSize, ArrowSize); }

        virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Elements, int32 Layer,
            const FWidgetStyle& WidgetStyle, bool) const override
        {
            // Clockwise from ahead (screen up), a wind from FromDeg blows toward FromDeg + 180.
            const float Radians = FMath::DegreesToRadians(FromDeg.Get());
            const FVector2f Direction(-FMath::Sin(Radians), FMath::Cos(Radians));
            const FVector2f Center(ArrowSize * 0.5f, ArrowSize * 0.5f);
            const FVector2f Side(-Direction.Y, Direction.X);
            const FVector2f Tip = Center + Direction * 12.0f;
            const FLinearColor Tint = Color * WidgetStyle.GetColorAndOpacityTint();
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(), TArray<FVector2f>{Center - Direction * 11.0f, Tip},
                ESlateDrawEffect::None, Tint, true, 2.5f);
            FSlateDrawElement::MakeLines(Elements, Layer, Geometry.ToPaintGeometry(),
                TArray<FVector2f>{Center + Direction * 3.0f + Side * 7.0f, Tip, Center + Direction * 3.0f - Side * 7.0f}, ESlateDrawEffect::None, Tint, true, 2.5f);
            return Layer;
        }

    private:
        TAttribute<float> FromDeg;
        FLinearColor Color = FLinearColor::White;
    };

    /** A fact's label on the left and its value on the right; the value wraps when it is long (a pair of designers). */
    TSharedRef<SWidget> FactRow(const TCHAR* Label, FName ValueTag, const TAttribute<FText>& Value, const TAttribute<EVisibility>& Visibility)
    {
        // clang-format off
        return SNew(SHorizontalBox).Visibility(Visibility)
            + SHorizontalBox::Slot().FillWidth(0.42f).VAlign(VAlign_Top).Padding(0, 0, 8, 0)
            [
                SNew(STextBlock).Text(FText::FromString(Label)).AutoWrapText(true).Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); })
                .ColorAndOpacity(HudStyle::LabelColor())
            ]
            + SHorizontalBox::Slot().FillWidth(0.58f).VAlign(VAlign_Top)
            [
                SNew(STextBlock).Tag(ValueTag).Text(Value).Justification(ETextJustify::Right).AutoWrapText(true)
                .Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::NavSize); }).ColorAndOpacity(HudStyle::TextColor())
            ];
        // clang-format on
    }
} // namespace RoundSetupPanelsDetail

void SClubHouseColumns::Construct(const FArguments&, const TSharedRef<SWidget>& Settings, const TSharedRef<SWidget>& Sidebar)
{
    Children.Add(Settings);
    Children.Add(Sidebar);
    SetCanTick(true);
}

bool SClubHouseColumns::FitsSideBySide(const FGeometry& Geometry)
{
    // Small windows also scale down their UI. Keep the columns readable at either scale.
    return Geometry.GetLocalSize().X * FMath::Min(1.f, Geometry.GetAccumulatedLayoutTransform().GetScale()) >= SideBySideWidth;
}

void SClubHouseColumns::OnArrangeChildren(const FGeometry& Geometry, FArrangedChildren& Arranged) const
{
    const float Width = Geometry.GetLocalSize().X;
    const bool bWide = FitsSideBySide(Geometry);
    const float SidebarWidth = bWide ? FMath::Clamp(Width / 3.f, 320.f, 460.f) : Width;
    const float SettingsWidth = bWide ? Width - SidebarWidth - 16.f : Width;
    const auto Settings = Children[0];
    const auto Sidebar = Children[1];
    const float SettingsHeight = Settings->GetDesiredSize().Y;
    const FVector2D SidebarPosition = bWide ? FVector2D(SettingsWidth + 16.f, 0) : FVector2D(0, SettingsHeight + 12.f);
    Arranged.AddWidget(Geometry.MakeChild(Settings, FVector2D::ZeroVector, FVector2D(SettingsWidth, SettingsHeight)));
    Arranged.AddWidget(Geometry.MakeChild(Sidebar, SidebarPosition, FVector2D(SidebarWidth, Sidebar->GetDesiredSize().Y)));
}

FVector2D SClubHouseColumns::ComputeDesiredSize(float) const
{
    const float SettingsHeight = Children[0]->GetDesiredSize().Y;
    const float SidebarHeight = Children[1]->GetDesiredSize().Y;
    return FVector2D(0, bSideBySide ? FMath::Max(SettingsHeight, SidebarHeight) : SettingsHeight + 12.f + SidebarHeight);
}

void SClubHouseColumns::Tick(const FGeometry& Geometry, double Time, float Delta)
{
    SPanel::Tick(Geometry, Time, Delta);
    const bool bWide = FitsSideBySide(Geometry);
    if (bWide != bSideBySide)
    {
        bSideBySide = bWide;
        Invalidate(EInvalidateWidgetReason::Layout);
    }
}

TSharedRef<SWidget> RoundSetupPanels::Section(const FOpenGolfAccountStyle& Style, const FText& Heading, const TSharedRef<SWidget>& Content)
{
    // clang-format off
    return SNew(SBorder).BorderImage(&Style.Section).Padding(MenuStyle::RowPadding)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 10)
            [
                SNew(STextBlock).Text(Heading).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); }).ColorAndOpacity(HudStyle::TextColor())
            ]
            + SVerticalBox::Slot().AutoHeight()[Content]
        ];
    // clang-format on
}

FName SCourseInformation::ValueTag(EField Field)
{
    static const TCHAR* const Names[] = {TEXT("Location"), TEXT("Elevation"), TEXT("Par"), TEXT("Designer"), TEXT("Distance")};
    return FName(FString(TEXT("CourseInformation.")) + Names[static_cast<uint8>(Field) % UE_ARRAY_COUNT(Names)]);
}

void SCourseInformation::Construct(const FArguments& Args)
{
    using namespace RoundSetupPanelsDetail;
    checkf(Args._Style && Args._Course, TEXT("SCourseInformation needs the menu's style and a course"));
    Course = Args._Course;
    TeeSet = Args._TeeSet;
    auto Named = [this](FText UGolfCourseDefinition::*Member)
    {
        return TAttribute<EVisibility>::CreateLambda([this, Member]
        {
            const UGolfCourseDefinition* Shown = Course.Get();
            return Shown && !(Shown->*Member).IsEmptyOrWhitespace() ? EVisibility::Visible : EVisibility::Collapsed;
        });
    };
    auto Read = [this](TFunction<FString(const UGolfCourseDefinition&)> Value)
    {
        return TAttribute<FText>::CreateLambda([this, Value = MoveTemp(Value)]
        {
            const UGolfCourseDefinition* Shown = Course.Get();
            return Shown ? FText::FromString(Value(*Shown)) : FText::GetEmpty();
        });
    };
    const TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    auto Add = [&Rows](const TSharedRef<SWidget>& Row) { Rows->AddSlot().AutoHeight().Padding(0, 0, 0, RowGap)[Row]; };
    Add(FactRow(TEXT("Location"), ValueTag(EField::Location), Read([](const UGolfCourseDefinition& C) { return C.Location.ToString(); }),
        Named(&UGolfCourseDefinition::Location)));
    Add(FactRow(TEXT("Elevation"), ValueTag(EField::Elevation), Read([](const UGolfCourseDefinition& C) { return RoundConditions::ElevationText(C); }),
        EVisibility::Visible));
    Add(FactRow(TEXT("Par"), ValueTag(EField::Par), Read([](const UGolfCourseDefinition& C) { return FString::FromInt(RoundConditions::CoursePar(C)); }),
        EVisibility::Visible));
    Add(FactRow(TEXT("Designer"), ValueTag(EField::Designer), Read([](const UGolfCourseDefinition& C) { return C.Designer.ToString(); }),
        Named(&UGolfCourseDefinition::Designer)));
    Add(FactRow(TEXT("Total distance"), ValueTag(EField::Distance),
        Read([this](const UGolfCourseDefinition& C) { return GolfSettings::RoundDistance(RoundConditions::CourseLengthM(C, TeeSet.Get())); }),
        EVisibility::Visible));
    ChildSlot[RoundSetupPanels::Section(*Args._Style, FText::FromString(TEXT("Course information")), Rows)];
}

void SCourseConditions::Construct(const FArguments& Args)
{
    using namespace RoundSetupPanelsDetail;
    checkf(Args._Style && Args._Course, TEXT("SCourseConditions needs the menu's style and a course"));
    Course = Args._Course;
    Setup = Args._Setup;
    auto Windy = [this] { return Setup.Get().WindSpeedMph > 0 ? EVisibility::Visible : EVisibility::Collapsed; };
    // clang-format off
    const TSharedRef<SWidget> Wind = SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0, 0, 12, 0)
        [
            SNew(SBox).Visibility_Lambda(Windy)
            [
                SNew(SWindArrow).FromDeg_Lambda([this] { return RelativeFromDeg(); }).Color(HudStyle::AccentColor())
            ]
        ]
        + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock).Tag(SpeedTag).Font_Lambda([] { return HudStyle::Font("Bold", MenuStyle::BodySize); }).ColorAndOpacity(HudStyle::TextColor())
                .Text_Lambda([this]
                {
                    const int32 Mph = Setup.Get().WindSpeedMph;
                    return FText::FromString(Mph > 0 ? RoundConditions::WindSpeedText(Mph) : FString(TEXT("No wind")));
                })
            ]
            + SVerticalBox::Slot().AutoHeight()
            [
                SNew(STextBlock).Tag(WindTag).Visibility_Lambda(Windy).AutoWrapText(true)
                .Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::NavSize); }).ColorAndOpacity(HudStyle::LabelColor())
                .Text_Lambda([this] { return FText::FromString(DirectionCaption()); })
            ]
        ];
    // clang-format on
    ChildSlot[RoundSetupPanels::Section(*Args._Style, FText::FromString(TEXT("Course conditions")), Wind)];
}

float SCourseConditions::RelativeFromDeg() const
{
    const UGolfCourseDefinition* Shown = Course.Get();
    const FOpenGolfRoundSetup Current = Setup.Get();
    if (!Shown)
        return 0.0f;
    const int32 LayoutIndex = Current.Holes.IsEmpty() ? 0 : Current.Holes[0] - 1;
    return RoundWindRelativeFromDeg(Current.WindFromDeg,
        RoundConditions::TeeShotBearingDeg(*Shown, LayoutIndex, RoundConditions::TeeSetName(*Shown, Current.TeeIndex)));
}

FString SCourseConditions::DirectionCaption() const
{
    const UGolfCourseDefinition* Shown = Course.Get();
    const FOpenGolfRoundSetup Current = Setup.Get();
    if (!Shown)
        return FString();
    const FString Direction = WindDirectionLabel(RelativeFromDeg());
    // A single-hole course is read against its own target line; a layout names the hole the wind is read on.
    return Current.Holes.IsEmpty() ? Direction : FString::Printf(TEXT("%s · hole %d"), *Direction, Current.Holes[0]);
}
