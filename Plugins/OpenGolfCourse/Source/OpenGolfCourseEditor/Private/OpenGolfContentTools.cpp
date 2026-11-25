// SPDX-License-Identifier: MIT
#include "OpenGolfContentTools.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "ObjectTools.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"

namespace
{
    /** Old -> new object path for a redirector, including a Blueprint's generated class and default object. */
    void AddRedirect(const UObjectRedirector& Redirector, TMap<FSoftObjectPath, FSoftObjectPath>& Map)
    {
        const FSoftObjectPath Old(&Redirector), New(Redirector.DestinationObject);
        Map.Add(Old, New);
        if (Cast<UBlueprint>(Redirector.DestinationObject))
        {
            Map.Add(FSoftObjectPath(Old.ToString() + TEXT("_C")), FSoftObjectPath(New.ToString() + TEXT("_C")));
            Map.Add(FSoftObjectPath(FString::Printf(TEXT("%s.Default__%s_C"), *Old.GetLongPackageName(), *Old.GetAssetName())),
                FSoftObjectPath(FString::Printf(TEXT("%s.Default__%s_C"), *New.GetLongPackageName(), *New.GetAssetName())));
        }
    }
} // namespace

// The steps of the Content Browser's "Update Redirector References" (FAssetFixUpRedirectors), without its report
// dialog, which a headless editor cannot show: load each referencer, rewrite its soft paths, save it, then delete
// each redirector whose referencers all saved.
int32 UOpenGolfContentTools::FixUpRedirectors(const TArray<FString>& Paths, int32& FixedUp)
{
    FixedUp = 0;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    FARFilter Filter;
    Filter.bRecursivePaths = true;
    Filter.ClassPaths.Add(UObjectRedirector::StaticClass()->GetClassPathName());
    for (const FString& Path : Paths)
        Filter.PackagePaths.Add(FName(*Path));
    TArray<FAssetData> Found;
    Registry.GetAssets(Filter, Found);

    TArray<UObjectRedirector*> Redirectors;
    TMap<FSoftObjectPath, FSoftObjectPath> RedirectMap;
    TMap<UObjectRedirector*, TArray<FName>> Referencers;
    TSet<UPackage*> ToSave;
    bool bCodeReference = false;
    for (const FAssetData& Data : Found)
    {
        auto* Redirector = Cast<UObjectRedirector>(Data.GetAsset());
        if (!Redirector || !Redirector->DestinationObject)
            continue;
        Redirectors.Add(Redirector);
        AddRedirect(*Redirector, RedirectMap);
        TArray<FName>& Names = Referencers.Add(Redirector);
        Registry.GetReferencers(Data.PackageName, Names);
        for (const FName Name : Names)
        {
            UPackage* Package = FindPackage(nullptr, *Name.ToString());
            if (!Package)
                Package = LoadPackage(nullptr, *Name.ToString(), LOAD_None);
            if (Package && Package->HasAnyPackageFlags(PKG_CompiledIn))
            {
                UE_LOG(LogTemp, Error, TEXT("%s is referenced by code (%s); it is kept."), *Data.PackageName.ToString(), *Name.ToString());
                bCodeReference = true;
            }
            else if (Package)
                ToSave.Add(Package);
        }
    }
    if (Redirectors.IsEmpty())
        return 0;

    // Hard references follow the redirector when the referencer loads; soft paths are rewritten here.
    FAssetToolsModule::GetModule().Get().RenameReferencingSoftObjectPaths(ToSave.Array(), RedirectMap);
    TArray<UPackage*> FailedToSave;
    if (!ToSave.IsEmpty())
        FEditorFileUtils::PromptForCheckoutAndSave(ToSave.Array(), false, false, &FailedToSave);

    TArray<UObject*> ToDelete;
    for (UObjectRedirector* Redirector : Redirectors)
    {
        const bool bAllSaved = !Referencers[Redirector].ContainsByPredicate([&FailedToSave](FName Name)
            { return FailedToSave.ContainsByPredicate([Name](const UPackage* Package) { return Package && Package->GetFName() == Name; }); });
        if (!bAllSaved || bCodeReference)
            continue;
        Redirector->RemoveFromRoot();
        ToDelete.Add(Redirector);
    }
    if (!ToDelete.IsEmpty())
        FixedUp = ObjectTools::DeleteObjects(ToDelete, false, ObjectTools::EAllowCancelDuringDelete::CancelNotAllowed);
    UE_LOG(LogTemp, Display, TEXT("OpenGolf redirectors: %d found, %d referencing packages saved (%d failed), %d deleted."),
        Redirectors.Num(), ToSave.Num() - FailedToSave.Num(), FailedToSave.Num(), FixedUp);
    return Redirectors.Num();
}
