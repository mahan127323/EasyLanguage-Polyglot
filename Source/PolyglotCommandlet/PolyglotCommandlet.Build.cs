// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

using UnrealBuildTool;

public class PolyglotCommandlet : ModuleRules
{
	public PolyglotCommandlet(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"PolyglotCore",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"JsonUtilities",
			});
	}
}
