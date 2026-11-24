// SPDX-License-Identifier: MIT
#include "UI/Menu/CourseBrowser.h"
#include "Course/GolfCourseSubsystem.h"
#include "Engine/Texture2D.h"
#include "GolfCourseDefinition.h"
#include "OpenGolfSettings.h"
#include "UI/Menu/MainMenuInternal.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"

using namespace MainMenuDetail;

namespace
{
    /** The strip under a card's name, dark enough to read white text over any preview. */
    const FLinearColor CardCaptionColor(0.015f, 0.025f, 0.02f, 0.92f);
    constexpr float CardGap = 12.0f;
    constexpr int32 CardNameSize = 20;
    constexpr int32 CountSize = 14;

    /** A labelled value under a selected card: Holes, Par or Length. */
    TSharedRef<SWidget> DetailColumn(const TCHAR* Label, const FString& Value)
    {
        return SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[MakeText(FText::FromString(Label), MenuStyle::SmallSize, true)]
            + SVerticalBox::Slot().AutoHeight()[MakeText(FText::FromString(Value), MenuStyle::BodySize)];
    }
} // namespace

void SCourseBrowser::Construct(const FArguments& Args)
{
    checkf(Args._Style, TEXT("SCourseBrowser needs the menu's style"));
    Style = Args._Style;
    Courses = Args._Courses;
    OnPlay = Args._OnPlay;
    SearchStyle = MenuStyle::SearchBox();
    InstalledCount = MakeText(FText::GetEmpty(), CountSize, true);

    // clang-format off
    ChildSlot
    [
        SNew(SScrollBox).Clipping(EWidgetClipping::ClipToBounds)
        + SScrollBox::Slot()[MakeText(FText::FromString(TEXT("Courses")), MenuStyle::TitleSize)]
        + SScrollBox::Slot().Padding(0, 2, 0, 16)[InstalledCount.ToSharedRef()]
        + SScrollBox::Slot().Padding(0, 0, 0, 16)
        [
            SNew(SSearchBox).Style(&SearchStyle).Tag(TEXT("CourseSearch")).HintText(FText::FromString(TEXT("Search courses")))
            .OnTextChanged_Lambda([this](const FText& Text) { Filter = Text.ToString().TrimStartAndEnd(); })
        ]
        + SScrollBox::Slot()[SAssignNew(Cards, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(CardGap, CardGap))]
    ];
    // clang-format on

    UGolfCourseSubsystem* Subsystem = Courses.Get();
    if (Subsystem && !Subsystem->IsLoaded())
        LoadedHandle = Subsystem->OnCoursesLoaded.AddSP(this, &SCourseBrowser::RebuildCards);
    else
        RebuildCards();
}

SCourseBrowser::~SCourseBrowser()
{
    if (UGolfCourseSubsystem* Subsystem = Courses.Get())
        Subsystem->OnCoursesLoaded.Remove(LoadedHandle);
}

void SCourseBrowser::RebuildCards()
{
    Cards->ClearChildren();
    Previews.Reset();
    const UGolfCourseSubsystem* Subsystem = Courses.Get();
    const int32 Count = Subsystem ? Subsystem->GetCourses().Num() : 0;
    InstalledCount->SetText(FText::FromString(FString::Printf(TEXT("%d installed"), Count)));
    if (!Subsystem)
        return;
    const FVector2D PreviewSize(CardWidth, CardWidth * PreviewAspect);
    for (const UGolfCourseDefinition* Course : Subsystem->GetCourses())
    {
        if (!Course)
            continue;
        const TSharedRef<FSlateBrush> Preview = MakeShared<FSlateBrush>();
        // Loaded with the list (the Menu bundle); a course without one keeps the card's size with an empty picture.
        if (UTexture2D* Texture = Course->Preview.Get())
        {
            Preview->SetResourceObject(Texture);
            Preview->ImageSize = PreviewSize;
        }
        else
        {
            *Preview = FSlateNoResource(PreviewSize);
        }
        Previews.Add(Preview);
        Cards->AddSlot()[MakeCard(*Course, *Preview)];
    }
}

bool SCourseBrowser::MatchesSearch(const UGolfCourseDefinition& Course) const
{
    return Filter.IsEmpty() || Course.DisplayName.ToString().Contains(Filter);
}

TSharedRef<SWidget> SCourseBrowser::MakeCard(const UGolfCourseDefinition& Course, const FSlateBrush& Preview)
{
    const TWeakObjectPtr<const UGolfCourseDefinition> Weak(&Course);
    const FSlateBrush* Fill = FCoreStyle::Get().GetBrush("WhiteBrush");
    // clang-format off
    return SNew(SBox).WidthOverride(CardWidth)
        .Visibility_Lambda([this, Weak] { return Weak.IsValid() && MatchesSearch(*Weak) ? EVisibility::Visible : EVisibility::Collapsed; })
        [
            SNew(SBorder).BorderImage(Fill).BorderBackgroundColor(HudStyle::LaunchTileColor()).Padding(1)
            [
                SNew(SVerticalBox)
                // The card: preview with the name across its foot. Selecting it shows the details below.
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton).ButtonStyle(&Style->Button).ContentPadding(0).Tag(TEXT("CourseCard"))
                    .OnClicked_Lambda([this, Weak] { Selected = Weak; return FReply::Handled(); })
                    [
                        SNew(SOverlay)
                        + SOverlay::Slot()[SNew(SImage).Image(&Preview)]
                        + SOverlay::Slot().VAlign(VAlign_Bottom)
                        [
                            SNew(SBorder).BorderImage(Fill).BorderBackgroundColor(CardCaptionColor).Padding(10)
                            [ MakeText(Course.DisplayName, CardNameSize) ]
                        ]
                    ]
                ]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SVerticalBox).Visibility_Lambda([this, Weak] { return Selected == Weak ? EVisibility::Visible : EVisibility::Collapsed; })
                    + SVerticalBox::Slot().AutoHeight().Padding(12, 10)
                    [
                        SNew(SHorizontalBox)
                        + SHorizontalBox::Slot().FillWidth(1)[DetailColumn(TEXT("Holes"), FString::FromInt(Course.Holes))]
                        + SHorizontalBox::Slot().FillWidth(1)[DetailColumn(TEXT("Par"), FString::FromInt(Course.Par))]
                        + SHorizontalBox::Slot().FillWidth(1)[DetailColumn(TEXT("Length"), GolfSettings::RoundDistance(Course.LengthM))]
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(12, 0, 12, 12)
                    [
                        MakeButton(*Style, FText::FromString(TEXT("Play")), [this, Weak]
                        {
                            if (const UGolfCourseDefinition* Picked = Weak.Get())
                                OnPlay.ExecuteIfBound(*Picked);
                        }, true)
                    ]
                ]
            ]
        ];
    // clang-format on
}
