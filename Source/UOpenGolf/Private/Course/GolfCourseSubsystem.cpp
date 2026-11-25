// SPDX-License-Identifier: MIT
#include "Course/GolfCourseSubsystem.h"
#include "GolfCourseDefinition.h"
#include "Course/CourseDownloads.h"
#include "Course/RoundConditions.h"
#include "OpenGolfSettings.h"
#include "Interfaces/IPluginManager.h"
#include "Algo/Sort.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "Engine/StreamableManager.h"
#include "Kismet/GameplayStatics.h"

void UGolfCourseSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    auto* Downloads = Collection.InitializeDependency<UCourseDownloads>();
    UAssetManager* Manager = UAssetManager::GetIfInitialized();
    if (!Manager)
    {
        HandleLoaded();
        return;
    }
    for (const auto& Plugin : IPluginManager::Get().GetEnabledPluginsWithContent())
    {
        if (Plugin->GetDescriptor().Category == TEXT("Golf courses"))
            Manager->ScanPathsForPrimaryAssets(UGolfCourseDefinition::AssetType, {Plugin->GetMountedAssetPath()},
                UGolfCourseDefinition::StaticClass(), false);
    }
    if (Downloads)
        for (const auto& Pair : Downloads->Local())
        {
            const FSoftObjectPath Path(Pair.Value.Definition);
            if (auto* Definition = Cast<UGolfCourseDefinition>(Path.TryLoad()))
                Manager->RegisterSpecificPrimaryAsset(Definition->GetPrimaryAssetId(), FAssetData(Definition));
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
    InstalledPreviews.Empty();
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
    SortCourses();
    OnCoursesLoaded.Broadcast();
}

void UGolfCourseSubsystem::SortCourses()
{
    Algo::Sort(Courses, [](const TObjectPtr<UGolfCourseDefinition>& A, const TObjectPtr<UGolfCourseDefinition>& B)
        { return A->DisplayName.CompareTo(B->DisplayName) < 0; });
}

void UGolfCourseSubsystem::AddCourse(UGolfCourseDefinition& Course)
{
    if (UAssetManager* Manager = UAssetManager::GetIfInitialized())
        Manager->RegisterSpecificPrimaryAsset(Course.GetPrimaryAssetId(), FAssetData(&Course));
    InstalledPreviews.Add(FSoftObjectPath(&Course), Course.Preview.LoadSynchronous());
    Courses.AddUnique(&Course);
    SortCourses();
}

void UGolfCourseSubsystem::RemoveCourse(const FSoftObjectPath& Definition)
{
    InstalledPreviews.Remove(Definition);
    Courses.RemoveAll([&](const TObjectPtr<UGolfCourseDefinition>& Course) { return !Course || FSoftObjectPath(Course) == Definition; });
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

OpenGolfPlay::FOpenGolfRoundSetup UGolfCourseSubsystem::LastSetup(const UGolfCourseDefinition& Course) const
{
    if (const OpenGolfPlay::FOpenGolfRoundSetup* Remembered = Setups.Find(Course.GetFName()))
    {
        OpenGolfPlay::FOpenGolfRoundSetup Setup = *Remembered;
        Setup.Validate(RoundConditions::HoleCount(Course), RoundConditions::TeeCount(Course));
        return Setup;
    }
    return RoundConditions::Defaults(Course, GolfSettings::Get());
}

void UGolfCourseSubsystem::RememberSetup(const UGolfCourseDefinition& Course, const OpenGolfPlay::FOpenGolfRoundSetup& Setup)
{
    Setups.Add(Course.GetFName(), Setup);
}
