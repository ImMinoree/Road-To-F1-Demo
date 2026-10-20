// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RoadToF1 : ModuleRules
{
	public RoadToF1(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"ChaosVehicles",
			"PhysicsCore",
			"UMG",
			"Slate",
			"SlateCore"
		});

		PublicIncludePaths.AddRange(new string[] {
			"RoadToF1",
			"RoadToF1/SportsCar",
			"RoadToF1/OffroadCar",
			"RoadToF1/Variant_OffRoad",
			"RoadToF1/Variant_TimeTrial",
			"RoadToF1/Variant_TimeTrial/UI"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { "Json", "Chaos" });

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
