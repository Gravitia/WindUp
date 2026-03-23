// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSButtonIndicatorComponent.h"
#include "ActorComponent/CSMeshAffectedByGravityCore.h"
#include "ActorComponent/CSMeshPulledByBlackhole.h"
#include "Components/ChildActorComponent.h"
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
		UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ GravityCore 컴포넌트 바인딩 성공 → %s"), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ GravityCore 컴포넌트 없음 → %s"), *Owner->GetName());
	}

	if (UCSMeshPulledByBlackhole* BlackholeComp = Owner->FindComponentByClass<UCSMeshPulledByBlackhole>())
	{
		BlackholeComp->OnInteractionStarted.AddDynamic(this, &UCSButtonIndicatorComponent::OnInteractionStarted);
		BlackholeComp->OnInteractionEnded.AddDynamic(this, &UCSButtonIndicatorComponent::OnInteractionEnded);
		UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ Blackhole 컴포넌트 바인딩 성공 → %s"), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ Blackhole 컴포넌트 없음 → %s"), *Owner->GetName());
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

	// ButtonChildActors가 비어있으면 오너의 모든 ChildActorComponent를 대상으로 한다
	TArray<UChildActorComponent*> Targets;
	if (ButtonChildActors.Num() > 0)
	{
		for (UChildActorComponent* CAC : ButtonChildActors)
		{
			if (IsValid(CAC)) Targets.Add(CAC);
		}
	}
	else
	{
		Owner->GetComponents<UChildActorComponent>(Targets);
	}

	for (UChildActorComponent* CAC : Targets)
	{
		AActor* ChildActor = CAC->GetChildActor();
		if (!IsValid(ChildActor))
		{
			UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ ChildActorComponent '%s' → ChildActor 없음"), *CAC->GetName());
			continue;
		}

		UStaticMeshComponent* Mesh = ChildActor->FindComponentByClass<UStaticMeshComponent>();
		if (!IsValid(Mesh))
		{
			UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ ChildActor '%s' → StaticMeshComponent 없음"), *ChildActor->GetName());
			continue;
		}

		FButtonMeshCache Entry;
		Entry.Mesh = Mesh;
		Entry.OriginalMaterial = Mesh->GetMaterial(MaterialSlotIndex);
		CachedButtonMeshes.Add(Entry);

		UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ 버튼 메쉬 캐시 성공 → ChildActor: %s / Mesh: %s / OriginalMat: %s"),
			*ChildActor->GetName(),
			*Mesh->GetName(),
			Entry.OriginalMaterial ? *Entry.OriginalMaterial->GetName() : TEXT("None"));
	}

	if (CachedButtonMeshes.Num() > 0)
	{
		UE_LOG(LogCS, Log, TEXT("[CSButtonIndicator] ✔ 총 %d개 버튼 메쉬 캐시 완료 → Owner: %s"),
			CachedButtonMeshes.Num(), *Owner->GetName());
	}
	else
	{
		UE_LOG(LogCS, Warning, TEXT("[CSButtonIndicator] ✘ 캐시된 버튼 메쉬 없음 → Owner: %s (ChildActorComponent %d개 탐색)"),
			*Owner->GetName(), Targets.Num());
	}
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
	UE_LOG(LogCS, Log, TEXT("CSButtonIndicatorComponent: Activated on %s"), *GetOwner()->GetName());
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
	UE_LOG(LogCS, Log, TEXT("CSButtonIndicatorComponent: Deactivated on %s"), *GetOwner()->GetName());
}
