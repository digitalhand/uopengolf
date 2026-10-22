// SPDX-License-Identifier: MIT
using UnrealBuildTool;
using System.Collections.Generic;

public class UOpenGolfTarget : TargetRules
{
    public UOpenGolfTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("UOpenGolf");
    }
}
