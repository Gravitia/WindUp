// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Common/CSAbilityTypes.h"
#include "CSAbilitySource.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI)
class UCSAbilitySource : public UInterface
{
	GENERATED_BODY()
};

/**
 * 캐릭터 능력 액터(블랙홀 / 그래비티코어 등)가 구현하는 인터페이스.
 * 리액터가 구체 클래스로 캐스팅하지 않고 "무슨 능력인지"만 물어볼 수 있게 한다.
 */
class CHRONOSPACE_API ICSAbilitySource
{
	GENERATED_BODY()

public:
	// 이 소스가 어떤 능력인지(단일 비트) 반환.
	virtual ECSAbilityType GetAbilityType() const = 0;
};
