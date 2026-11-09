using UnrealBuildTool;

public class FICSITRoles : ModuleRules
{
	public FICSITRoles(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "CoreOnline", "InputCore", "EnhancedInput", "Engine", "FactoryGame", "SML" });
		PrivateDependencyModuleNames.AddRange(new[] { "Projects", "Json", "AssetRegistry", "NetCore", "Slate", "SlateCore", "InputCore", "UMG", "GameplayTags" });
		if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new[] { "UnrealEd", "AssetTools" });
	}
}
