// SPDX-License-Identifier: MIT
#pragma once

#include "Play/OpenGolfRoundSetup.h"
#include "Widgets/SCompoundWidget.h"

class SVerticalBox;
class SWrapBox;
class SScrollBox;
class UGolfCourseDefinition;
class UOpenGolfOnlineSubsystem;
struct FOpenGolfAccountStyle;

/**
 * The page between a course card's Play and the round. Three tabs hold what the player chooses:
 * - Players: the signed-in player or the option to play signed out.
 * - Match: the Gimmie and its radius.
 * - Course: the green speed, the tees and the holes to play.
 * A sidebar beside them (under them on a narrow window) shows the course's information and the round's conditions,
 * above a fixed action bar. The wind is not a choice: the caller rolls one for the round
 * (RoundConditions::RollWind) and the page shows it. Every course gets the same page; a single-hole course has no hole
 * picker and a course with one tee set no tee picker.
 */
class SRoundSetup : public SCompoundWidget
{
public:
    DECLARE_DELEGATE_OneParam(FOnStart, const OpenGolfPlay::FOpenGolfRoundSetup&);

    enum class ETab : uint8
    {
        Players,
        Match,
        Course
    };

    // Pointer arguments need a default: SLATE_ARGUMENT leaves them uninitialized when a caller omits them.
    SLATE_BEGIN_ARGS(SRoundSetup) : _Style(nullptr), _Course(nullptr) {}
    /** The menu's button and text style; it must outlive the page. */
    SLATE_ARGUMENT(const FOpenGolfAccountStyle*, Style)
    SLATE_ARGUMENT(const UGolfCourseDefinition*, Course)
    /** The choices the page opens with, including the round's rolled wind. */
    SLATE_ARGUMENT(OpenGolfPlay::FOpenGolfRoundSetup, Setup)
    /** What Reset to defaults restores (RoundConditions::Defaults); give it the same wind as Setup to keep the weather. */
    SLATE_ARGUMENT(OpenGolfPlay::FOpenGolfRoundSetup, Defaults)
    /** The account service: Players shows its signed-in player. Players can also start a round signed out. */
    SLATE_ARGUMENT(TWeakObjectPtr<UOpenGolfOnlineSubsystem>, Online)
    /** Start round, with the setup the round plays. */
    SLATE_EVENT(FOnStart, OnStart)
    SLATE_EVENT(FSimpleDelegate, OnBack)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    /** The choices as they stand; Holes may be empty while no hole is ticked. */
    const OpenGolfPlay::FOpenGolfRoundSetup& GetSetup() const { return Setup; }
    /** A hole is ticked, or the course is a single hole. */
    bool CanStart() const;
    ETab GetTab() const { return Tab; }
    /** Where keyboard and gamepad focus goes when the page opens: Start round, so the defaults are one press away. */
    TSharedPtr<SWidget> GetPreferredFocus() const { return StartButton; }

    // Tags for tests and tools: chips are SChoiceChips::ChipName(<tag>, index).
    /** The tab chips, in ETab order. */
    static const FName TabTag;
    /** The signed-in player's card on Players, and its avatar. */
    static const FName PlayerTag;
    static const FName PlayerAvatarTag;
    static const FName HoleSetTag;
    static const FName GimmieTag;
    static const FName GimmieSwitchTag;
    /** The tee set chips, in the first hole's order. */
    static const FName TeesTag;
    static const FName FasterTag;
    static const FName SlowerTag;
    static const FName StartTag;
    static const FName ResetTag;
    /** The checkbox of hole Number. */
    static FName HoleTag(int32 Number);

private:
    TSharedRef<SWidget> MakeHeader(const UGolfCourseDefinition& Picked);
    TSharedRef<SWidget> MakeFooter();
    void AddRow(const TSharedRef<SVerticalBox>& Rows, const FString& Label, const FString& Help, const TSharedRef<SWidget>& Control);
    void AddHeading(const TSharedRef<SVerticalBox>& Rows, const FString& Heading);
    TSharedRef<SWidget> MakeTabs();
    TSharedRef<SWidget> MakePlayers();
    TSharedRef<SWidget> MakeMatch();
    TSharedRef<SWidget> MakeCourse();
    TSharedRef<SWidget> MakeTees();
    TSharedRef<SWidget> MakeGreenSpeed();
    TSharedRef<SWidget> MakeGimmieSwitch();
    TSharedRef<SWidget> MakeGimmieRadius();
    TSharedRef<SWidget> MakeHoles();
    TSharedRef<SWidget> MakeSidebar();
    /** "9 holes · Par 36 · 3,180 yd" for the ticked holes from the picked tees. */
    FText HoleSummary() const;
    FName TeeSet() const;
    void ToggleHole(int32 Number);
    int32 HoleSetIndex() const;

    const FOpenGolfAccountStyle* Style = nullptr;
    TWeakObjectPtr<const UGolfCourseDefinition> Course;
    TWeakObjectPtr<UOpenGolfOnlineSubsystem> Online;
    OpenGolfPlay::FOpenGolfRoundSetup Setup;
    OpenGolfPlay::FOpenGolfRoundSetup Defaults;
    FOnStart OnStart;
    FSimpleDelegate OnBack;
    FSlateBrush PreviewBrush;
    TSharedPtr<SWrapBox> Columns;
    TSharedPtr<SScrollBox> Scroll;
    TSharedPtr<SWidget> StartButton;
    ETab Tab = ETab::Players;
    int32 HoleCount = 0;
    int32 TeeCount = 0;
};
