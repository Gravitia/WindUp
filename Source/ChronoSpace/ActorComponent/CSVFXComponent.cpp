// Fill out your copyright notice in the Description page of Project Settings.


#include "CSVFXComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"

UCSVFXComponent::UCSVFXComponent()
{
}

void UCSVFXComponent::PlayWorldVFX( EWorldVFX VFX )
{
	if (VFX == EWorldVFX::NONE) return;

    check( WorldVFXMap.Contains(VFX) );

	UNiagaraSystem* FXAsset = WorldVFXMap[VFX].NiagaraPtr;
	if (FXAsset == nullptr)
	{
		UE_LOG(LogTemp, Warning, TEXT("VFX Load Fail : %d"), static_cast<int32>(VFX));
		return;
	}
	
    UNiagaraComponent* NiagaraComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation( 
		GetWorld(),
		FXAsset,
		GetOwner()->GetActorLocation() + WorldVFXMap[VFX].Offset,
        GetOwner()->GetActorRotation() + FRotator(0.f, -90.f, 0.f)
	);
}

void UCSVFXComponent::PlayActorAttackedVFX( EActorAttachedVFX VFX )
{
	if ( VFX == EActorAttachedVFX::NONE ) return;

	check( ActorAttackedVFXMap.Contains(VFX) );

    UNiagaraSystem* FXAsset = ActorAttackedVFXMap[VFX].NiagaraPtr;
    if (!FXAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("ActorAttackedVFX Load Fail (idx: %d)"), static_cast<int32>(VFX));
        return;
    }

    USceneComponent* ParentComp = nullptr;

    // 소켓이 있을 경우 스켈레탈 메시 우선
    if ( ActorAttackedVFXMap[VFX].SocketName != NAME_None )
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
        ActorAttackedVFXMap[VFX].SocketName,
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


