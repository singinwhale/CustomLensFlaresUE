// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomLensFlare : ModuleRules
{
	public CustomLensFlare(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add(EngineDirectory + "/Source/Runtime/Renderer/Private");

			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core", 
				"Renderer",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"RenderCore",
				"Projects",
				"RHICore",
				"RHI",
			}
		);
	}
}