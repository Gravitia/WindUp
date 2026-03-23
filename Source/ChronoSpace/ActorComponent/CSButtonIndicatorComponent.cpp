// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSButtonIndicatorComponent.h"
#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "Components/StaticMeshComponent.h"
#include "ChronoSpace.h"

UCSButtonIndicatorComponent::UCSButtonIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCSButtonIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheButtonMeshes();
	BindToSiblingComponents();
}

void UCSButtonIndicatorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromSiblingComponents();
	Super::EndPlay(EndPlayReason);
}

void UCSButtonIndicatorComponent::BindToSiblingComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (UCSMeshAffectedByGravityCore* GravityComp = Owner->FindComponentByClass<UCSMeshAffectedByGravityCore>())
	{
		GravityComp->OnInteractionStarted.AddDynamic(this, &UCSButtonIndicatorComponent::OnInteractionStarted);
		GravityComp->OnInteractionEnded.AddDynamic(this, &UCSButtonIndicatorComponent::OnInteractionEnded);
		UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ GravityCore 바인딩 성공 → %s"), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ GravityCore 없음 → %s"), *Owner->GetName());
	}

	if (UCSMeshPulledByBlackhole* BlackholeComp = Owner->FindComponentByClass<UCSMeshPulledByBlackhole>())
	{
		BlackholeComp->OnInteractionStarted.AddDynamic(this, &UCSButtonIndicatorComponent::OnInteractionStarted);
		BlackholeComp->OnInteractionEnded.AddDynamic(this, &UCSButtonIndicatorComponent::OnInteractionEnded);
		UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ Blackhole 바인딩 성공 → %s"), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ Blackhole 없음 → %s"), *Owner->GetName());
	}
}

void UCSButtonIndicatorComponent::UnbindFromSiblingComponents()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	if (UCSMeshAffectedByGravityCore* GravityComp = Owner->FindComponentByClass<UCSMeshAffectedByGravityCore>())
	{
		GravityComp->OnInteractionStarted.RemoveDynamic(this, &UCSButtonIndicatorComponent::OnInteractionStarted);
		GravityComp->OnInteractionEnded.RemoveDynamic(this, &UCSButtonIndicatorComponent::OnInteractionEnded);
	}

	if (UCSMeshPulledByBlackhole* BlackholeComp = Owner->FindComponentByClass<UCSMeshPulledByBlackhole>())
	{
		BlackholeComp->OnInteractionStarted.RemoveDynamic(this, &UCSButtonIndicatorComponent::OnInteractionStarted);
		BlackholeComp->OnInteractionEnded.RemoveDynamic(this, &UCSButtonIndicatorComponent::OnInteractionEnded);
	}
}

void UCSButtonIndicatorComponent::CacheButtonMeshes()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

	CachedButtonMeshes.Reset();

	// ButtonMeshes가 비어있으면 이름에 "button"이 포함된 StaticMeshComponent를 자동 탐색
	if (ButtonMeshes.Num() == 0)
	{
		TArray<UStaticMeshComponent*> AllMeshes;
		Owner->GetComponents<UStaticMeshComponent>(AllMeshes);

		for (UStaticMeshComponent* Mesh : AllMeshes)
		{
			if (Mesh->GetName().Contains(TEXT("button"), ESearchCase::IgnoreCase))
			{
				ButtonMeshes.Add(Mesh);
				UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ 자동 탐색 → %s"), *Mesh->GetName());
			}
		}

		if (ButtonMeshes.Num() == 0)
		{
			UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ 'button' 포함 메쉬 없음 → %s"), *Owner->GetName());
			return;
		}
	}

	for (UStaticMeshComponent* Mesh : ButtonMeshes)
	{
		if (!IsValid(Mesh))
		{
			UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ 유효하지 않은 메쉬 항목 → %s"), *Owner->GetName());
			continue;
		}

		FButtonMeshCache Entry;
		Entry.Mesh = Mesh;
		Entry.OriginalMaterial = Mesh->GetMaterial(MaterialSlotIndex);
		CachedButtonMeshes.Add(Entry);

		UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ 메쉬 캐시 → %s / 원본 머티리얼: %s"),
			*Mesh->GetName(),
			Entry.OriginalMaterial ? *Entry.OriginalMaterial->GetName() : TEXT("None"));
	}

	UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ 총 %d개 캐시 완료 → %s"),
		CachedButtonMeshes.Num(), *Owner->GetName());
}

void UCSButtonIndicatorComponent::SetButtonMaterials(UMaterialInterface* Material)
{
	if (!IsValid(Material)) return;

	for (const FButtonMeshCache& Entry : CachedButtonMeshes)
	{
		if (Entry.Mesh.IsValid())
		{
			Entry.Mesh->SetMaterial(MaterialSlotIndex, Material);
		}
	}
}

void UCSButtonIndicatorComponent::OnInteractionStarted()
{
	SetButtonMaterials(ActivatedMaterial);
	UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ 활성화 → %s"), *GetOwner()->GetName());
}

void UCSButtonIndicatorComponent::OnInteractionEnded()
{
	for (const FButtonMeshCache& Entry : CachedButtonMeshes)
	{
		if (Entry.Mesh.IsValid())
		{
			Entry.Mesh->SetMaterial(MaterialSlotIndex, Entry.OriginalMaterial);
		}
	}
	UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ 비활성화 → %s"), *GetOwner()->GetName());
}
