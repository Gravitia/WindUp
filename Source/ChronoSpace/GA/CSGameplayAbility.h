// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "StructUtils/InstancedStruct.h"
#include "CSGameplayAbility.generated.h"

/**
 * 프로젝트 공통 어빌리티 베이스.
 *
 * 클라에서만 아는 정보(마우스, 뷰포트, 카메라)로 도는 LocalOnly 어빌리티가
 * 서버 권한 처리를 요청할 때 쓰는 명령 릴레이를 제공한다.
 *
 * 어빌리티마다 캐릭터에 RPC 를 뚫지 않는다. 어빌리티가 정의한 USTRUCT 를
 * FInstancedStruct 로 감싸 SendServerCommand 로 보내면, ACSCharacterPlayer 의 릴레이 RPC 가
 * 서버 쪽 같은 어빌리티 인스턴스의 OnServerCommand 로 넘겨준다.
 *
 * 전제: InstancingPolicy 가 InstancedPerActor 여야 서버에 인스턴스가 있다.
 */
UCLASS()
class CHRONOSPACE_API UCSGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	/**
	 * 서버 인스턴스에서 실행된다. Payload.GetPtr<T>() 로 명령 타입을 분기한다.
	 * 클라가 보낸 값은 그대로 믿지 말고 서버에서 다시 검증(트레이스 등)한 뒤 쓴다.
	 */
	virtual void OnServerCommand(const FInstancedStruct& Payload) {}

	/**
	 * 서버가 명령을 받아들일지 결정한다. 릴레이가 OnServerCommand 전에 부른다.
	 * LocalOnly 어빌리티는 서버 스펙이 활성 상태가 아니므로 IsActive() 로 걸러선 안 된다.
	 * 기본은 아바타가 살아 있을 때만 허용. 어빌리티가 더 좁힐 수 있다.
	 */
	virtual bool CanReceiveServerCommand(const FInstancedStruct& Payload) const;

protected:
	/**
	 * 서버로 명령을 보낸다. 리슨 호스트(아바타에 권한 있음)면 RPC 없이 OnServerCommand 를 바로 부른다.
	 * bReliable=false 는 매 틱 위치 갱신처럼 유실돼도 되는 명령에 쓴다.
	 */
	void SendServerCommand(const FInstancedStruct& Payload, bool bReliable = true);
};
