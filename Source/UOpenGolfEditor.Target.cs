// SPDX-License-Identifier: MIT
using UnrealBuildTool;
using System.Collections.Generic;

public class UOpenGolfEditorTarget : TargetRules
{
    public UOpenGolfEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("UOpenGolf");
    }
}
