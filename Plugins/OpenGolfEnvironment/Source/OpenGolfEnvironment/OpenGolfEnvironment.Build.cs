// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGolfEnvironment : ModuleRules
{
    public OpenGolfEnvironment(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
    }
}
