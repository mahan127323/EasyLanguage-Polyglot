// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

using UnrealBuildTool;

public class PolyglotCore : ModuleRules
{
	public PolyglotCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"EditorSubsystem",
				"InputCore",
				"Json",
				"JsonUtilities",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			});
	}
}
