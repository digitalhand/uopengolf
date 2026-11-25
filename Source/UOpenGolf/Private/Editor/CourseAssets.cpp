// SPDX-License-Identifier: MIT
// uopengolf.CreateCourseAssets: the bundled course's definition and its menu preview, created only when missing.
#if WITH_EDITOR
#include "AssetImportTask.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Engine/Texture2D.h"
#include "Factories/TextureFactory.h"
#include "GolfCourseDefinition.h"
#include "HAL/IConsoleManager.h"
#include "IAssetTools.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UOpenGolfRange.h"

namespace
{
    /** Where the Asset Manager looks for GolfCourse definitions (Config/DefaultGame.ini). */
    const TCHAR* CourseFolder = TEXT("/Game/UOpenGolf/Courses");

    bool SaveAsset(UObject& Asset)
    {
        UPackage* Package = Asset.GetPackage();
        FAssetRegistryModule::AssetCreated(&Asset);
        Package->MarkPackageDirty();
        FSavePackageArgs SaveArgs;
        SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        const bool bSaved = UPackage::SavePackage(Package, &Asset, *Filename, SaveArgs);
        UE_LOG(LogTemp, Display, TEXT("%s %s"), bSaved ? TEXT("Created") : TEXT("Could not save"), *Package->GetName());
        return bSaved;
    }

    /** The preview texture at CourseFolder/Name, imported from SourceFile with UI settings if it does not exist yet. */
    UTexture2D* ImportPreview(const FString& SourceFile, const FString& Name)
    {
        const FString PackageName = FString(CourseFolder) / Name;
        if (FPackageName::DoesPackageExist(PackageName))
            return LoadObject<UTexture2D>(nullptr, *(PackageName + TEXT(".") + Name));
        if (!FPaths::FileExists(SourceFile))
        {
            UE_LOG(LogTemp, Error, TEXT("Course preview source not found: %s"), *SourceFile);
            return nullptr;
        }
        UAssetImportTask* Task = NewObject<UAssetImportTask>();
        Task->Filename = SourceFile;
        Task->DestinationPath = CourseFolder;
        Task->DestinationName = Name;
        Task->bAutomated = true;
        Task->bReplaceExisting = false;
        // The texture factory imports synchronously under DestinationName (Interchange would ignore the name).
        Task->Factory = NewObject<UTextureFactory>();
        FAssetToolsModule::GetModule().Get().ImportAssetTasks({Task});
        UTexture2D* Texture = Task->GetObjects().IsEmpty() ? nullptr : Cast<UTexture2D>(Task->GetObjects()[0]);
        if (!Texture)
        {
            UE_LOG(LogTemp, Error, TEXT("Could not import %s"), *SourceFile);
            return nullptr;
        }
        // Drawn by Slate as is: the UI group, one mip, uncompressed color.
        Texture->LODGroup = TEXTUREGROUP_UI;
        Texture->CompressionSettings = TC_EditorIcon;
        Texture->MipGenSettings = TMGS_NoMipmaps;
        Texture->SRGB = true;
        Texture->PostEditChange();
        return SaveAsset(*Texture) ? Texture : nullptr;
    }

    void CreateCourseAssets()
    {
        const FString Name(TEXT("DA_SavannaRange"));
        const FString PackageName = FString(CourseFolder) / Name;
        if (FPackageName::DoesPackageExist(PackageName))
        {
            UE_LOG(LogTemp, Display, TEXT("%s already exists; keeping its edits."), *PackageName);
            return;
        }
        UTexture2D* Preview = ImportPreview(FPaths::ProjectDir() / TEXT("Assets/Images/Courses/SavannaRange.png"), TEXT("T_SavannaRange_Preview"));
        if (!Preview)
            return;
        UPackage* Package = CreatePackage(*PackageName);
        auto* Course = NewObject<UGolfCourseDefinition>(Package, *Name, RF_Public | RF_Standalone);
        Course->DisplayName = FText::FromString(TEXT("Savanna Range"));
        Course->Map = TSoftObjectPtr<UWorld>(FSoftObjectPath(TEXT("/Game/UOpenGolf/Maps/L_SavannaRange.L_SavannaRange")));
        Course->Preview = Preview;
        Course->Holes = 1;
        Course->Par = AUOpenGolfRange::Par;
        Course->LengthM = AUOpenGolfRange::HoleDistanceM;
        SaveAsset(*Course);
    }

    FAutoConsoleCommand CreateCourseAssetsCommand(TEXT("uopengolf.CreateCourseAssets"),
        TEXT("Create the bundled course's definition and menu preview under /Game/UOpenGolf/Courses if they do not exist; preserves existing assets."),
        FConsoleCommandDelegate::CreateStatic(&CreateCourseAssets));
} // namespace
#endif
