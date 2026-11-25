// SPDX-License-Identifier: MIT
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CourseDownloads.generated.h"

struct FCoursePackFile
{
    FString Name;
    int64 Size = 0;
    FString Hash;
};

struct FCoursePack
{
    FString Id, Name, Version, Definition, GameVersion;
    TArray<FCoursePackFile> Files;
    int64 DownloadBytes() const;
};

/**
 * Downloadable courses: a public catalog (catalog.json at [CourseDownloads] BaseUrl), and the packs installed under
 * Saved/InstalledCourses/<id>/<version>. A download is staged and checked against its SHA-256 before it replaces
 * anything. A new course is mounted and playable at once; an update of a mounted course and an uninstall finish at the
 * next start, because a mounted pack's files stay open.
 */
UCLASS()
class UCourseDownloads : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    void Refresh();
    void Install(const FString& Id);
    void Uninstall(const FString& Id);
    bool IsInstalled(const FString& Id) const { return Installed.Contains(Id); }
    bool IsCurrent(const FCoursePack& Pack) const;
    /** Uninstalled in this session; its files go at the next start, and it cannot be downloaded again until then. */
    bool IsPendingRemoval(const FString& Id) const { return Removed.Contains(Id); }
    bool Busy() const { return Request.IsValid() || !Pending.Id.IsEmpty(); }
    /** The course being downloaded, or empty. */
    const FString& DownloadingId() const { return Pending.Id; }
    const TArray<FCoursePack>& Entries() const { return Catalog; }
    const FCoursePack* FindEntry(const FString& Id) const { return Catalog.FindByPredicate([&](const FCoursePack& P) { return P.Id == Id; }); }
    /** The installed pack whose course definition is ObjectPath, or null for a course that came with the game. */
    const FCoursePack* FindInstalledDefinition(const FString& ObjectPath) const;
    const FString& Message() const { return Status; }
    const TMap<FString, FCoursePack>& Local() const { return Installed; }
    DECLARE_MULTICAST_DELEGATE(FChanged);
    FChanged Changed;
private:
    void NextFile();
    void Commit();
    void Finish(const FString& Message);
    bool Mount(const FCoursePack& Pack);
    void RemoveOtherVersions(const FString& Id, const FString& Keep) const;
    FString Directory(const FCoursePack& Pack) const;
    FString Root, BaseUrl, GameVersion, Status, Staging;
    TMap<FString, FCoursePack> Installed;
    TArray<FCoursePack> Catalog;
    FCoursePack Pending;
    TSet<FString> Removed;
    /** Packs mounted in this session; their files cannot be replaced until the next start. */
    TSet<FString> Mounted;
    int32 FileIndex = 0;
    FHttpRequestPtr Request;
};
