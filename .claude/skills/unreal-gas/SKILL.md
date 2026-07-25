---
name: unreal-gas
description: 언리얼 게임플레이 어빌리티 시스템(GAS)과 게임플레이 태그를 다룰 때 사용한다. AttributeSet과 어트리뷰트 조회, 어빌리티 시스템 컴포넌트 런타임 상태 확인, 게임플레이 큐 실행과 노티파이 에셋 관리, 게임플레이 태그 추가·조회 등.
---

# GAS · 게임플레이 태그

주 툴셋:
- `GASToolsets.AttributeSetToolset` — AttributeSet 클래스와 어트리뷰트 탐색
- `GASToolsets.AbilitySystemInspectorToolset` — 런타임 ASC 상태 조회
- `GASToolsets.GameplayCueToolset` — 게임플레이 큐 조회/실행/노티파이 관리
- `GameplayTagsToolset.GameplayTagsToolset` — 게임플레이 태그 읽기/관리

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 어떤 툴셋을 쓸지

- "어떤 어트리뷰트가 있는지", "AttributeSet 구조" → `AttributeSetToolset`
- "지금 이 캐릭터의 태그/어트리뷰트 값이 뭔지" (플레이 중 디버깅) → `AbilitySystemInspectorToolset`
- "이펙트 큐가 안 터진다", "큐 노티파이 확인" → `GameplayCueToolset`
- "태그를 추가/확인" → `GameplayTagsToolset`

## AbilitySystemInspectorToolset 주의

각 함수는 **액터 포인터를 직접** 받는다. 액터가 null이거나 AbilitySystemComponent가 없으면 스크립트 에러가 난다.

- 액터 참조는 `SceneTools`/`ActorTools`로 먼저 얻는다 → `unreal-level-actor`
- 런타임 상태 조회이므로 **PIE가 실행 중이어야 의미가 있다.** 에디터 정지 상태에서 부르면 빈 결과다. PIE 시작은 `EditorAppToolset` → `unreal-editor-debug`

## 게임플레이 태그

- 태그는 `Config/`의 태그 ini에 저장된다. 툴셋으로 추가한 뒤 `Config/` 변경분을 확인하면 커밋 대상이 보인다.
- 태그 이름은 계층 구조다. 새로 만들기 전에 조회해서 기존 네이밍 규칙을 따른다.

## 어빌리티/이펙트 에셋 자체를 고칠 때

GAS 툴셋은 대체로 **조회와 실행** 위주다. 어빌리티 블루프린트나 게임플레이 이펙트의 로직·기본값을 바꾸려면:

- 그래프/변수 → `BlueprintTools` (`unreal-blueprint`)
- 클래스 기본값 → `BlueprintTools.get_default_object` + `ObjectTools.set_properties`

## 이 프로젝트에서

- GAS 관련 C++는 `Source/ChronoSpace/GA`, 어트리뷰트는 `Source/ChronoSpace/Attribute`에 있다.
- 태그 설정은 `Config/`에 있다.
