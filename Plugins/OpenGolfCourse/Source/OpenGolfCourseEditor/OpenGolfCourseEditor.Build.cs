// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGolfCourseEditor : ModuleRules
{
    public OpenGolfCourseEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // GeometryCore is public: OpenGolfTerrainTiles.h returns its triangle index type.
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "GeometryCore", "OpenGolfCourse" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "Landscape", "MeshDescription", "StaticMeshDescription",
            "AssetRegistry", "PhysicsCore", "OpenFairwayPhysics", "PCG", "RenderCore"
        });
    }
}
