// Blueprint Internals Toolset.

#include "BlueprintInternalsToolset.h"

#include "Components/TimelineComponent.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveVector.h"
#include "Engine/Blueprint.h"
#include "Engine/TimelineTemplate.h"
#include "K2Node_CustomEvent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"

namespace
{
	const TCHAR* const VectorComponents[3] = { TEXT("X"), TEXT("Y"), TEXT("Z") };
	const TCHAR* const ColorComponents[4] = { TEXT("R"), TEXT("G"), TEXT("B"), TEXT("A") };

	template <typename EnumType>
	FString EnumToString(EnumType Value)
	{
		if (const UEnum* Enum = StaticEnum<EnumType>())
		{
			return Enum->GetNameStringByValue(static_cast<int64>(Value));
		}
		return FString::FromInt(static_cast<int32>(Value));
	}

	/// Maps a replication mode to the engine net function flag. Returns 0 for NotReplicated.
	uint32 ToNetFlag(EBlueprintCustomEventReplication Replication)
	{
		switch (Replication)
		{
		case EBlueprintCustomEventReplication::Multicast:         return FUNC_NetMulticast;
		case EBlueprintCustomEventReplication::RunOnServer:       return FUNC_NetServer;
		case EBlueprintCustomEventReplication::RunOnOwningClient: return FUNC_NetClient;
		default:                                                  return 0;
		}
	}

	EBlueprintCustomEventReplication FromFunctionFlags(uint32 FunctionFlags)
	{
		if ((FunctionFlags & FUNC_Net) == 0)
		{
			return EBlueprintCustomEventReplication::NotReplicated;
		}
		if (FunctionFlags & FUNC_NetMulticast) { return EBlueprintCustomEventReplication::Multicast; }
		if (FunctionFlags & FUNC_NetServer)    { return EBlueprintCustomEventReplication::RunOnServer; }
		if (FunctionFlags & FUNC_NetClient)    { return EBlueprintCustomEventReplication::RunOnOwningClient; }
		return EBlueprintCustomEventReplication::NotReplicated;
	}

	void ReadCurve(const FRichCurve& Curve, const TCHAR* Component, FBlueprintTimelineTrack& OutTrack)
	{
		FBlueprintTimelineCurve& CurveInfo = OutTrack.Curves.AddDefaulted_GetRef();
		CurveInfo.Component = Component ? Component : TEXT("");

		for (const FRichCurveKey& Key : Curve.Keys)
		{
			FBlueprintTimelineKey& KeyInfo = CurveInfo.Keys.AddDefaulted_GetRef();
			KeyInfo.Time = Key.Time;
			KeyInfo.Value = Key.Value;
			KeyInfo.InterpMode = EnumToString(Key.InterpMode.GetValue());
		}
	}

	/// Fills in the fields shared by every track type.
	FBlueprintTimelineTrack& BeginTrack(
		FBlueprintTimelineInfo& Info,
		const FTTTrackBase& Track,
		const TCHAR* TrackType,
		const UObject* Curve)
	{
		FBlueprintTimelineTrack& TrackInfo = Info.Tracks.AddDefaulted_GetRef();
		TrackInfo.TrackName = Track.GetTrackName().ToString();
		TrackInfo.TrackType = TrackType;
		if (Curve && Track.bIsExternalCurve)
		{
			TrackInfo.ExternalCurvePath = Curve->GetPathName();
		}
		return TrackInfo;
	}

	/// Locates the FRichCurve backing a track component, and the curve object owning it so the
	/// caller can call Modify() on it. Returns nullptr when the track or component is unknown.
	FRichCurve* FindCurve(
		UBlueprint* Blueprint,
		const FString& TimelineName,
		const FString& TrackName,
		const FString& Component,
		UObject*& OutCurveOwner,
		bool& bOutIsExternal)
	{
		OutCurveOwner = nullptr;
		bOutIsExternal = false;

		for (const TObjectPtr<UTimelineTemplate>& Template : Blueprint->Timelines)
		{
			if (!Template || Template->GetVariableName().ToString() != TimelineName)
			{
				continue;
			}

			for (FTTFloatTrack& Track : Template->FloatTracks)
			{
				if (Track.GetTrackName().ToString() != TrackName || !Track.CurveFloat)
				{
					continue;
				}
				OutCurveOwner = Track.CurveFloat;
				bOutIsExternal = Track.bIsExternalCurve;
				return &Track.CurveFloat->FloatCurve;
			}

			for (FTTVectorTrack& Track : Template->VectorTracks)
			{
				if (Track.GetTrackName().ToString() != TrackName || !Track.CurveVector)
				{
					continue;
				}
				for (int32 Index = 0; Index < 3; ++Index)
				{
					if (Component.Equals(VectorComponents[Index], ESearchCase::IgnoreCase))
					{
						OutCurveOwner = Track.CurveVector;
						bOutIsExternal = Track.bIsExternalCurve;
						return &Track.CurveVector->FloatCurves[Index];
					}
				}
				return nullptr;
			}

			for (FTTLinearColorTrack& Track : Template->LinearColorTracks)
			{
				if (Track.GetTrackName().ToString() != TrackName || !Track.CurveLinearColor)
				{
					continue;
				}
				for (int32 Index = 0; Index < 4; ++Index)
				{
					if (Component.Equals(ColorComponents[Index], ESearchCase::IgnoreCase))
					{
						OutCurveOwner = Track.CurveLinearColor;
						bOutIsExternal = Track.bIsExternalCurve;
						return &Track.CurveLinearColor->FloatCurves[Index];
					}
				}
				return nullptr;
			}
		}
		return nullptr;
	}
}

TArray<FBlueprintTimelineInfo> UBlueprintInternalsToolset::GetTimelines(UBlueprint* Blueprint)
{
	TArray<FBlueprintTimelineInfo> Result;

	if (!Blueprint)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Blueprint cannot be null."));
		return Result;
	}

	for (const TObjectPtr<UTimelineTemplate>& Template : Blueprint->Timelines)
	{
		if (!Template)
		{
			continue;
		}

		FBlueprintTimelineInfo& Info = Result.AddDefaulted_GetRef();
		Info.TimelineName = Template->GetVariableName().ToString();
		Info.Length = Template->TimelineLength;
		Info.LengthMode = EnumToString(Template->LengthMode.GetValue());
		Info.bAutoPlay = Template->bAutoPlay != 0;
		Info.bLoop = Template->bLoop != 0;
		Info.bReplicated = Template->bReplicated != 0;

		for (const FTTFloatTrack& Track : Template->FloatTracks)
		{
			FBlueprintTimelineTrack& TrackInfo = BeginTrack(Info, Track, TEXT("Float"), Track.CurveFloat);
			if (Track.CurveFloat)
			{
				ReadCurve(Track.CurveFloat->FloatCurve, nullptr, TrackInfo);
			}
		}

		for (const FTTVectorTrack& Track : Template->VectorTracks)
		{
			FBlueprintTimelineTrack& TrackInfo = BeginTrack(Info, Track, TEXT("Vector"), Track.CurveVector);
			if (Track.CurveVector)
			{
				for (int32 Index = 0; Index < 3; ++Index)
				{
					ReadCurve(Track.CurveVector->FloatCurves[Index], VectorComponents[Index], TrackInfo);
				}
			}
		}

		for (const FTTLinearColorTrack& Track : Template->LinearColorTracks)
		{
			FBlueprintTimelineTrack& TrackInfo = BeginTrack(Info, Track, TEXT("LinearColor"), Track.CurveLinearColor);
			if (Track.CurveLinearColor)
			{
				for (int32 Index = 0; Index < 4; ++Index)
				{
					ReadCurve(Track.CurveLinearColor->FloatCurves[Index], ColorComponents[Index], TrackInfo);
				}
			}
		}

		for (const FTTEventTrack& Track : Template->EventTracks)
		{
			FBlueprintTimelineTrack& TrackInfo = BeginTrack(Info, Track, TEXT("Event"), nullptr);
			TrackInfo.EventFunctionName = Track.GetFunctionName().ToString();
		}
	}

	return Result;
}

void UBlueprintInternalsToolset::SetTimelineKeyValue(
	UBlueprint* Blueprint,
	const FString& TimelineName,
	const FString& TrackName,
	const FString& Component,
	int32 KeyIndex,
	float NewValue)
{
	if (!Blueprint)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Blueprint cannot be null."));
		return;
	}

	UObject* CurveOwner = nullptr;
	bool bIsExternal = false;
	FRichCurve* Curve = FindCurve(Blueprint, TimelineName, TrackName, Component, CurveOwner, bIsExternal);

	if (!Curve || !CurveOwner)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("No curve for timeline '%s', track '%s', component '%s' in '%s'. ")
			TEXT("Call GetTimelines to see the available tracks and components."),
			*TimelineName, *TrackName, *Component, *Blueprint->GetPathName()));
		return;
	}

	if (bIsExternal)
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("Track '%s' uses the external curve asset '%s'. Edit that asset instead, ")
			TEXT("since changing it affects every user of the curve."),
			*TrackName, *CurveOwner->GetPathName()));
		return;
	}

	if (!Curve->Keys.IsValidIndex(KeyIndex))
	{
		UKismetSystemLibrary::RaiseScriptError(FString::Printf(
			TEXT("Key index %d is out of range. That curve has %d keys."),
			KeyIndex, Curve->Keys.Num()));
		return;
	}

	CurveOwner->Modify();
	Curve->Keys[KeyIndex].Value = NewValue;
	CurveOwner->PostEditChange();

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}

FBlueprintCustomEventReplicationInfo UBlueprintInternalsToolset::GetCustomEventReplication(UK2Node_CustomEvent* Node)
{
	FBlueprintCustomEventReplicationInfo Info;

	if (!Node)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Node cannot be null."));
		return Info;
	}

	Info.EventName = Node->CustomFunctionName.ToString();
	Info.Replication = FromFunctionFlags(Node->FunctionFlags);
	Info.bReliable = (Node->FunctionFlags & FUNC_NetReliable) != 0;

	return Info;
}

void UBlueprintInternalsToolset::SetCustomEventReplication(
	UK2Node_CustomEvent* Node,
	EBlueprintCustomEventReplication Replication,
	bool bReliable)
{
	if (!Node)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Node cannot be null."));
		return;
	}

	UBlueprint* Blueprint = Node->GetBlueprint();
	if (!Blueprint)
	{
		UKismetSystemLibrary::RaiseScriptError(TEXT("Node does not belong to a Blueprint."));
		return;
	}

	// Mirrors FBlueprintGraphActionDetails::SetNetFlags, which is what the Details panel uses.
	const uint32 NetFlag = ToNetFlag(Replication);
	const int32 FlagsToClear = FUNC_Net | FUNC_NetMulticast | FUNC_NetServer | FUNC_NetClient;
	const int32 FlagsToSet = NetFlag ? (FUNC_Net | NetFlag) : 0;

	Node->Modify();
	Node->FunctionFlags &= ~FlagsToClear;
	Node->FunctionFlags |= FlagsToSet;

	// Reliability only means anything on a replicated event.
	if (NetFlag != 0 && bReliable)
	{
		Node->FunctionFlags |= FUNC_NetReliable;
	}
	else
	{
		Node->FunctionFlags &= ~FUNC_NetReliable;
	}

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
}
