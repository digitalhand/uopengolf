// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class UOpenGolf : ModuleRules
{
    public UOpenGolf(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "Json", "OpenLaunchMonitor", "OpenFairwayPhysics", "OpenGolfCourse", "Slate", "SlateCore", "RenderCore", "ProceduralMeshComponent" });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "EngineSettings", "UMGEditor", "AssetRegistry" });
        }
    }
}
