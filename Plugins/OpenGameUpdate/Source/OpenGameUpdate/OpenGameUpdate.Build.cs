// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGameUpdate : ModuleRules
{
    public OpenGameUpdate(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "HTTP", "Json", "Projects" });
        // Linux updates: waits for the game to close, applies the package or archive and restarts the game.
        if (Target.Platform == UnrealTargetPlatform.Linux)
            RuntimeDependencies.Add("$(PluginDir)/Resources/Linux/apply-update.sh", StagedFileType.NonUFS);
    }
}
