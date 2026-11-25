// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class UOpenGolf : ModuleRules
{
    public UOpenGolf(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "UMG", "OpenLaunchMonitor", "OpenFairwayPhysics", "OpenGolfCourse", "OpenGolfEnvironment" });
        PrivateDependencyModuleNames.AddRange(new[] { "Projects", "HTTP", "InputCore", "EnhancedInput", "Json", "Slate", "SlateCore", "RenderCore", "RHI", "ProceduralMeshComponent", "OpenGolfOnline", "OpenGameUpdate",
            // UGameMapsSettings: leaving a round travels to the game's default map.
            "EngineSettings" });
        // The native folder picker in Settings › Data & system.
        if (Target.Platform == UnrealTargetPlatform.Win64)
            PublicSystemLibraries.AddRange(new[] { "Ole32.lib", "Shell32.lib" });

        RuntimeDependencies.Add("$(ProjectDir)/Content/UI/Clubs/*.png", StagedFileType.NonUFS);

        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new[] { "DesktopPlatform", "PythonScriptPlugin", "ToolMenus", "OpenGolfCourseEditor", "UnrealEd", "UMGEditor", "AssetRegistry", "AssetTools" });
        }
    }
}
