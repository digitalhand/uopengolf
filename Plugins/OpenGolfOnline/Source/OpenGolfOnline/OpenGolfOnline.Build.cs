// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGolfOnline : ModuleRules
{
    public OpenGolfOnline(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "Slate", "SlateCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "HTTP", "SSL", "Sockets", "Networking" });
        AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl", "OpenSSL");
        if (Target.Platform == UnrealTargetPlatform.Win64)
            PublicSystemLibraries.Add("Advapi32.lib");
    }
}
