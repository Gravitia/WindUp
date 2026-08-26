// Fill out your copyright notice in the Description page of Project Settings.

#include "Subsystem/CSObjectResetSubsystem.h"

#include "ChronoSpace.h"
#include "Engine/World.h"

UCSObjectResetSubsystem* UCSObjectResetSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		return World->GetSubsystem<UCSObjectResetSubsystem>();
	}

	return nullptr;
}

void UCSObjectResetSubsystem::Register(UCSObjectResetComponent* Component)
{
	if (IsValid(Component))
	{
		RegisteredComponents.AddUnique(Component);
	}
}

void UCSObjectResetSubsystem::Unregister(UCSObjectResetComponent* Component)
{
	RegisteredComponents.RemoveAll(
		[Component](const TWeakObjectPtr<UCSObjectResetComponent>& Entry)
		{
			// 이미 죽은 항목도 같이 걷어낸다.
			// TWeakObjectPtr 은 대상이 파괴돼도 nullptr 과 같지 않아서 Remove(nullptr) 로는 안 빠진다.
			return !Entry.IsValid() || Entry.Get() == Component;
		});
}

int32 UCSObjectResetSubsystem::ResetAll(ECSObjectResetReason Reason)
{
	int32 ResetNum = 0;

	for (int32 Index = RegisteredComponents.Num() - 1; Index >= 0; --Index)
	{
		UCSObjectResetComponent* Component = RegisteredComponents[Index].Get();
		if (!IsValid(Component))
		{
			RegisteredComponents.RemoveAtSwap(Index);
			continue;
		}

		if (Component->ResetToHome(Reason))
		{
			++ResetNum;
		}
	}

	UE_LOG(LogCS, Log, TEXT("[ObjectReset] ResetAll: %d / %d"), ResetNum, RegisteredComponents.Num());

	return ResetNum;
}
