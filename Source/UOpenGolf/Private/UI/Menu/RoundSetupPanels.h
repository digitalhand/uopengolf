// SPDX-License-Identifier: MIT
#pragma once

#include "Play/OpenGolfRoundSetup.h"
#include "Widgets/SCompoundWidget.h"

class UGolfCourseDefinition;
struct FOpenGolfAccountStyle;

/** The round setup sidebar's facts about the course: where it is, its elevation, par, designer and length from the picked tees. */
class SCourseInformation : public SCompoundWidget
{
public:
    enum class EField : uint8
    {
        /** Hidden when the course names none. */
        Location,
        Elevation,
        Par,
        /** Hidden when the course names none. */
        Designer,
        /** From the picked tees. */
        Distance
    };

    // Pointer arguments need a default: SLATE_ARGUMENT leaves them uninitialized when a caller omits them.
    SLATE_BEGIN_ARGS(SCourseInformation) : _Style(nullptr), _Course(nullptr) {}
    /** The menu's style; it must outlive the panel. */
    SLATE_ARGUMENT(const FOpenGolfAccountStyle*, Style)
    SLATE_ARGUMENT(const UGolfCourseDefinition*, Course)
    /** The picked tee set, read while the panel shows. */
    SLATE_ATTRIBUTE(FName, TeeSet)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    /** The tag of Field's value text, for tests and tools. */
    static FName ValueTag(EField Field);

private:
    TWeakObjectPtr<const UGolfCourseDefinition> Course;
    TAttribute<FName> TeeSet;
};

/** The round setup sidebar's conditions: the round's random wind, read against the tee shot of the first picked hole. */
class SCourseConditions : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SCourseConditions) : _Style(nullptr), _Course(nullptr) {}
    SLATE_ARGUMENT(const FOpenGolfAccountStyle*, Style)
    SLATE_ARGUMENT(const UGolfCourseDefinition*, Course)
    /** The setup as it stands: its wind, tees and holes. */
    SLATE_ATTRIBUTE(OpenGolfPlay::FOpenGolfRoundSetup, Setup)
    SLATE_END_ARGS()

    void Construct(const FArguments& Args);

    /** The wind speed, such as "9 mph", or "No wind". */
    static const FName SpeedTag;
    /** Where the wind comes from on the first picked hole, such as "Right to left · hole 1". */
    static const FName WindTag;

private:
    /** The wind against the first picked hole's tee shot: 0 headwind, 90 from the right. */
    float RelativeFromDeg() const;
    FString DirectionCaption() const;

    TWeakObjectPtr<const UGolfCourseDefinition> Course;
    TAttribute<OpenGolfPlay::FOpenGolfRoundSetup> Setup;
};

namespace RoundSetupPanels
{
    /** A sidebar or tab panel: the menu's section background with a bold heading over Content. */
    TSharedRef<SWidget> Section(const FOpenGolfAccountStyle& Style, const FText& Heading, const TSharedRef<SWidget>& Content);
} // namespace RoundSetupPanels
