#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "CSActorAttachmentComponent.generated.h"

class UPrimitiveComponent;
class USceneComponent;

USTRUCT(BlueprintType)
struct FCSActorAttachmentSlot
{
	GENERATED_BODY()

	// The first SceneComponent on the owner with this Component Tag is used as the anchor.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor Attachment")
	FName AnchorTag = NAME_None;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Actor Attachment")
	TObjectPtr<USceneComponent> Anchor = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Actor Attachment")
	TObjectPtr<AActor> AttachedActor = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FCSActorAttachmentSlotEvent,
	FName, SlotName,
	AActor*, Actor);

UCLASS(ClassGroup = "ChronoSpace", meta = (BlueprintSpawnableComponent))
class CHRONOSPACE_API UCSActorAttachmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCSActorAttachmentComponent();

	UFUNCTION(BlueprintCallable, Category = "Actor Attachment")
	bool AttachActorToSlot(FName SlotName, AActor* Actor);

	UFUNCTION(BlueprintCallable, Category = "Actor Attachment")
	bool DetachActorFromSlot(FName SlotName);

	UFUNCTION(BlueprintCallable, Category = "Actor Attachment")
	int32 DetachAllActors();

	UFUNCTION(BlueprintCallable, Category = "Actor Attachment")
	bool SetAttachmentAnchorForSlot(FName SlotName, USceneComponent* NewAnchor);

	UFUNCTION(BlueprintCallable, Category = "Actor Attachment")
	bool ResolveAttachmentSlots();

	UFUNCTION(BlueprintPure, Category = "Actor Attachment")
	AActor* GetAttachedActorInSlot(FName SlotName) const;

	UFUNCTION(BlueprintPure, Category = "Actor Attachment")
	bool IsSlotOccupied(FName SlotName) const;

	UFUNCTION(BlueprintPure, Category = "Actor Attachment")
	FName FindSlotForActor(AActor* Actor) const;

	UPROPERTY(BlueprintAssignable, Category = "Actor Attachment|Events")
	FCSActorAttachmentSlotEvent OnActorAttached;

	UPROPERTY(BlueprintAssignable, Category = "Actor Attachment|Events")
	FCSActorAttachmentSlotEvent OnActorDetached;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor Attachment")
	TMap<FName, FCSActorAttachmentSlot> AttachmentSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor Attachment")
	bool bDisablePhysicsWhileAttached = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor Attachment")
	bool bDisableCollisionWhileAttached = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor Attachment")
	bool bRestoreStateOnDetach = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Actor Attachment")
	bool bRequireAuthority = true;

private:
	struct FPrimitiveState
	{
		TWeakObjectPtr<UPrimitiveComponent> Component;
		ECollisionEnabled::Type CollisionEnabled = ECollisionEnabled::NoCollision;
		bool bSimulatingPhysics = false;
	};

	bool CanModifyAttachment() const;
	bool ResolveAttachmentSlot(FName SlotName, FCSActorAttachmentSlot& Slot);
	TArray<FPrimitiveState> SaveAndDisablePrimitiveStates(AActor* Actor);
	void RestorePrimitiveStates(const TArray<FPrimitiveState>& PrimitiveStates);

	TMap<FName, TArray<FPrimitiveState>> SavedPrimitiveStatesBySlot;
};
