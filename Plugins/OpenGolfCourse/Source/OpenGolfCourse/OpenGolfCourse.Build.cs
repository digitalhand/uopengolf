// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGolfCourse : ModuleRules
{
    public OpenGolfCourse(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // The flagstick draws its cloth flag with a procedural mesh.
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "ProceduralMeshComponent", "OpenFairwayPhysics" });
        // Surface assets carry OpenFairway physical materials; the asset tests check them.
        PrivateDependencyModuleNames.AddRange(new[] { "PhysicsCore" });
    }
}
