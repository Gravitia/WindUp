// Fill out your copyright notice in the Description page of Project Settings.


#include "ActorComponent/CSProgressActivatableComponent.h"
#include "Net/UnrealNetwork.h"

UCSProgressActivatableComponent::UCSProgressActivatableComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // 컴포넌트 자체가 복제 대상이 되도록
    SetIsReplicatedByDefault(true);
}

void UCSProgressActivatableComponent::BeginPlay()
{
    Super::BeginPlay();

    // 초기값은 서버에서 결정해서 복제되게 하는 게 안전
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bIsProgressActive = bStartActive;
    }
}

void UCSProgressActivatableComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UCSProgressActivatableComponent, bIsProgressActive);
}

void UCSProgressActivatableComponent::OnRep_ProgressActive()
{
    // 클라에서 상태 바뀔 때 연출/로직 붙일 자리
    // 예: Owner의 VFX 토글, 사운드, 위젯 표시 등
}

void UCSProgressActivatableComponent::SetProgressActive(bool bInActive)
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        return; // 서버만 상태 변경
    }

    if (bIsProgressActive == bInActive)
    {
        return; // 멱등
    }

    bIsProgressActive = bInActive;

    // 즉시 전파 체감 개선
    Owner->ForceNetUpdate();
}