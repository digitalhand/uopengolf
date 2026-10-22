// SPDX-License-Identifier: MIT
using System;
using System.IO;
using System.Linq;
using System.Reflection;
using UnrealBuildTool;

public class OpenLaunchMonitor : ModuleRules
{
    public OpenLaunchMonitor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new[] { "Json" });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // The Bluetooth LE transport uses the Windows SDK's C++/WinRT projection.
            // It is added to this module only, so the shared build environment is unchanged.
            PublicSystemIncludePaths.Add(FindCppWinRTIncludeDir(Target));
            PublicSystemLibraries.Add("WindowsApp.lib");
            bEnableExceptions = true;
        }
        // Linux loads sd-bus from the host's libsystemd.so.0 at runtime, so nothing is linked here.
    }

    private static string FindCppWinRTIncludeDir(ReadOnlyTargetRules Target)
    {
        string SdkDir = Target.WindowsPlatform.WindowsSdkDir;
        if (string.IsNullOrEmpty(SdkDir))
        {
            throw new BuildException("OpenLaunchMonitor: the Windows SDK directory is unknown.");
        }

        // Prefer the SDK version UnrealBuildTool selected so C++/WinRT matches the other SDK headers.
        object Environment = Target.WindowsPlatform.GetType()
            .GetProperty("Environment", BindingFlags.Instance | BindingFlags.NonPublic | BindingFlags.Public)?
            .GetValue(Target.WindowsPlatform);
        string SelectedVersion = Environment?.GetType().GetField("WindowsSdkVersion")?.GetValue(Environment)?.ToString();
        string IncludeRoot = Path.Combine(SdkDir, "Include");
        if (!string.IsNullOrEmpty(SelectedVersion))
        {
            string Selected = Path.Combine(IncludeRoot, SelectedVersion, "cppwinrt");
            if (Directory.Exists(Selected))
            {
                return Selected;
            }
        }

        string Fallback = Directory.Exists(IncludeRoot)
            ? Directory.GetDirectories(IncludeRoot)
                .Where(Dir => Directory.Exists(Path.Combine(Dir, "cppwinrt")))
                .OrderByDescending(Dir => Version.TryParse(Path.GetFileName(Dir), out Version Parsed) ? Parsed : new Version(0, 0))
                .Select(Dir => Path.Combine(Dir, "cppwinrt"))
                .FirstOrDefault()
            : null;
        if (Fallback == null)
        {
            throw new BuildException("OpenLaunchMonitor: no Windows SDK C++/WinRT headers (Include/<version>/cppwinrt) were found.");
        }
        return Fallback;
    }
}
