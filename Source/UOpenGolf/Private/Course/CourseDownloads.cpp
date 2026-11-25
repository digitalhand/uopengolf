// SPDX-License-Identifier: MIT
#include "Course/CourseDownloads.h"
#include "Course/GolfCourseSubsystem.h"
#include "Engine/GameInstance.h"
#include "GolfCourseDefinition.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "GameUpdateHash.h"
#include "HAL/FileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    FString Platform() { return FPlatformProperties::IniPlatformName(); }
    bool Token(const FString& S)
    {
        if (S.IsEmpty() || S.Len() > 80) return false;
        for (TCHAR C : S)
            if (!FChar::IsAlnum(C) && C != '_' && C != '-') return false;
        return true;
    }
    bool ParsePack(const TSharedPtr<FJsonObject>& Json, FCoursePack& Out)
    {
        FString Target;
        const TArray<TSharedPtr<FJsonValue>>* Files;
        if (!Json || !Json->TryGetStringField(TEXT("platform"), Target) || Target != Platform() ||
            !Json->TryGetStringField(TEXT("id"), Out.Id) || !Token(Out.Id) ||
            !Json->TryGetStringField(TEXT("version"), Out.Version) || !Token(Out.Version) ||
            !Json->TryGetStringField(TEXT("name"), Out.Name) || !Json->TryGetStringField(TEXT("game_version"), Out.GameVersion) ||
            !Json->TryGetStringField(TEXT("definition"), Out.Definition) || !Out.Definition.StartsWith(TEXT("/") + Out.Id + TEXT("/")) ||
            !FPackageName::IsValidObjectPath(Out.Definition) || !Json->TryGetArrayField(TEXT("files"), Files)) return false;
        TSet<FString> Seen;
        for (const auto& Value : *Files)
        {
            const TSharedPtr<FJsonObject>* Obj;
            FCoursePackFile F;
            if (!Value->TryGetObject(Obj) || !(*Obj)->TryGetStringField(TEXT("name"), F.Name) ||
                !(F.Name == TEXT("Course.pak") || F.Name == TEXT("Course.utoc") || F.Name == TEXT("Course.ucas")) ||
                Seen.Contains(F.Name) || !(*Obj)->TryGetNumberField(TEXT("size"), F.Size) || F.Size <= 0 ||
                !(*Obj)->TryGetStringField(TEXT("sha256"), F.Hash) || F.Hash.Len() != 64) return false;
            Seen.Add(F.Name);
            Out.Files.Add(MoveTemp(F));
        }
        return Seen.Contains(TEXT("Course.pak")) && Seen.Contains(TEXT("Course.utoc")) == Seen.Contains(TEXT("Course.ucas"));
    }
    bool ReadJson(const FString& Text, TSharedPtr<FJsonObject>& Out)
    {
        return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) && Out.IsValid();
    }
}

int64 FCoursePack::DownloadBytes() const
{
    int64 Total = 0;
    for (const FCoursePackFile& File : Files)
        Total += File.Size;
    return Total;
}

FString UCourseDownloads::Directory(const FCoursePack& Pack) const { return Root / Pack.Id / Pack.Version; }

const FCoursePack* UCourseDownloads::FindInstalledDefinition(const FString& ObjectPath) const
{
    for (const auto& Pair : Installed)
        if (Pair.Value.Definition == ObjectPath)
            return &Pair.Value;
    return nullptr;
}

void UCourseDownloads::RemoveOtherVersions(const FString& Id, const FString& Keep) const
{
    TArray<FString> Folders;
    IFileManager::Get().FindFiles(Folders, *(Root / Id / TEXT("*")), false, true);
    for (const FString& Folder : Folders)
        if (Folder != Keep)
            IFileManager::Get().DeleteDirectory(*(Root / Id / Folder), false, true);
}
bool UCourseDownloads::IsCurrent(const FCoursePack& Pack) const
{
    const auto* Current = Installed.Find(Pack.Id);
    return Current && Current->Version == Pack.Version && Current->GameVersion == Pack.GameVersion;
}

void UCourseDownloads::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("InstalledCourses"));
    GConfig->GetString(TEXT("CourseDownloads"), TEXT("BaseUrl"), BaseUrl, GGameIni);
    BaseUrl.RemoveFromEnd(TEXT("/"));
    GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"), TEXT("ProjectVersion"), GameVersion, GGameIni);
    TArray<FString> Ids;
    auto& Files = IFileManager::Get();
    Files.FindFiles(Ids, *(Root / TEXT("*")), false, true);
    for (const auto& Id : Ids)
    {
        if (!Token(Id)) continue;
        const FString Folder = Root / Id;
        if (Files.FileExists(*(Folder / TEXT("remove"))))
        {
            Files.DeleteDirectory(*Folder, false, true);
            continue;
        }
        FString JsonText;
        TSharedPtr<FJsonObject> Json;
        FCoursePack Pack;
        if (FFileHelper::LoadFileToString(JsonText, *(Folder / TEXT("installed.json"))) && ReadJson(JsonText, Json) &&
            ParsePack(Json, Pack) && Pack.Id == Id && Pack.GameVersion == GameVersion)
        {
            // An update staged last session, or one interrupted, leaves other version folders behind.
            RemoveOtherVersions(Id, Pack.Version);
            if (Mount(Pack))
                Installed.Add(Id, Pack);
        }
    }
}

bool UCourseDownloads::Mount(const FCoursePack& Pack)
{
    if (!FCoreDelegates::MountPak.IsBound()) return false;
    const FString Path = Directory(Pack) / TEXT("Course.pak");
    if (!FCoreDelegates::MountPak.Execute(Path, 0)) return false;
    Mounted.Add(Pack.Id);
    const FString VirtualRoot = TEXT("/") + Pack.Id + TEXT("/");
    if (!FPackageName::MountPointExists(VirtualRoot))
        FPackageName::RegisterMountPoint(VirtualRoot, FPaths::ProjectPluginsDir() / Pack.Id / TEXT("Content/"));
    return true;
}

void UCourseDownloads::Deinitialize()
{
    if (Request) { Request->OnProcessRequestComplete().Unbind(); Request->CancelRequest(); }
    Pending = {};
    Request.Reset();
    Super::Deinitialize();
}

void UCourseDownloads::Finish(const FString& Message)
{
    Status = Message;
    Request.Reset();
    Pending = {};
    Changed.Broadcast();
}

void UCourseDownloads::Refresh()
{
    if (Busy() || BaseUrl.IsEmpty()) return;
    if (!BaseUrl.StartsWith(TEXT("https://")) && !BaseUrl.StartsWith(TEXT("http://127.0.0.1:")))
    {
        Finish(TEXT("The course catalog needs an HTTPS address."));
        return;
    }
    Request = FHttpModule::Get().CreateRequest();
    Request->SetURL(BaseUrl + TEXT("/catalog.json"));
    Request->SetVerb(TEXT("GET"));
    Status = TEXT("Checking course downloads…");
    Request->OnProcessRequestComplete().BindWeakLambda(this, [this](FHttpRequestPtr, FHttpResponsePtr Response, bool Ok)
    {
        TSharedPtr<FJsonObject> Json;
        const TArray<TSharedPtr<FJsonValue>>* Entries;
        if (!Ok || !Response || Response->GetResponseCode() != 200 || !ReadJson(Response->GetContentAsString(), Json) ||
            !Json->TryGetArrayField(TEXT("courses"), Entries))
        {
            Finish(TEXT("Course catalog unavailable. Installed courses still play offline."));
            return;
        }
        Catalog.Reset();
        for (const auto& Value : *Entries)
        {
            const TSharedPtr<FJsonObject>* Obj;
            FCoursePack Pack;
            if (Value->TryGetObject(Obj) && ParsePack(*Obj, Pack) && Pack.GameVersion == GameVersion)
                Catalog.Add(MoveTemp(Pack));
        }
        Finish(FString());
    });
    if (!Request->ProcessRequest()) Finish(TEXT("Could not start the catalog request."));
}

void UCourseDownloads::Install(const FString& Id)
{
    if (Busy() || IsPendingRemoval(Id)) return;
    const auto* Pack = Catalog.FindByPredicate([&](const auto& Item){ return Item.Id == Id; });
    if (!Pack || IsCurrent(*Pack)) return;
    Pending = *Pack;
    Staging = Root / Pending.Id / TEXT(".staging");
    IFileManager::Get().MakeDirectory(*Staging, true);
    FileIndex = 0;
    NextFile();
}

void UCourseDownloads::NextFile()
{
    const auto& File = Pending.Files[FileIndex];
    Status = FString::Printf(TEXT("Downloading %s · file %d of %d"), *Pending.Name, FileIndex + 1, Pending.Files.Num());
    const FString Path = Staging / File.Name;
    TSharedPtr<FArchive> Stream(IFileManager::Get().CreateFileWriter(*Path));
    Request = FHttpModule::Get().CreateRequest();
    if (!Stream || !Request->SetResponseBodyReceiveStream(Stream.ToSharedRef()))
    {
        Finish(TEXT("Could not open the course download folder."));
        return;
    }
    Request->SetVerb(TEXT("GET"));
    Request->SetURL(BaseUrl / Pending.Id / Pending.Version / Platform() / File.Name);
    Request->OnProcessRequestComplete().BindWeakLambda(this, [this, Stream, Path, File](FHttpRequestPtr, FHttpResponsePtr Response, bool Ok)
    {
        Stream->Close();
        if (!Ok || !Response || Response->GetResponseCode() != 200 || Stream->IsError() || IFileManager::Get().FileSize(*Path) != File.Size)
        {
            Finish(TEXT("Course download interrupted. Retry from Courses."));
            return;
        }
        TWeakObjectPtr<UCourseDownloads> Weak(this);
        Async(EAsyncExecution::ThreadPool, [Weak, Path, File]
        {
            const bool Verified = GameUpdate::Sha256OfFile(Path).Equals(File.Hash, ESearchCase::IgnoreCase);
            AsyncTask(ENamedThreads::GameThread, [Weak, Verified]
            {
                auto* Self = Weak.Get();
                if (!Self || Self->Pending.Id.IsEmpty()) return;
                if (!Verified) { Self->Finish(TEXT("Course checksum failed.")); return; }
                if (++Self->FileIndex < Self->Pending.Files.Num()) { Self->NextFile(); return; }
                Self->Commit();
            });
        });
    });
    if (!Request->ProcessRequest()) Finish(TEXT("Could not start the course download."));
}

void UCourseDownloads::Commit()
{
    auto& Files = IFileManager::Get();
    const FString Target = Directory(Pending);
    Files.MakeDirectory(*Target, true);
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetStringField(TEXT("id"), Pending.Id);
    Json->SetStringField(TEXT("name"), Pending.Name);
    Json->SetStringField(TEXT("version"), Pending.Version);
    Json->SetStringField(TEXT("game_version"), Pending.GameVersion);
    Json->SetStringField(TEXT("definition"), Pending.Definition);
    Json->SetStringField(TEXT("platform"), Platform());
    TArray<TSharedPtr<FJsonValue>> Objects;
    for (const auto& File : Pending.Files)
    {
        if (!Files.Move(*(Target / File.Name), *(Staging / File.Name), true))
        {
            Finish(TEXT("Could not store the downloaded course.")); return;
        }
        auto Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), File.Name);
        Obj->SetStringField(TEXT("sha256"), File.Hash);
        Obj->SetNumberField(TEXT("size"), File.Size);
        Objects.Add(MakeShared<FJsonValueObject>(Obj));
    }
    Json->SetArrayField(TEXT("files"), Objects);
    FString Text;
    FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text));
    const FString Receipt = Root / Pending.Id / TEXT("installed.json");
    if (!FFileHelper::SaveStringToFile(Text, *(Receipt + TEXT(".tmp"))) || !Files.Move(*Receipt, *(Receipt + TEXT(".tmp")), true))
    {
        Finish(TEXT("Could not save the installed course record.")); return;
    }
    const FCoursePack Pack = Pending;
    // The receipt now names this version, so the card stops offering it; the next start mounts it either way.
    Installed.Add(Pack.Id, Pack);
    // A course already mounted keeps playing its old files until the next start; a new one mounts now.
    if (Mounted.Contains(Pack.Id))
    {
        Finish(FString::Printf(TEXT("%s updated. Restart the game to use the new version."), *Pack.Name));
        return;
    }
    UGolfCourseSubsystem* Courses = GetGameInstance()->GetSubsystem<UGolfCourseSubsystem>();
    UGolfCourseDefinition* Course = Mount(Pack) ? Cast<UGolfCourseDefinition>(FSoftObjectPath(Pack.Definition).TryLoad()) : nullptr;
    if (!Course || !Courses)
    {
        Finish(FString::Printf(TEXT("%s installed. Restart the game to play it."), *Pack.Name));
        return;
    }
    Courses->AddCourse(*Course);
    Finish(FString::Printf(TEXT("%s installed."), *Pack.Name));
}

void UCourseDownloads::Uninstall(const FString& Id)
{
    if (Busy() || !Installed.Contains(Id)) return;
    if (!FFileHelper::SaveStringToFile(TEXT("remove"), *(Root / Id / TEXT("remove"))))
    {
        Finish(TEXT("Could not queue course removal.")); return;
    }
    // A mounted pack's files stay open, so they are deleted at the next start; the course leaves the list now.
    const FCoursePack Pack = Installed.FindAndRemoveChecked(Id);
    Removed.Add(Id);
    if (UGolfCourseSubsystem* Courses = GetGameInstance()->GetSubsystem<UGolfCourseSubsystem>())
        Courses->RemoveCourse(FSoftObjectPath(Pack.Definition));
    Finish(FString::Printf(TEXT("%s removed. Its files are deleted when the game next starts."), *Pack.Name));
}
