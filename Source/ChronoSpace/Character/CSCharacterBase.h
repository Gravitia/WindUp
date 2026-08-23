// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CSCharacterBase.generated.h"

UCLASS()
class CHRONOSPACE_API ACSCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this character's properties
	ACSCharacterBase();

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 사망/부활은 서버가 결정하고 bIsDead 복제로 모든 머신에 적용된다.
	// (예전엔 호출된 머신에서만 바뀌어서, 킬존 오버랩이 서버/클라 양쪽에서 각자 SetDead 를 부르는 데 의존했다.)
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	virtual void SetDead();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly)
	virtual void SetRevive();

	UFUNCTION(BlueprintPure)
	bool IsDead() const { return bIsDead; }

protected:
	UFUNCTION()
	void OnRep_IsDead();

	// 로컬 적용 (모든 머신에서 실행) - 서브클래스는 이 둘을 오버라이드한다
	virtual void HandleDead();
	virtual void HandleRevive();

	UPROPERTY(ReplicatedUsing = OnRep_IsDead)
	bool bIsDead = false;

	UPROPERTY()
	TObjectPtr<class UCSCustomGravityDirComponent> CustomGravityDirComponent;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "VFX")
	TObjectPtr< class UCSVFXComponent > VFXComponent;
}; 
