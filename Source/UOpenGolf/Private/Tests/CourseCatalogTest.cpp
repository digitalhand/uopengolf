// SPDX-License-Identifier: MIT
// The bundled course's definition: found by the Asset Manager, travelled to by name, and in step with the range.
#if WITH_DEV_AUTOMATION_TESTS
#include "Course/GolfCourseSubsystem.h"
#include "Engine/AssetManager.h"
#include "Engine/Texture2D.h"
#include "GolfCourseDefinition.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Tests/BundledCourse.h"
#include "UOpenGolfRange.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourseCatalogTest, "UOpenGolf.Course.Catalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCourseCatalogTest::RunTest(const FString&)
{
    const FPrimaryAssetId Id(UGolfCourseDefinition::AssetType, TEXT("DA_SavannaRange"));
    UAssetManager* Manager = UAssetManager::GetIfInitialized();
    if (!TestNotNull(TEXT("The Asset Manager is running"), Manager))
        return false;
    TArray<FPrimaryAssetId> Ids;
    Manager->GetPrimaryAssetIdList(UGolfCourseDefinition::AssetType, Ids);
    TestTrue(TEXT("The Asset Manager lists the bundled course"), Ids.Contains(Id));

    const UGolfCourseDefinition* Course = BundledCourse::Load();
    if (!TestNotNull(TEXT("The bundled course loads (run uopengolf.CreateCourseAssets if missing)"), Course))
        return false;
    TestTrue(TEXT("Its primary asset id is its asset name"), Course->GetPrimaryAssetId() == Id);
    TestFalse(TEXT("It has a name for the browser"), Course->DisplayName.IsEmpty());
    const FString MapPackage = Course->Map.ToSoftObjectPath().GetLongPackageName();
    TestTrue(TEXT("Its map exists"), !MapPackage.IsEmpty() && FPackageName::DoesPackageExist(MapPackage));

    const UTexture2D* Preview = Course->Preview.LoadSynchronous();
    if (TestNotNull(TEXT("Its preview loads"), Preview))
    {
        TestTrue(TEXT("The preview is a UI texture"), Preview->LODGroup == TEXTUREGROUP_UI);
#if WITH_EDITORONLY_DATA
        // The import setting, not the built mips: headless runs (NullRHI) build no platform data to count.
        TestTrue(TEXT("The preview has no mipmaps"), Preview->MipGenSettings == TMGS_NoMipmaps);
#endif
    }

    // The scorecard and HUD describe the hole the range builds.
    TestEqual(TEXT("Par matches the range's hole"), Course->Par, AUOpenGolfRange::Par);
    TestEqual(TEXT("Length matches the range's hole"), Course->LengthM, AUOpenGolfRange::HoleDistanceM);
    TestEqual(TEXT("A single hole"), Course->Holes, 1);

    // The next level's game mode finds the course from the travel URL alone.
    const FString Options = UGolfCourseSubsystem::TravelOptions(*Course);
    TestEqual(TEXT("Travel names the course"), Options, FString(TEXT("Course=DA_SavannaRange")));
    TestTrue(TEXT("The URL finds the course"), UGolfCourseSubsystem::FindCourse(TEXT("?") + Options) == Course);
    TestNull(TEXT("An unknown course is Training"), UGolfCourseSubsystem::FindCourse(TEXT("?Course=DA_NoSuchCourse")));
    TestNull(TEXT("No course option is Training"), UGolfCourseSubsystem::FindCourse(TEXT("?MainMenu")));
    return true;
}
#endif
