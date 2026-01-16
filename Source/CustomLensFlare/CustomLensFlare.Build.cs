// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CustomLensFlare : ModuleRules
{
	public CustomLensFlare(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(new string[]
		{
			EngineDirectory + "/Source/Runtime/Renderer/Private",
			EngineDirectory + "/Source/Runtime/Renderer/Internal",
		});

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