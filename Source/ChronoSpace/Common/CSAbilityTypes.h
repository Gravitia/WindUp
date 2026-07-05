#pragma once

#include "CoreMinimal.h"
#include "CSAbilityTypes.generated.h"

/**
 * 캐릭터 능력의 종류.
 *
 * 값은 "비트 인덱스"다 (Blackhole=0, GravityCore=1). Epic 표준 Bitflags 패턴을 따라
 * 리액터의 int32 프로퍼티에 meta=(Bitmask, BitmaskEnum=...) 로 붙이면 에디터가 체크박스로
 * 다중 선택을 보여주며, 각 항목은 (1 << 값) 비트에 매핑된다. 소스가 자기 타입을 알려줄 때는
 * 이 enum 값을 그대로 반환하고, 비교 시엔 (1 << 값)으로 마스크를 만든다.
 *
 * 주의: UseEnumValuesAsMaskValuesInEditor 방식은 에디터 저장값과 런타임 값이 어긋나
 * "특정 능력만 감지 안 되는" 버그를 유발하므로 쓰지 않는다.
 */
UENUM(BlueprintType, meta = (Bitflags))
enum class ECSAbilityType : uint8
{
	Blackhole    UMETA(DisplayName = "Blackhole"),     // 0 → bit 0 (mask 1)
	GravityCore  UMETA(DisplayName = "Gravity Core"),  // 1 → bit 1 (mask 2)
};

/**
 * 활성 유지 방식.
 *  - Toggle(반복) : 능력이 닿는 동안만 활성, 떨어지면 비활성.
 *  - Latch(1회)   : 한번 활성되면 계속 유지. ResetReactor() 로 재무장.
 */
UENUM(BlueprintType)
enum class ECSReactorMode : uint8
{
	Toggle  UMETA(DisplayName = "Repeat (Toggle)"),
	Latch   UMETA(DisplayName = "Once (Latch)"),
};

/**
 * 짝(Pair) 연동 방식.
 *  - None         : 단독. 자기 조건만으로 활성.
 *  - RequiresBoth : 짝과 둘 다 조건 충족해야 활성 (AND). 2스위치 퍼즐.
 *  - Either       : 짝 중 하나라도 조건 충족하면 활성 (OR).
 */
UENUM(BlueprintType)
enum class ECSReactorPairLogic : uint8
{
	None          UMETA(DisplayName = "Alone"),
	RequiresBoth  UMETA(DisplayName = "Pair - Both (AND)"),
	Either        UMETA(DisplayName = "Pair - Either (OR)"),
};
