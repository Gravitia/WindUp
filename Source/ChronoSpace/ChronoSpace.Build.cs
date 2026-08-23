// Fill out your copyright notice in the Description page of Project Settings.

using System;
using System.Linq;
using EpicGames.Core;
using UnrealBuildTool;
using UnrealBuildTool.Rules;

public class ChronoSpace : ModuleRules
{
	public ChronoSpace(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(new string[] { "ChronoSpace" });

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"NavigationSystem",
			"UMG",
			"DeveloperSettings",
			"GameplayTasks",
			"Slate",
			"SlateCore",
			"OnlineSubsystem",        // generic IOnlineSubsystem / IOnlineSession / IOnlineIdentity
			"OnlineSubsystemUtils",   // FBlueprintSessionResult, BP proxies
			"Niagara",
			"RenderCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"GameplayAbilities",
			"GameplayTasks",
			"GameplayTags"
		});

		// ------------------------------------------------------------------
		// Online backend selection (see Source/ChronoSpace/Subsystem/Online).
		//
		// These defines are POLICY, not linkage. Every backend is reached
		// through IOnlineSubsystem::Get(FName), which resolves the module by
		// name at runtime (OnlineSubsystemModule.cpp:22-42), so no platform
		// OSS module is ever linked here. The defines exist only so an
		// adapter for a backend that cannot work in this configuration is
		// not compiled at all.
		//
		// PublicDefinitions (not Private) because CS_WITH_* is tested in
		// headers under Subsystem/Online that other code includes; private
		// defines do not propagate to dependents.
		//
		// Always emit BOTH 1 and 0 so `#if CS_WITH_X` is always well-defined
		// and -Wundef / MSVC C4668 never fire. Same convention the engine
		// uses and explains at AIModule.Build.cs:41-52.
		// ------------------------------------------------------------------
		bool bWithEIK          = IsProjectPluginEnabled(Target, "EOSIntegrationKit");
		bool bWithSteam        = IsProjectPluginEnabled(Target, "OnlineSubsystemSteam");
		bool bWithSteamSockets = IsProjectPluginEnabled(Target, "SteamSockets");

		PublicDefinitions.Add("CS_WITH_EIK="          + (bWithEIK          ? "1" : "0"));
		PublicDefinitions.Add("CS_WITH_STEAM="        + (bWithSteam        ? "1" : "0"));
		PublicDefinitions.Add("CS_WITH_STEAMSOCKETS=" + (bWithSteamSockets ? "1" : "0"));
	}

	/// <summary>
	/// True if PluginName is enabled in the .uproject for this platform,
	/// configuration and target type.
	///
	/// Target.IsPluginEnabledForTarget(string) DOES NOT EXIST on
	/// ReadOnlyTargetRules in UE 5.8 - do not reach for it. And
	/// Target.EnablePlugins / DisablePlugins come only from the
	/// -EnablePlugin= / -DisablePlugin= command line (TargetRules.cs:865/872);
	/// they are EMPTY for plugins enabled in the .uproject, so checking them
	/// silently evaluates false and the backend is silently never compiled.
	///
	/// Plugins.GetPlugin(name) also exists but only consults PluginInfoCache
	/// and can return null depending on when module rules are constructed
	/// (Plugins.cs:265). Reading the .uproject directly has no such ordering
	/// hazard, and mirrors the allow/deny logic UBT itself applies in
	/// Plugins.IsPluginEnabledForTarget (Plugins.cs:689).
	/// </summary>
	private static bool IsProjectPluginEnabled(ReadOnlyTargetRules Target, string PluginName)
	{
		if (Target.ProjectFile == null)
		{
			return false;
		}

		try
		{
			ProjectDescriptor Project = ProjectDescriptor.FromFile(Target.ProjectFile);
			if (Project.Plugins == null)
			{
				return false;
			}

			PluginReferenceDescriptor Ref = Project.Plugins.FirstOrDefault(
				p => String.Equals(p.Name, PluginName, StringComparison.OrdinalIgnoreCase));

			// Not listed. Note this also returns false for a plugin that is
			// EnabledByDefault and omitted - fine here, because both
			// OnlineSubsystemSteam and SteamSockets ship
			// "EnabledByDefault": false and must be listed explicitly.
			if (Ref == null || !Ref.bEnabled)
			{
				return false;
			}

			// Conservative: an optional plugin may not be present at build
			// time, so do not compile an adapter that assumes it. Matches
			// Plugins.IsPluginEnabledForTarget, which skips bOptional refs.
			if (Ref.bOptional)
			{
				return false;
			}

			return Ref.IsEnabledForPlatform(Target.Platform)
				&& Ref.IsEnabledForTargetConfiguration(Target.Configuration)
				&& Ref.IsEnabledForTarget(Target.Type);
		}
		catch (Exception)
		{
			// Malformed / unreadable .uproject: fail closed rather than
			// compiling an adapter whose plugin may not be staged.
			return false;
		}
	}
}
