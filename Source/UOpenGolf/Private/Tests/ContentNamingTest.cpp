// SPDX-License-Identifier: MIT
// Docs/ContentNaming.md, held in the editor: each asset's prefix matches its class, no redirector is left behind,
// and every actor in the project's maps has a label without spaces or retired words. Name casing, variants and
// folders are checked without the editor by Assets/Scripts/check_content_names.py.
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Level.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/ObjectRedirector.h"

namespace ContentNamingTestDetail
{
    /** Content mounts the project owns. Engine and marketplace mounts follow their own conventions. */
    const TCHAR* const Mounts[] = {TEXT("/Game/UOpenGolf"), TEXT("/OpenGolfCourse"), TEXT("/OpenGolfSavanna"),
        TEXT("/OpenGolfEnvironment"), TEXT("/HunterRanch")};

    /** Class name -> prefix, matched on the asset's class or its nearest listed superclass. */
    const TMap<FName, FString>& Prefixes()
    {
        static const TMap<FName, FString> Map = {
            {TEXT("WidgetBlueprint"), TEXT("WBP_")}, {TEXT("Blueprint"), TEXT("BP_")},
            {TEXT("StaticMesh"), TEXT("SM_")}, {TEXT("SkeletalMesh"), TEXT("SK_")},
            {TEXT("MaterialInstanceConstant"), TEXT("MI_")}, {TEXT("Material"), TEXT("M_")},
            {TEXT("MaterialFunction"), TEXT("MF_")}, {TEXT("MaterialParameterCollection"), TEXT("MPC_")},
            {TEXT("Texture2D"), TEXT("T_")}, {TEXT("TextureCube"), TEXT("HDR_")}, {TEXT("PhysicalMaterial"), TEXT("PM_")},
            {TEXT("World"), TEXT("L_")}, {TEXT("PCGGraph"), TEXT("PCG_")}, {TEXT("DataTable"), TEXT("DT_")},
            {TEXT("CurveTable"), TEXT("CT_")}, {TEXT("NiagaraSystem"), TEXT("FXS_")}, {TEXT("NiagaraEmitter"), TEXT("FXE_")},
            {TEXT("DataAsset"), TEXT("DA_")}};
        return Map;
    }

    /** Expected prefix for an asset's class, or empty when the class is not covered. */
    FString PrefixFor(const FTopLevelAssetPath& ClassPath)
    {
        for (const UClass* Class = FindObject<UClass>(ClassPath); Class; Class = Class->GetSuperClass())
            if (const FString* Prefix = Prefixes().Find(Class->GetFName()))
                return *Prefix;
        return FString();
    }

    /** Problems with an actor label, or empty. */
    FString LabelProblem(const AActor& Actor)
    {
        const FString Label = Actor.GetActorLabel();
        if (Label.Contains(TEXT(" ")))
            return TEXT("contains a space");
        for (const TCHAR* Word : {TEXT("coastal"), TEXT("draft")})
            if (Label.Contains(Word, ESearchCase::IgnoreCase))
                return FString::Printf(TEXT("contains '%s'"), Word);
        if (Actor.IsA<AStaticMeshActor>() && !Label.StartsWith(TEXT("SM_")))
            return TEXT("shows a static mesh but does not start with SM_");
        if (Cast<UBlueprintGeneratedClass>(Actor.GetClass()) && !Label.StartsWith(TEXT("BP_")))
            return TEXT("is a Blueprint instance but does not start with BP_");
        return FString();
    }
} // namespace ContentNamingTestDetail

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentNamingAssetsTest, "UOpenGolf.Content.Naming.Assets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentNamingAssetsTest::RunTest(const FString&)
{
    using namespace ContentNamingTestDetail;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    int32 Checked = 0;
    for (const TCHAR* Mount : Mounts)
    {
        TArray<FAssetData> Assets;
        Registry.GetAssetsByPath(FName(Mount), Assets, true);
        for (const FAssetData& Asset : Assets)
        {
            const FString Path = Asset.PackageName.ToString();
            if (Asset.AssetClassPath == UObjectRedirector::StaticClass()->GetClassPathName())
            {
                AddError(FString::Printf(TEXT("%s is a redirector left by a rename; fix up redirectors"), *Path));
                continue;
            }
            const FString Name = Asset.AssetName.ToString();
            if (Name.EndsWith(TEXT("_BuiltData")))
                continue; // The engine names a map's lighting data after the map.
            const FString Prefix = PrefixFor(Asset.AssetClassPath);
            if (!Prefix.IsEmpty() && !Name.StartsWith(Prefix, ESearchCase::CaseSensitive))
                AddError(FString::Printf(TEXT("%s is a %s and needs the %s prefix"), *Path, *Asset.AssetClassPath.GetAssetName().ToString(), *Prefix));
            ++Checked;
        }
    }
    TestTrue(TEXT("The project's content was found"), Checked > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FContentNamingActorsTest, "UOpenGolf.Content.Naming.Actors",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FContentNamingActorsTest::RunTest(const FString&)
{
    using namespace ContentNamingTestDetail;
    IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    int32 Maps = 0;
    for (const TCHAR* Mount : Mounts)
    {
        TArray<FAssetData> Worlds;
        Registry.GetAssetsByPath(FName(Mount), Worlds, true);
        for (const FAssetData& Asset : Worlds)
        {
            if (Asset.AssetClassPath != UWorld::StaticClass()->GetClassPathName())
                continue;
            const UWorld* World = Cast<UWorld>(Asset.GetAsset());
            if (!TestNotNull(*FString::Printf(TEXT("%s loads"), *Asset.PackageName.ToString()), World) || !World->PersistentLevel)
                continue;
            ++Maps;
            for (const AActor* Actor : World->PersistentLevel->Actors)
            {
                if (!Actor)
                    continue;
                const FString Problem = LabelProblem(*Actor);
                if (!Problem.IsEmpty())
                    AddError(FString::Printf(TEXT("%s: actor '%s' %s"), *Asset.PackageName.ToString(), *Actor->GetActorLabel(), *Problem));
            }
        }
    }
    TestTrue(TEXT("The project's maps were found"), Maps > 0);
    return true;
}
#endif
