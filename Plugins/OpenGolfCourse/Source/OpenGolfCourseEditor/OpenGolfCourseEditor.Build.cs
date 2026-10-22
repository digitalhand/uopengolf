// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGolfCourseEditor : ModuleRules
{
    public OpenGolfCourseEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "OpenGolfCourse" });
        PrivateDependencyModuleNames.AddRange(new[] {
            "UnrealEd", "Landscape", "GeometryCore", "MeshDescription", "StaticMeshDescription",
            "AssetRegistry", "PhysicsCore", "OpenFairwayPhysics", "PCG", "RenderCore"
        });
    }
}
