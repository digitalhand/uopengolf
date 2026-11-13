// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class UOpenGolf : ModuleRules
{
    public UOpenGolf(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "OpenLaunchMonitor", "OpenFairwayPhysics", "OpenGolfCourse" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "EnhancedInput", "Json", "Slate", "SlateCore", "RenderCore", "ProceduralMeshComponent", "OpenGolfOnline" });

        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Clubs/*.png", StagedFileType.NonUFS);

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "EngineSettings", "UMGEditor", "AssetRegistry" });
        }
    }
}
