// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CSCameraZoomComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class CHRONOSPACE_API UCSCameraZoomComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	UCSCameraZoomComponent();
	
	void Init( class USpringArmComponent* SpringArm, float InOrgLength );
	void ZoomCamera( float InZoomLength, float InZoomSpeed );

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UPROPERTY()
	TObjectPtr<class USpringArmComponent> CameraBoom;
	
	float OrgLength{ 0 };

	float TargetZoom{ 0 };
	float ZoomSpeed{ 1 };

private:
	bool bIsInit{ false };

	const float SpeedCoef{ 100.0f };
};
