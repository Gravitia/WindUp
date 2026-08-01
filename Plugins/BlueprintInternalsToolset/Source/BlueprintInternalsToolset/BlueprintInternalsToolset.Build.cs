using UnrealBuildTool;

public class BlueprintInternalsToolset : ModuleRules
{
	public BlueprintInternalsToolset(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;
		bUseUnity = true;

		PublicDependencyModuleNames.Add("Core");

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"BlueprintGraph",
			"CoreUObject",
			"Engine",
			"ToolsetRegistry",
			"UnrealEd",
		});
	}
}
