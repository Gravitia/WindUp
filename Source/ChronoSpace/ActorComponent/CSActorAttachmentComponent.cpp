#include "ActorComponent/CSActorAttachmentComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

UCSActorAttachmentComponent::UCSActorAttachmentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCSActorAttachmentComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveAttachmentSlots();
}

bool UCSActorAttachmentComponent::AttachActorToSlot(FName SlotName, AActor* Actor)
{
	if (!CanModifyAttachment() || !IsValid(Actor) || FindSlotForActor(Actor) != NAME_None)
	{
		return false;
	}

	FCSActorAttachmentSlot* Slot = AttachmentSlots.Find(SlotName);
	if (!Slot || IsValid(Slot->AttachedActor))
	{
		return false;
	}

	if (!IsValid(Slot->Anchor) && !ResolveAttachmentSlot(SlotName, *Slot))
	{
		return false;
	}

	if (Actor == GetOwner() || Actor->GetRootComponent() == nullptr)
	{
		return false;
	}

	TArray<FPrimitiveState> PrimitiveStates = SaveAndDisablePrimitiveStates(Actor);

	const bool bAttached = Actor->AttachToComponent(
		Slot->Anchor,
		FAttachmentTransformRules::SnapToTargetNotIncludingScale);

	if (!bAttached)
	{
		RestorePrimitiveStates(PrimitiveStates);
		return false;
	}

	Slot->AttachedActor = Actor;
	SavedPrimitiveStatesBySlot.Add(SlotName, MoveTemp(PrimitiveStates));
	OnActorAttached.Broadcast(SlotName, Actor);
	return true;
}

bool UCSActorAttachmentComponent::DetachActorFromSlot(FName SlotName)
{
	if (!CanModifyAttachment())
	{
		return false;
	}

	FCSActorAttachmentSlot* Slot = AttachmentSlots.Find(SlotName);
	if (!Slot || !IsValid(Slot->AttachedActor))
	{
		return false;
	}

	AActor* ActorToDetach = Slot->AttachedActor;
	ActorToDetach->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	if (bRestoreStateOnDetach)
	{
		if (const TArray<FPrimitiveState>* PrimitiveStates = SavedPrimitiveStatesBySlot.Find(SlotName))
		{
			RestorePrimitiveStates(*PrimitiveStates);
		}
	}

	Slot->AttachedActor = nullptr;
	SavedPrimitiveStatesBySlot.Remove(SlotName);
	OnActorDetached.Broadcast(SlotName, ActorToDetach);
	return true;
}

int32 UCSActorAttachmentComponent::DetachAllActors()
{
	if (!CanModifyAttachment())
	{
		return 0;
	}

	TArray<FName> OccupiedSlotNames;
	for (const TPair<FName, FCSActorAttachmentSlot>& Pair : AttachmentSlots)
	{
		if (IsValid(Pair.Value.AttachedActor))
		{
			OccupiedSlotNames.Add(Pair.Key);
		}
	}

	int32 DetachedCount = 0;
	for (const FName SlotName : OccupiedSlotNames)
	{
		DetachedCount += DetachActorFromSlot(SlotName) ? 1 : 0;
	}

	return DetachedCount;
}

bool UCSActorAttachmentComponent::SetAttachmentAnchorForSlot(
	FName SlotName,
	USceneComponent* NewAnchor)
{
	if (!IsValid(NewAnchor) || NewAnchor->GetOwner() != GetOwner())
	{
		return false;
	}

	FCSActorAttachmentSlot* Slot = AttachmentSlots.Find(SlotName);
	if (!Slot || IsValid(Slot->AttachedActor))
	{
		return false;
	}

	Slot->Anchor = NewAnchor;
	return true;
}

bool UCSActorAttachmentComponent::ResolveAttachmentSlots()
{
	bool bAllResolved = true;

	for (TPair<FName, FCSActorAttachmentSlot>& Pair : AttachmentSlots)
	{
		if (!ResolveAttachmentSlot(Pair.Key, Pair.Value))
		{
			bAllResolved = false;
		}
	}

	return bAllResolved;
}

AActor* UCSActorAttachmentComponent::GetAttachedActorInSlot(FName SlotName) const
{
	const FCSActorAttachmentSlot* Slot = AttachmentSlots.Find(SlotName);
	return Slot ? Slot->AttachedActor : nullptr;
}

bool UCSActorAttachmentComponent::IsSlotOccupied(FName SlotName) const
{
	return IsValid(GetAttachedActorInSlot(SlotName));
}

FName UCSActorAttachmentComponent::FindSlotForActor(AActor* Actor) const
{
	if (!IsValid(Actor))
	{
		return NAME_None;
	}

	for (const TPair<FName, FCSActorAttachmentSlot>& Pair : AttachmentSlots)
	{
		if (Pair.Value.AttachedActor == Actor)
		{
			return Pair.Key;
		}
	}

	return NAME_None;
}

bool UCSActorAttachmentComponent::CanModifyAttachment() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && (!bRequireAuthority || Owner->HasAuthority());
}

bool UCSActorAttachmentComponent::ResolveAttachmentSlot(
	FName SlotName,
	FCSActorAttachmentSlot& Slot)
{
	AActor* Owner = GetOwner();
	if (!IsValid(Owner) || Slot.AnchorTag.IsNone())
	{
		return false;
	}

	Slot.Anchor = Owner->FindComponentByTag<USceneComponent>(Slot.AnchorTag);
	if (!IsValid(Slot.Anchor))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[CSActorAttachmentComponent] '%s': Slot '%s' could not find SceneComponent tag '%s'."),
			*Owner->GetName(),
			*SlotName.ToString(),
			*Slot.AnchorTag.ToString());
		return false;
	}

	return true;
}

TArray<UCSActorAttachmentComponent::FPrimitiveState>
UCSActorAttachmentComponent::SaveAndDisablePrimitiveStates(AActor* Actor)
{
	TArray<FPrimitiveState> PrimitiveStates;
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Primitive : PrimitiveComponents)
	{
		if (!IsValid(Primitive))
		{
			continue;
		}

		FPrimitiveState& State = PrimitiveStates.AddDefaulted_GetRef();
		State.Component = Primitive;
		State.CollisionEnabled = Primitive->GetCollisionEnabled();
		State.bSimulatingPhysics = Primitive->IsSimulatingPhysics();

		if (bDisablePhysicsWhileAttached && State.bSimulatingPhysics)
		{
			Primitive->SetSimulatePhysics(false);
		}

		if (bDisableCollisionWhileAttached)
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	return PrimitiveStates;
}

void UCSActorAttachmentComponent::RestorePrimitiveStates(
	const TArray<FPrimitiveState>& PrimitiveStates)
{
	for (const FPrimitiveState& State : PrimitiveStates)
	{
		UPrimitiveComponent* Primitive = State.Component.Get();
		if (!IsValid(Primitive))
		{
			continue;
		}

		Primitive->SetCollisionEnabled(State.CollisionEnabled);
		Primitive->SetSimulatePhysics(State.bSimulatingPhysics);
	}
}
