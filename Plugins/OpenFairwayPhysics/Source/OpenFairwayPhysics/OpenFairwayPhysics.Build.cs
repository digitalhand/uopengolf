// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenFairwayPhysics : ModuleRules
{
    public OpenFairwayPhysics(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // The OpenFairway core (Public/OpenFairway) uses only Core; the ball actor adds Engine.
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
