---
name: unreal-ai
description: 언리얼의 AI와 로직 그래프 에셋을 조사할 때 사용한다. 비헤이비어 트리(BT) 구조와 태스크·데코레이터·서비스 검사, 스테이트 트리(StateTree) 에셋 조사, 월드 컨디션 구조체 확인, 대화 그래프(Conversation) 조회 등.
---

# AI · 로직 그래프 조사

주 툴셋:
- `aimodule_toolset.toolsets.behavior_tree.BehaviorTreeTools` — 비헤이비어 트리 에셋 검사
- `state_tree_toolset.toolsets.state_tree.StateTreeTools` — 스테이트 트리 에셋 검사
- `WorldConditionsToolset.WorldConditionTools` — `FWorldConditionQueryDefinition`, `FWorldConditionBase` 구조체 검사
- `conversation_toolset.toolsets.conversation.ConversationTools` — 대화 그래프(`UConversationDatabase`) 조회

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 이 툴셋들의 성격

네 툴셋 모두 **검사(inspect) 중심**이다. 트리 구조를 읽고 이해하는 데는 강하지만, 노드를 새로 만들고 배치하는 편집 기능은 제한적이다.

- "AI가 왜 이렇게 행동하지?", "이 BT 구조가 어떻게 되어 있지?" → 이 스킬
- BT 태스크/데코레이터/서비스의 **로직 자체**를 바꿔야 한다 → 그건 블루프린트나 C++ 작업이다:
  - 블루프린트로 만든 노드 → `unreal-blueprint`
  - C++ 노드 → `Source/ChronoSpace/BT`를 일반 파일 도구로 편집

## 표준 작업 순서

1. `AssetTools.find_assets`로 BT/StateTree 에셋을 찾는다 (`Content/10_BehaviorTree`).
2. 해당 툴셋으로 트리 구조를 조회한다.
3. 노드 프로퍼티는 `ObjectTools.list_properties` → `get_properties`로 확인한다.
4. 값 변경이 필요하면 `ObjectTools.set_properties`를 시도하고, 결과를 다시 읽어 실제로 반영됐는지 확인한다.
5. `AssetTools.save_assets`로 저장한다.

## 주의

- 블랙보드 키 이름은 블랙보드 에셋에 정의된 것만 유효하다. 조회 없이 이름을 지어내면 AI가 조용히 멈춘다.
- AI 동작을 실제로 확인하려면 PIE를 켜야 한다 → `unreal-editor-debug`
- 실행 중 AI 상태 로그는 `LogsToolset`으로 카테고리 verbosity를 올려서 본다.

## 이 프로젝트에서

- BT 에셋은 `Content/10_BehaviorTree`, 관련 C++는 `Source/ChronoSpace/BT`에 있다.
