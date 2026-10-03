// Copyright 2026 BaiMaoMaLou. All Rights Reserved.

using UnrealBuildTool;

public class PolyglotAdapters : ModuleRules
{
	public PolyglotAdapters(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Note: no NiagaraEditor / MaterialEditor / BlueprintGraph / ContentBrowser
		// dependency here on purpose. Those editors are probed at runtime so this
		// module compiles against any 5.x editor without extra linkage.

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
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
			});
	}
}
