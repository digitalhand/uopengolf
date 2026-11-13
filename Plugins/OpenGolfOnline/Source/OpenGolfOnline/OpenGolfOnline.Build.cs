// SPDX-License-Identifier: MIT
using UnrealBuildTool;

public class OpenGolfOnline : ModuleRules
{
    public OpenGolfOnline(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Json", "Slate", "SlateCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "InputCore", "HTTP", "SSL", "Sockets", "Networking" });
        // Direct curl calls also need its static dependencies when linking the editor DLL.
        AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl", "OpenSSL", "nghttp2", "zlib");
        if (Target.Platform == UnrealTargetPlatform.Win64)
            PublicSystemLibraries.Add("Advapi32.lib");
    }
}
