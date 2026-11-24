// SPDX-License-Identifier: MIT
#pragma once

#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "Widgets/SCompoundWidget.h"

class SScrollBox;
class STextBlock;
class SWrapBox;
class UGolfCourseDefinition;
class UGolfCourseSubsystem;
struct FOpenGolfAccountStyle;

/**
 * The home page's Courses panel: a title, the installed count, a search box and one card per installed course
 * (UGolfCourseSubsystem), each its preview with the name across the bottom. Selecting a card shows the course's hole,
 * par and length and a Play button. The cards are built when the course list has loaded and filtered by name as the
 * search changes; the menu page around them is never rebuilt for either.
 */
class SCourseBrowser : public SCompoundWidget
{
public:
    DECLARE_DELEGATE_OneParam(FOnPlay, const UGolfCourseDefinition&);

    // Pointer arguments need a default: SLATE_ARGUMENT leaves them uninitialized when a caller omits them.
    SLATE_BEGIN_ARGS(SCourseBrowser) : _Style(nullptr), _Courses(nullptr) {}
    /** The menu's button and text style; it must outlive the browser. */
    SLATE_ARGUMENT(const FOpenGolfAccountStyle*, Style)
    /** The installed courses; null shows none. */
    SLATE_ARGUMENT(UGolfCourseSubsystem*, Courses)
    /** Play on the selected card. */
    SLATE_EVENT(FOnPlay, OnPlay)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);
    virtual ~SCourseBrowser() override;

    /** Card width in Slate units; the preview fills it at a 5:3 aspect. */
    static constexpr float CardWidth = 360.0f;
    static constexpr float PreviewAspect = 0.6f;

private:
    /** One card per course, with a fresh preview brush each; called when the course list has loaded. */
    void RebuildCards();
    TSharedRef<SWidget> MakeCard(const UGolfCourseDefinition& Course, const FSlateBrush& Preview);
    bool MatchesSearch(const UGolfCourseDefinition& Course) const;

    const FOpenGolfAccountStyle* Style = nullptr;
    TWeakObjectPtr<UGolfCourseSubsystem> Courses;
    FOnPlay OnPlay;
    FDelegateHandle LoadedHandle;
    TSharedPtr<SWrapBox> Cards;
    TSharedPtr<STextBlock> InstalledCount;
    /** Brushes the card images point at, so stable addresses; the subsystem keeps their textures loaded. */
    TArray<TSharedRef<FSlateBrush>> Previews;
    FSearchBoxStyle SearchStyle;
    FString Filter;
    TWeakObjectPtr<const UGolfCourseDefinition> Selected;
};
