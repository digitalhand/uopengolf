// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class UOpenGolf : ModuleRules
{
    public UOpenGolf(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "Json", "OpenLaunchMonitor", "OpenFairwayPhysics", "Slate", "SlateCore" });

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "EngineSettings" });
        }
    }
}
