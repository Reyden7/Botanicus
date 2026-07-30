// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Botanicus : ModuleRules
{
	public Botanicus(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreOnline",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ItemDataRuntime",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Landscape",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		PublicIncludePaths.AddRange(new string[] {
			"Botanicus"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

	}
}
