// SPDX-License-Identifier: MIT
#if WITH_DEV_AUTOMATION_TESTS

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Misc/AutomationTest.h"
#include "OpenGolfTerrain.h"
#include "OpenGolfTerrainBuilder.h"

namespace SurfaceRebuildTestDetail
{
    AStaticMeshActor* MakeGround(const TCHAR* MeshName, std::initializer_list<FName> Tags)
    {
        auto* Actor = NewObject<AStaticMeshActor>(GetTransientPackage());
        Actor->Tags.Append(Tags.begin(), static_cast<int32>(Tags.size()));
        if (MeshName)
            Actor->GetStaticMeshComponent()->SetStaticMesh(NewObject<UStaticMesh>(GetTransientPackage(), MeshName));
        return Actor;
    }
} // namespace SurfaceRebuildTestDetail

// Rebuilding the surfaces swaps only the generated ground meshes, matched to the definition's surfaces by the
// index BuildTerrain gave each mesh. Vegetation, trees and Landscapes keep their actors.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSurfaceRebuildTest, "UOpenGolf.Course.SurfaceRebuild",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSurfaceRebuildTest::RunTest(const FString& Parameters)
{
    namespace Detail = SurfaceRebuildTestDetail;
    using Area = AOpenGolfTerrainArea;
    auto Index = [](const AActor* Actor) { return UOpenGolfTerrainBuilder::SurfaceMeshIndex(*Actor); };

    TestEqual(TEXT("The default surface is the first mesh"), Index(Detail::MakeGround(TEXT("SM_Surface_0"), {Area::GeneratedTag})), 0);
    TestEqual(TEXT("A region surface keeps its build index"), Index(Detail::MakeGround(TEXT("SM_Surface_3"), {Area::GeneratedTag})), 3);
    TestEqual(TEXT("Ground cover is not a surface"), Index(Detail::MakeGround(TEXT("SM_Surface_1"), {Area::GeneratedTag, Area::VegetationTag})), INDEX_NONE);
    TestEqual(TEXT("Untagged actors are not generated ground"), Index(Detail::MakeGround(TEXT("SM_Surface_2"), {})), INDEX_NONE);
    TestEqual(TEXT("Other meshes are not surfaces"), Index(Detail::MakeGround(TEXT("SM_Rock_2"), {Area::GeneratedTag})), INDEX_NONE);
    TestEqual(TEXT("A malformed index is rejected"), Index(Detail::MakeGround(TEXT("SM_Surface_x"), {Area::GeneratedTag})), INDEX_NONE);
    TestEqual(TEXT("An empty actor is not a surface"), Index(Detail::MakeGround(nullptr, {Area::GeneratedTag})), INDEX_NONE);
    return true;
}
#endif
