// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

using UnrealBuildTool;

public class PolyglotUI : ModuleRules
{
	public PolyglotUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"UnrealEd",
				"PolyglotCore",
				"PolyglotAdapters",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ToolMenus",
				"InputCore",
				"WorkspaceMenuStructure",
				"DeveloperSettings",
				"EditorStyle",
				"PropertyEditor",
				"DesktopPlatform",
				"Json",
				"Projects",
				"ImageWrapper",
				"ImageCore",
				});
	}
}
