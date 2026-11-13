// SPDX-License-Identifier: MIT
#include "OpenGolfOnlineSubsystem.h"
#include "OnlineJson.h"
#include "OnlineAtomicFile.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"

using namespace OpenGolfOnline;

FString UOpenGolfOnlineSubsystem::LocalPath() const
{
#if WITH_DEV_AUTOMATION_TESTS
    if (!TestLocalRoot.IsEmpty()) return TestLocalRoot / TEXT("player.json");
#endif
    return FPaths::Combine(FPlatformProcess::UserSettingsDir(), Config.StorageName, TEXT("Online"), TEXT("player.json"));
}
void UOpenGolfOnlineSubsystem::LoadLocal()
{
    const FString Path = LocalPath();
    if (!IFileManager::Get().FileExists(*Path)) return;
    FString Contents;
    TSharedPtr<FJsonObject> Data;
    const int64 Size = IFileManager::Get().FileSize(*Path);
    if (Size >= 0 && Size <= 8 * 1024 * 1024 && FFileHelper::LoadFileToString(Contents,*Path)) Data = Object(Contents);
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    bool Valid = Data && Data->TryGetArrayField(TEXT("shots"), Rows) && Rows->Num() <= 5000;
    FString Owner = Text(Data,TEXT("account_key"));
    Valid &= Owner.IsEmpty() || Owner.Len() == 64;
    if (Valid)
    {
        for (const auto& Row : *Rows)
        {
            FGuid Id;
            if (!Row || Row->Type != EJson::Object || !FGuid::Parse(Text(Row->AsObject(),TEXT("id")),Id)) { Valid = false; break; }
        }
    }
    if (!Valid)
    {
        const FString Backup = Path + TEXT(".corrupt-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
        IFileManager::Get().Move(*Backup,*Path,false,false);
        StorageWarning = TEXT("The local profile file could not be read. It was preserved for recovery.");
        bLocalWriteFailed = IFileManager::Get().FileExists(*Path);
        return;
    }
    AccountKey = OutboxOwner = Owner;
    Outbox = *Rows;
    const TSharedPtr<FJsonObject>* Cached = nullptr;
    if (Data->TryGetObjectField(TEXT("profile"),Cached)) Profile = *Cached;
}
bool UOpenGolfOnlineSubsystem::SaveLocal()
{
    auto Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("account_key"),AccountKey);
    Data->SetArrayField(TEXT("shots"),Outbox);
    if (Profile) Data->SetObjectField(TEXT("profile"),Profile);
    const FString Path = LocalPath();
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
#if PLATFORM_LINUX
    const FTCHARToUTF8 Directory(*FPaths::GetPath(Path));
    chmod(Directory.Get(),S_IRWXU);
#endif
    const bool Ok = WriteAtomically(Path,Json(Data));
    bLocalWriteFailed = !Ok;
    if (!Ok) { StorageWarning = TEXT("Could not save pending shots on this computer. Keep the game open and retry."); Touch(); }
    return Ok;
}
bool UOpenGolfOnlineSubsystem::QueueShot(const TSharedPtr<FJsonObject>& Shot, const FString& Owner)
{
    if (!Shot || Owner.IsEmpty() || Owner != AccountKey || Owner != OutboxOwner) return false;
    if (Outbox.Num() >= 5000)
    {
        Status = TEXT("The pending-shot queue is full. Connect and sync before saving more shots."); Touch(); return false;
    }
    const FString Id = Text(Shot,TEXT("id"));
    for (const auto& Row : Outbox) if (Text(Row->AsObject(),TEXT("id")) == Id) return true;
    Outbox.Add(MakeShared<FJsonValueObject>(Shot));
    SaveLocal(); UploadAt = FPlatformTime::Seconds() + 5; Touch();
    return true;
}
void UOpenGolfOnlineSubsystem::Flush()
{
    if (!HasSession() || bUploading || bDeleting || Outbox.IsEmpty() || AccountKey != OutboxOwner) return;
    if (bLocalWriteFailed && !SaveLocal()) return;
    UploadBatch(FMath::Min(UploadLimit,Outbox.Num()));
}
void UOpenGolfOnlineSubsystem::UploadBatch(int32 Count)
{
    bUploading = true; Touch();
    TArray<TSharedPtr<FJsonValue>> Batch;
    for (int32 I=0;I<Count;++I) Batch.Add(Outbox[I]);
    auto Body = MakeShared<FJsonObject>(); Body->SetArrayField(TEXT("shots"),Batch);
    Api(TEXT("/range-shots/batch"),TEXT("POST"),Json(Body),[this,Count](FReply Reply)
    {
        bUploading = false;
        const auto Result = Object(Reply.Body);
        const bool Accepted = SuccessReply(Reply) && Result && Number(Result,TEXT("accepted"),-1) >= 0
            && Number(Result,TEXT("duplicates"),-1) >= 0 && Number(Result,TEXT("accepted")) + Number(Result,TEXT("duplicates")) == Count;
        if (Accepted)
        {
            Outbox.RemoveAt(0,Count); UploadLimit = 100; SaveLocal(); UploadAt = FPlatformTime::Seconds() + 0.1;
        }
        else if (Reply.Status == 400 || Reply.Status == 409 || Reply.Status == 413 || Reply.Status == 422)
        {
            if (Count > 1) UploadLimit = FMath::Max(1,Count/2);
            else { Outbox.RemoveAt(0); ++RejectedCount; SaveLocal(); Status = TEXT("The server rejected a shot. It was removed from the upload queue."); }
            UploadAt = FPlatformTime::Seconds() + 0.1;
        }
        else
        {
            Status = TEXT("Shot upload paused. Pending shots are kept for retry.");
            UploadAt = FPlatformTime::Seconds() + (Reply.Status == 429 ? 120 : 30);
        }
        Touch();
    });
}
void UOpenGolfOnlineSubsystem::LoadCarry()
{
    OnBeforeReadProfile.Broadcast();
    Flush();
    if (bUploading || bLoading || bDeleting) { bCarryRequested = true; return; }
    bLoading = true; Touch();
    Api(TEXT("/clubs/carry?days=90"),TEXT("GET"),{},[this](FReply Reply)
    {
        bLoading = false;
        TArray<TSharedPtr<FJsonValue>> Rows;
        if (SuccessReply(Reply) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reply.Body),Rows))
        { Carry = MoveTemp(Rows); ++HistoryRevision; OnHistoryChanged.Broadcast(); Status = TEXT("Profile updated."); }
        else Status = TEXT("Could not load club averages. Try again.");
        Touch();
    });
}
void UOpenGolfOnlineSubsystem::LoadHistory(const FString& Club)
{
    static const TSet<FString> Clubs = {TEXT("DR"),TEXT("3W"),TEXT("5W"),TEXT("7W"),TEXT("4I"),TEXT("5I"),TEXT("6I"),TEXT("7I"),TEXT("8I"),TEXT("9I"),TEXT("PW"),TEXT("GW"),TEXT("SW")};
    if (!Clubs.Contains(Club)) return;
    OnBeforeReadProfile.Broadcast();
    if (HistoryClub != Club) History.Empty();
    HistoryClub = Club; ++HistoryRequest; bHistoryRequested = true; Touch();
    Flush();
}
void UOpenGolfOnlineSubsystem::UpdateDisplayName(const FString& Name)
{
    if (bLoading || Name.TrimStartAndEnd().IsEmpty() || Name.Len()>40) return;
    for (TCHAR C : Name) if (C < 32 || C == 127) return;
    auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("display_name"),Name.TrimStartAndEnd());
    bLoading = true; Touch();
    Api({},TEXT("PATCH"),Json(Body),[this](FReply Reply)
    {
        bLoading = false;
        auto Data = Object(Reply.Body);
        if (SuccessReply(Reply) && Data) { Profile = Data; SaveLocal(); Status = TEXT("Profile saved."); }
        else Status = TEXT("Could not save the profile. Try again.");
        Touch();
    });
}
void UOpenGolfOnlineSubsystem::DeleteShot(const FString& Id)
{
    FGuid Parsed;
    if (!FGuid::Parse(Id,Parsed) || bDeleting) return;
    PendingDelete = Id; bDeleting = true; Touch();
}
FString UOpenGolfOnlineSubsystem::ExportHistory()
{
    const TCHAR* Fields[] = {TEXT("club"),TEXT("shot_at"),TEXT("carry_yd"),TEXT("roll_yd"),TEXT("ball_speed_mph"),TEXT("launch_angle_deg"),TEXT("launch_direction_deg"),TEXT("total_spin_rpm"),TEXT("spin_axis_deg"),TEXT("back_spin_rpm"),TEXT("side_spin_rpm"),TEXT("face_angle_deg"),TEXT("club_path_deg"),TEXT("face_to_path_deg"),TEXT("attack_angle_deg"),TEXT("dynamic_loft_deg")};
    FString Csv = TEXT("Club|Timestamp (UTC)|Carry (yd)|Roll (yd)|Ball speed (mph)|Launch angle (deg)|Launch direction (deg)|Total spin (rpm)|Spin axis (deg)|Backspin (rpm)|Sidespin (rpm)|Face angle (deg)|Club path (deg)|Face-to-path (deg)|Attack angle (deg)|Dynamic loft (deg)\r\n");
    for (const auto& Row : History)
    {
        const auto Data = Row->AsObject();
        for (int32 I=0;I<UE_ARRAY_COUNT(Fields);++I)
        {
            if (I) Csv += TEXT("|");
            FString Value;
            if (I<2) Value = Text(Data,Fields[I]);
            else
            {
                double V;
                if (I==13)
                {
                    double Face,Path;
                    if (Data->TryGetNumberField(TEXT("face_angle_deg"),Face) && Data->TryGetNumberField(TEXT("club_path_deg"),Path)) Value=FString::SanitizeFloat(Face-Path);
                }
                else if (Data->TryGetNumberField(Fields[I],V)) Value=FString::SanitizeFloat(V);
            }
            Value.ReplaceInline(TEXT("\""),TEXT("\"\""));
            Csv += TEXT("\"") + Value + TEXT("\"");
        }
        Csv += TEXT("\r\n");
    }
    const FString Directory = FPaths::Combine(FPlatformProcess::UserSettingsDir(),Config.StorageName,TEXT("Exports"));
    IFileManager::Get().MakeDirectory(*Directory,true);
    const FString Path = Directory / (HistoryClub + TEXT("-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".csv"));
    if (!FFileHelper::SaveStringToFile(Csv,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { Status=TEXT("Could not export shot history."); Touch(); return {}; }
    Status = TEXT("Exported to ") + Path; Touch(); return Path;
}

void UOpenGolfOnlineSubsystem::ProcessPlayerRequests()
{
    if (!HasSession() || bLoading || bUploading) return;
    if (bDeleting)
    {
        if (PendingDelete.IsEmpty()) return;
        const FString Id = MoveTemp(PendingDelete); PendingDelete.Empty();
        const auto Previous = Outbox;
        Outbox.RemoveAll([&Id](const auto& Row){return Text(Row->AsObject(),TEXT("id"))==Id;});
        if (!SaveLocal()) { Outbox=Previous; bDeleting=false; Status=TEXT("Could not save the deletion. Try again."); Touch(); return; }
        auto Body = MakeShared<FJsonObject>(); Body->SetArrayField(TEXT("ids"),{MakeShared<FJsonValueString>(Id)});
        Api(TEXT("/range-shots/delete"),TEXT("POST"),Json(Body),[this](FReply Reply)
        {
            bDeleting=false;
            if (SuccessReply(Reply)) { bCarryRequested=true; bHistoryRequested=!HistoryClub.IsEmpty(); Status=TEXT("Shot deleted."); }
            else Status=TEXT("Could not delete the shot. Refresh and try again.");
            Touch();
        });
        return;
    }
    if (bHistoryRequested)
    {
        bHistoryRequested=false; bLoading=true;
        const uint64 Version=HistoryRequest;
        const FString Path=TEXT("/clubs/")+HistoryClub.ToLower()+TEXT("/shots?days=90&limit=200");
        Api(Path,TEXT("GET"),{},[this,Version](FReply Reply)
        {
            bLoading=false;
            if (Version!=HistoryRequest) return;
            TArray<TSharedPtr<FJsonValue>> Rows;
            if (SuccessReply(Reply) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Reply.Body),Rows))
            { History=MoveTemp(Rows); ++HistoryRevision; OnHistoryChanged.Broadcast(); Status=TEXT("Shot history updated."); }
            else Status=TEXT("Could not load shot history. Try again.");
            Touch();
        });
    }
    else if (bCarryRequested) { bCarryRequested=false; LoadCarry(); }
}
