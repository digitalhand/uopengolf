// SPDX-License-Identifier: MIT
#include "Course/GolfCourseSubsystem.h"
#include "GolfCourseDefinition.h"
#include "Algo/Sort.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"

void UGolfCourseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UAssetManager* Manager = UAssetManager::GetIfInitialized();
    if (!Manager)
    {
        HandleLoaded();
        return;
    }
    Handle = Manager->LoadPrimaryAssetsWithType(UGolfCourseDefinition::AssetType, {UGolfCourseDefinition::MenuBundle},
        FStreamableDelegate::CreateUObject(this, &ThisClass::HandleLoaded));
    // No handle: nothing to load, or everything already loaded, and the delegate is not called.
    if (!Handle.IsValid() || Handle->HasLoadCompleted())
        HandleLoaded();
}

void UGolfCourseSubsystem::Deinitialize()
{
    if (Handle.IsValid())
        Handle->CancelHandle();
    Handle.Reset();
    Courses.Empty();
    Super::Deinitialize();
}

void UGolfCourseSubsystem::HandleLoaded()
{
    if (bLoaded)
        return;
    bLoaded = true;
    if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
    {
        TArray<UObject*> Loaded;
        Manager->GetPrimaryAssetObjectList(UGolfCourseDefinition::AssetType, Loaded);
        for (UObject* Object : Loaded)
            if (auto* Course = Cast<UGolfCourseDefinition>(Object))
                Courses.Add(Course);
    }
    Algo::Sort(Courses, [](const TObjectPtr<UGolfCourseDefinition>& A, const TObjectPtr<UGolfCourseDefinition>& B)
        { return A->DisplayName.CompareTo(B->DisplayName) < 0; });
    OnCoursesLoaded.Broadcast();
}

const UGolfCourseDefinition* UGolfCourseSubsystem::FindCourse(const FString& Options)
{
    const FString Name = UGameplayStatics::ParseOption(Options, PlayOption);
    UAssetManager* Manager = UAssetManager::GetIfInitialized();
    if (Name.IsEmpty() || !Manager)
        return nullptr;
    const FPrimaryAssetId Id(UGolfCourseDefinition::AssetType, FName(*Name));
    if (auto* Course = Manager->GetPrimaryAssetObject<UGolfCourseDefinition>(Id))
        return Course;
    // Launched straight into a round (?Course= on the command line) before the list loaded: a small asset, loaded now.
    const FSoftObjectPath Path = Manager->GetPrimaryAssetPath(Id);
    return Path.IsValid() ? Cast<UGolfCourseDefinition>(Path.TryLoad()) : nullptr;
}

FString UGolfCourseSubsystem::TravelOptions(const UGolfCourseDefinition& Course)
{
    return FString::Printf(TEXT("%s=%s"), PlayOption, *Course.GetPrimaryAssetId().PrimaryAssetName.ToString());
}
