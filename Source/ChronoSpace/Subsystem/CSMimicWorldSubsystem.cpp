// Fill out your copyright notice in the Description page of Project Settings.


#include "Subsystem/CSMimicWorldSubsystem.h"
#include "Actor/CSMimicTargetZone.h"

void UCSMimicWorldSubsystem::RegisterTargetZone(ACSMimicTargetZone* TargetZone)
{
	if (TargetZone == nullptr) return;

	TargetZonesByChannel.FindOrAdd(TargetZone->GetLinkChannel()).AddUnique(TargetZone);
}

void UCSMimicWorldSubsystem::UnregisterTargetZone(ACSMimicTargetZone* TargetZone)
{
	if (TargetZone == nullptr) return;

	if (TArray<TWeakObjectPtr<ACSMimicTargetZone>>* Zones = TargetZonesByChannel.Find(TargetZone->GetLinkChannel()))
	{
		Zones->Remove(TargetZone);
	}
}

TArray<ACSMimicTargetZone*> UCSMimicWorldSubsystem::GetTargetZones(FName LinkChannel) const
{
	TArray<ACSMimicTargetZone*> Result;

	if (const TArray<TWeakObjectPtr<ACSMimicTargetZone>>* Zones = TargetZonesByChannel.Find(LinkChannel))
	{
		for (const TWeakObjectPtr<ACSMimicTargetZone>& Zone : *Zones)
		{
			if (Zone.IsValid())
			{
				Result.Add(Zone.Get());
			}
		}
	}

	return Result;
}
