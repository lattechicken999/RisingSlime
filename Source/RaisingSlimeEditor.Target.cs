using UnrealBuildTool;
using System.Collections.Generic;

public class RaisingSlimeEditorTarget : TargetRules
{
	public RaisingSlimeEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("RaisingSlime");
	}
}
