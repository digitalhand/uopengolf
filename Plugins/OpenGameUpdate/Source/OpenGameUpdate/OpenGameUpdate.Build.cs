// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGameUpdate : ModuleRules
{
    public OpenGameUpdate(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "HTTP", "Json" });
    }
}
