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
			"DeveloperSettings",
            "PhysicsCore",
			"InputCore",
			"EnhancedInput",
			"ItemDataRuntime",
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"AIModule",
            "AnimGraphRuntime",
			"NavigationSystem",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"Landscape",
			"ProceduralMeshComponent",
            "Niagara",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.Add("RenderCore");
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] {
				"AssetTools",
				// Isolated equipment diagnostics: snapshots and native connection-close reasons.
				"Json",
				"NetCore",
                "NiagaraEditor",
				"Kismet",
				"UnrealEd",
                "AnimGraph",
                "BlueprintGraph",
				"UMGEditor"
			});
		}

		DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

		PublicIncludePaths.AddRange(new string[] {
			"Botanicus"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

	}
}
