// SPDX-License-Identifier: MIT
#include "UI/Menu/CourseBrowser.h"
#include "Course/GolfCourseSubsystem.h"
#include "Course/CourseDownloads.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "GolfCourseDefinition.h"
#include "OpenGolfSettings.h"
#include "UI/Menu/MainMenuInternal.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

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
        + SScrollBox::Slot().HAlign(HAlign_Left).Padding(0, 0, 0, 16)
        [
            SNew(SBox).WidthOverride(MenuStyle::SearchWidth)
            [
                SNew(SSearchBox).Style(&SearchStyle).Tag(TEXT("CourseSearch")).HintText(FText::FromString(TEXT("Search courses")))
                .OnTextChanged_Lambda([this](const FText& Text) { Filter = Text.ToString().TrimStartAndEnd(); })
            ]
        ]
        + SScrollBox::Slot().Padding(0, 0, 0, 12)
        [
            SNew(STextBlock).Font_Lambda([] { return HudStyle::Font("Regular", MenuStyle::BodySize); })
            .ColorAndOpacity(HudStyle::LabelColor()).AutoWrapText(true)
            .Visibility_Lambda([this] { return Downloads.IsValid() && !Downloads->Message().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed; })
            .Text_Lambda([this] { return FText::FromString(Downloads.IsValid() ? Downloads->Message() : FString()); })
        ]
        + SScrollBox::Slot()[SAssignNew(Cards, SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(CardGap, CardGap))]
    ];
    // clang-format on

    UGolfCourseSubsystem* Subsystem = Courses.Get();
    if (Subsystem)
    {
        Downloads = Subsystem->GetGameInstance()->GetSubsystem<UCourseDownloads>();
        if (Downloads.IsValid())
        {
            DownloadHandle = Downloads->Changed.AddSP(this, &SCourseBrowser::RebuildCards);
            Downloads->Refresh();
        }
    }
    if (Subsystem && !Subsystem->IsLoaded())
        LoadedHandle = Subsystem->OnCoursesLoaded.AddSP(this, &SCourseBrowser::RebuildCards);
    else
        RebuildCards();
}

SCourseBrowser::~SCourseBrowser()
{
    if (Downloads.IsValid()) Downloads->Changed.Remove(DownloadHandle);
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
            Preview->ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY());
        }
        else
        {
            *Preview = FSlateNoResource(PreviewSize);
        }
        Previews.Add(Preview);
        Cards->AddSlot()[MakeCard(*Course, *Preview)];
    }
    if (!Downloads.IsValid())
        return;
    // Catalog courses not installed yet; installed ones show Update and Uninstall on their own card.
    for (const FCoursePack& Pack : Downloads->Entries())
    {
        if (Downloads->IsInstalled(Pack.Id) || Downloads->IsPendingRemoval(Pack.Id))
            continue;
        const FString Detail = FString::Printf(TEXT("Version %s  ·  %.0f MB"), *Pack.Version, Pack.DownloadBytes() / 1e6);
        Cards->AddSlot()[MakeDownloadCard(Pack.Id, Pack.Name, Detail)];
    }
}

TSharedRef<SWidget> SCourseBrowser::MakeDownloadCard(const FString& Id, const FString& Name, const FString& Detail)
{
    const FSlateBrush* Fill = FCoreStyle::Get().GetBrush("WhiteBrush");
    // clang-format off
    return SNew(SBox).WidthOverride(CardWidth)
        .Visibility_Lambda([this, Name] { return Filter.IsEmpty() || Name.Contains(Filter) ? EVisibility::Visible : EVisibility::Collapsed; })
        [
            SNew(SBorder).BorderImage(Fill).BorderBackgroundColor(HudStyle::LaunchTileColor()).Padding(12)
            [
                SNew(SVerticalBox)
                + SVerticalBox::Slot().AutoHeight()[MakeText(FText::FromString(Name), CardNameSize)]
                + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 10)[MakeText(FText::FromString(Detail), MenuStyle::SmallSize, true)]
                + SVerticalBox::Slot().AutoHeight()
                [
                    SNew(SButton).ButtonStyle(&Style->PrimaryButton).Tag(TEXT("CourseDownload"))
                    .IsEnabled_Lambda([this] { return Downloads.IsValid() && !Downloads->Busy(); })
                    .OnClicked_Lambda([this, Id] { if (Downloads.IsValid()) Downloads->Install(Id); return FReply::Handled(); })
                    [
                        SNew(STextBlock).Font(HudStyle::Font("Regular", MenuStyle::BodySize)).ColorAndOpacity(Style->PrimaryLabelColor)
                        .Text_Lambda([this, Id] { return FText::FromString(Downloads.IsValid() && Downloads->DownloadingId() == Id ? TEXT("Downloading…") : TEXT("Download")); })
                    ]
                ]
            ]
        ];
    // clang-format on
}

TSharedRef<SWidget> SCourseBrowser::MakeCardActions(const UGolfCourseDefinition& Course)
{
    const TWeakObjectPtr<const UGolfCourseDefinition> Weak(&Course);
    const TSharedRef<SVerticalBox> Actions = SNew(SVerticalBox);
    // The tees and the round's conditions are picked on the Round setup page that Play opens.
    Actions->AddSlot().AutoHeight()[MakeButton(*Style, FText::FromString(TEXT("Play")), [this, Weak]
    {
        if (const UGolfCourseDefinition* Picked = Weak.Get())
            OnPlay.ExecuteIfBound(*Picked);
    }, true)];
    const FCoursePack* Pack = Downloads.IsValid() ? Downloads->FindInstalledDefinition(Course.GetPathName()) : nullptr;
    if (!Pack)
        return Actions;
    const FString Id = Pack->Id;
    const FCoursePack* Entry = Downloads->FindEntry(Id);
    if (Entry && !Downloads->IsCurrent(*Entry))
        Actions->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[MakeButton(*Style, FText::FromString(TEXT("Update")), [this, Id]
        {
            if (Downloads.IsValid()) Downloads->Install(Id);
        })];
    Actions->AddSlot().AutoHeight().Padding(0, 6, 0, 0)[MakeButton(*Style, FText::FromString(TEXT("Uninstall")), [this, Id]
    {
        if (Downloads.IsValid()) Downloads->Uninstall(Id);
    })];
    return Actions;
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
                        + SOverlay::Slot()
                        [
                            SNew(SBox).WidthOverride(CardWidth).HeightOverride(CardWidth * PreviewAspect)
                            .Clipping(EWidgetClipping::ClipToBounds)
                            [ SNew(SScaleBox).Stretch(EStretch::ScaleToFill)[SNew(SImage).Image(&Preview)] ]
                        ]
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
                        MakeCardActions(Course)
                    ]
                ]
            ]
        ];
    // clang-format on
}
