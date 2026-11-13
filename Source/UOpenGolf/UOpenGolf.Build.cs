// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class UOpenGolf : ModuleRules
{
    public UOpenGolf(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "OpenLaunchMonitor", "OpenFairwayPhysics", "OpenGolfCourse" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "EnhancedInput", "Json", "Slate", "SlateCore", "RenderCore", "ProceduralMeshComponent", "OpenGolfOnline", "OpenGameUpdate" });
        // The native folder picker in Settings › Data & system.
        if (Target.Platform == UnrealTargetPlatform.Win64)
            PublicSystemLibraries.AddRange(new[] { "Ole32.lib", "Shell32.lib" });

        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Clubs/*.png", StagedFileType.NonUFS);

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "EngineSettings", "UMGEditor", "AssetRegistry" });
        }
    }
}
