// Copyright 2026 Simulated Flow. All Rights Reserved.

using UnrealBuildTool;

public class HitTicker : ModuleRules
{
	public HitTicker(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Runtime only. No UMG, no UnrealEd: the numbers are the product, so every line of this module has
		// to survive a cooked Shipping build. SlateCore is here for FSlateFontInfo, nothing else.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"DeveloperSettings",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"RenderCore",
		});
	}
}
