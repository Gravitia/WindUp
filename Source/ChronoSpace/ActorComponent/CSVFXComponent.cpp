// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSVFXComponent.h"
#include "CSVFXComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"

UCSVFXComponent::UCSVFXComponent()
{
	
}

void UCSVFXComponent::PlayWorldVFX(EWorldVFX VFX)
{
	if (VFX == EWorldVFX::NONE) return;

    check( WorldVFXMap.Contains(VFX) );

	UNiagaraSystem* FXAsset = WorldVFXMap[VFX].LoadSynchronous();
	if (FXAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("VFX Load Fail : %d"), static_cast<int32>(VFX));
		return;
	}
	
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		GetWorld(),
		FXAsset,
		GetOwner()->GetActorLocation()
	);
}

void UCSVFXComponent::PlayActorAttackedVFX( EActorAttachedVFX VFX, FName Socket /*= NAME_None*/ )
{
	if ( VFX == EActorAttachedVFX::NONE ) return;

	check( ActorAttackedVFXMap.Contains(VFX) );

    UNiagaraSystem* FXAsset = ActorAttackedVFXMap[VFX].LoadSynchronous();
    if (!FXAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("ActorAttackedVFX Load Fail (idx: %d)"), static_cast<int32>(VFX));
        return;
    }

    USceneComponent* ParentComp = nullptr;

    // 소켓이 있을 경우 스켈레탈 메시 우선
    if (Socket != NAME_None)
    {
        if (USkeletalMeshComponent* Skel = GetOwner()->FindComponentByClass<USkeletalMeshComponent>())
            ParentComp = Skel;
    }

    // 그래도 없으면 루트로
    if (!ParentComp)
        ParentComp = GetOwner()->GetRootComponent();

    // Actor 에 부착하여 즉시 재생
    UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAttached(
        FXAsset,
        ParentComp, 
        Socket,
        FVector::ZeroVector,                 
        FRotator::ZeroRotator,               
        EAttachLocation::KeepRelativeOffset,
        true                                 
    );

    if (!NiagaraComp)
    {
        UE_LOG(LogTemp, Warning, TEXT("SpawnSystemAttached Failed"));
        return;
    }

    NiagaraComp->Activate(true);
}


