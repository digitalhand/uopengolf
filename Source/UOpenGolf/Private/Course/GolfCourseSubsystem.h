// SPDX-License-Identifier: MIT
#pragma once

#include "Subsystems/GameInstanceSubsystem.h"
#include "GolfCourseSubsystem.generated.h"

class UGolfCourseDefinition;
struct FStreamableHandle;

/**
 * The installed courses, found through the Asset Manager (type GolfCourse) and loaded with their menu previews when the
 * game instance starts. It lives as long as the game instance, so the list and previews survive level travel; its
 * streamable handle keeps the preview textures loaded for the browser's Slate brushes.
 *
 * Travel: a round opens the course's map with ?Course=<asset name>; leaving a round opens the default map with
 * ?Courses, which reopens the browser.
 */
UCLASS()
class UGolfCourseSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /** URL option naming the course to play, by its definition's asset name. */
    static constexpr const TCHAR* PlayOption = TEXT("Course");
    /** URL option that opens the main menu at the Courses browser. */
    static constexpr const TCHAR* BrowserOption = TEXT("Courses");

    DECLARE_MULTICAST_DELEGATE(FOnCoursesLoaded);
    /** Fires once, when the list and its previews have loaded. */
    FOnCoursesLoaded OnCoursesLoaded;

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** The installed courses sorted by name; empty until IsLoaded. */
    const TArray<TObjectPtr<UGolfCourseDefinition>>& GetCourses() const { return Courses; }
    bool IsLoaded() const { return bLoaded; }

    /** The course a URL's PlayOption names, loaded now if it is not loaded yet; null when the URL names none or no such course. */
    static const UGolfCourseDefinition* FindCourse(const FString& Options);

    /** The URL options that start a round of Course. */
    static FString TravelOptions(const UGolfCourseDefinition& Course);

private:
    void HandleLoaded();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGolfCourseDefinition>> Courses;
    TSharedPtr<FStreamableHandle> Handle;
    bool bLoaded = false;
};
