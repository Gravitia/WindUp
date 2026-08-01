// Blueprint Internals Toolset.

#pragma once

#include "CoreMinimal.h"

#include "ToolsetRegistry/ToolsetDefinition.h"

#include "BlueprintInternalsToolset.generated.h"

class UBlueprint;
class UK2Node_CustomEvent;

/// Replication mode of a Blueprint custom event, matching the "Replicates" dropdown
/// in the event node's Details panel.
UENUM(BlueprintType)
enum class EBlueprintCustomEventReplication : uint8
{
	/// Not replicated. The event runs only on the machine that calls it.
	NotReplicated,
	/// Called on the server, executed on the server and on every client that has the actor relevant.
	Multicast,
	/// Called on a client, executed on the server.
	RunOnServer,
	/// Called on the server, executed on the owning client.
	RunOnOwningClient
};

/// A single keyframe on a Timeline curve.
USTRUCT(BlueprintType)
struct FBlueprintTimelineKey
{
	GENERATED_BODY()

	/// Time of the key in seconds.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	float Time = 0.0f;

	/// Value of the curve at this key.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	float Value = 0.0f;

	/// Interpolation mode leaving this key, e.g. "RCIM_Cubic".
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString InterpMode;
};

/// One curve of a Timeline track. Float tracks have a single curve; vector tracks have
/// three; linear color tracks have four.
USTRUCT(BlueprintType)
struct FBlueprintTimelineCurve
{
	GENERATED_BODY()

	/// Curve component. Empty for a float track, "X"/"Y"/"Z" for a vector track,
	/// "R"/"G"/"B"/"A" for a linear color track. Pass this to SetTimelineKeyValue.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString Component;

	/// Keyframes in time order.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	TArray<FBlueprintTimelineKey> Keys;
};

/// A track on a Timeline. The track name is what appears on the Timeline node's output pin;
/// vector and linear color tracks appear as several pins suffixed with the component name.
USTRUCT(BlueprintType)
struct FBlueprintTimelineTrack
{
	GENERATED_BODY()

	/// Track name, matching the Timeline node's output pin (minus any component suffix).
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString TrackName;

	/// "Float", "Vector", "LinearColor" or "Event".
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString TrackType;

	/// Asset path of the curve when the track uses an external curve asset.
	/// Empty when the curve is stored inside the Blueprint.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString ExternalCurvePath;

	/// Name of the function an event track fires. Empty for other track types.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString EventFunctionName;

	/// Curves making up this track. Empty for event tracks.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	TArray<FBlueprintTimelineCurve> Curves;
};

/// A Timeline defined on a Blueprint.
USTRUCT(BlueprintType)
struct FBlueprintTimelineInfo
{
	GENERATED_BODY()

	/// Timeline variable name, matching the Timeline node in the graph.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString TimelineName;

	/// Length of the Timeline in seconds.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	float Length = 0.0f;

	/// How the length is determined, e.g. "TL_TimelineLength" or "TL_LastKeyFrame".
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	FString LengthMode;

	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	bool bAutoPlay = false;

	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	bool bLoop = false;

	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	bool bReplicated = false;

	/// Every track on this Timeline: float, vector, linear color and event.
	UPROPERTY(BlueprintReadWrite, Category = "Timeline")
	TArray<FBlueprintTimelineTrack> Tracks;
};

/// Current replication state of a custom event node.
USTRUCT(BlueprintType)
struct FBlueprintCustomEventReplicationInfo
{
	GENERATED_BODY()

	/// Name of the custom event.
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	FString EventName;

	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	EBlueprintCustomEventReplication Replication = EBlueprintCustomEventReplication::NotReplicated;

	/// Whether the RPC is reliable. Meaningless when Replication is NotReplicated.
	UPROPERTY(BlueprintReadWrite, Category = "Blueprint")
	bool bReliable = false;
};

/// Provides tools for Blueprint internals that the standard BlueprintTools toolset does not
/// expose: Timeline track curves, and the replication flags on custom event nodes.
///
/// These fields are declared as bare UPROPERTY() in the engine, so they are invisible to
/// generic property tools such as ObjectTools.get_properties. This toolset reads and writes
/// them directly in C++.
UCLASS(BlueprintType, Hidden)
class UBlueprintInternalsToolset : public UToolsetDefinition
{
	GENERATED_BODY()

public:
	/**
	 * Returns every Timeline defined on a Blueprint, including the keyframes of every
	 * float, vector and linear color track. Use this instead of reading curve values off
	 * the Timeline editor UI.
	 * @param Blueprint The Blueprint to read. Raises a script error if null.
	 * @return One entry per Timeline, in the order they are stored on the Blueprint.
	 */
	UFUNCTION(meta = (AICallable), Category = "Blueprint")
	static TArray<FBlueprintTimelineInfo> GetTimelines(UBlueprint* Blueprint);

	/**
	 * Sets the value of a single keyframe on a Timeline curve.
	 * The Blueprint is marked structurally modified, so compile and save afterwards.
	 * This changes gameplay-visible values, so it should only be called after getting
	 * explicit direction from the user.
	 * @param Blueprint The Blueprint that owns the Timeline.
	 * @param TimelineName The Timeline variable name as returned by GetTimelines.
	 * @param TrackName The track name as returned by GetTimelines.
	 * @param Component The curve component as returned by GetTimelines: empty for a float
	 *   track, "X"/"Y"/"Z" for a vector track, "R"/"G"/"B"/"A" for a linear color track.
	 * @param KeyIndex Zero-based index of the key, in the order returned by GetTimelines.
	 * @param NewValue The value to assign to that key.
	 */
	UFUNCTION(meta = (AICallable), Category = "Blueprint")
	static void SetTimelineKeyValue(
		UBlueprint* Blueprint,
		const FString& TimelineName,
		const FString& TrackName,
		const FString& Component,
		int32 KeyIndex,
		float NewValue);

	/**
	 * Returns the replication mode and reliability of a custom event node.
	 * @param Node The custom event node. Raises a script error if null.
	 */
	UFUNCTION(meta = (AICallable), Category = "Blueprint")
	static FBlueprintCustomEventReplicationInfo GetCustomEventReplication(UK2Node_CustomEvent* Node);

	/**
	 * Sets the replication mode and reliability of a custom event node, the same way the
	 * "Replicates" dropdown in the Details panel does.
	 * The Blueprint is marked structurally modified, so compile and save afterwards.
	 * @param Node The custom event node to modify. Raises a script error if null.
	 * @param Replication The replication mode to apply.
	 * @param bReliable Whether the RPC is reliable. Ignored when Replication is NotReplicated.
	 */
	UFUNCTION(meta = (AICallable), Category = "Blueprint")
	static void SetCustomEventReplication(
		UK2Node_CustomEvent* Node,
		EBlueprintCustomEventReplication Replication,
		bool bReliable);
};
