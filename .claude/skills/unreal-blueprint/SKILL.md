---
name: unreal-blueprint
description: 언리얼 블루프린트를 만들거나 수정할 때 사용한다. 노드 그래프 편집, 핀 연결, 변수·함수·이벤트·이벤트 디스패처 추가, 부모 클래스 변경, 컴파일, 리플리케이션 설정 등 BP 작업 전반. .uasset 블루프린트를 읽거나 쓰라는 요청이면 이 스킬을 쓴다.
---

# 블루프린트 작업

주 툴셋: `editor_toolset.toolsets.blueprint.BlueprintTools`
보조 툴셋: `editor_toolset.toolsets.object.ObjectTools` (프로퍼티/기본값), `editor_toolset.toolsets.asset.AssetTools` (저장)

블루프린트 `.uasset`을 Read/Edit로 직접 건드리지 않는다. 공통 규칙은 `unreal-mcp` 스킬 참고.

## 표준 작업 순서

1. `AssetTools.find_assets`로 대상 블루프린트 경로를 찾는다.
2. `list_graphs` → `get_graph`로 어떤 그래프가 있는지 파악한다.
3. 그래프 내용을 읽을 때는 `read_graph_dsl`이 노드 단위 조회보다 훨씬 빠르다.
4. 수정한다 (아래 참고).
5. `compile_blueprint`로 컴파일한다. **생략하면 변경이 반영되지 않는다.**
6. `AssetTools.save_assets`로 저장한다.

## 그래프 편집: DSL을 우선 쓴다

- `get_graph_dsl_docs` — DSL 문법을 먼저 확인한다. 처음 쓸 때는 반드시 호출한다.
- `read_graph_dsl` — 그래프 전체를 텍스트로 읽는다.
- `write_graph_dsl` — 그래프를 텍스트로 통째로 쓴다.

노드를 여러 개 만들고 연결해야 하면 `create_node` + `connect_pins`를 반복하는 것보다 `write_graph_dsl` 한 번이 훨씬 안정적이다. 핀이 엉뚱한 곳에 연결되는 사고도 줄어든다.

## 노드 단위 편집이 필요할 때

- 탐색: `find_nodes`, `get_node_infos`, `get_connected_subgraph`
- 노드 타입 찾기: `find_node_types`, `find_node_categories`, `get_node_type_pins`
- 생성/삭제: `create_node`, `delete_node`, `retarget_node_class`
- 핀: `connect_pins`, `break_pins`, `get_pin_value`, `set_pin_value`, `add_node_pin`, `remove_node_pin`
- 배치: `arrange_nodes`, `set_node_position`

`create_node` 전에 `get_node_type_pins`로 핀 이름을 확인한다. 핀 이름은 추측하지 않는다.

## 변수

- 조회: `list_variables`, `get_variable_category`, `get_variable_replication`
- 추가: `add_variable` (기본 타입), `add_object_variable` (오브젝트 참조), `add_struct_variable` (구조체)
- 설정: `set_variable_category`, `set_variable_instance_editable`, `set_variable_replication`
- 삭제: `remove_variable`

멀티플레이 관련 변수는 `set_variable_replication`을 잊지 않는다. 이 프로젝트는 데디케이티드 서버 구성이다.

## 함수 · 이벤트

- 함수: `list_functions`, `add_function_graph`, `remove_function_graph`
- 함수 파라미터: `add_function_param`, `add_object_function_param`, `add_struct_function_param`, `remove_function_param`
- 이벤트: `list_events`, `add_event`, `list_compatible_event_functions`
- 컴포넌트 이벤트 바인딩: `list_component_events`, `add_component_bound_event`
- 이벤트 디스패처: `list_event_dispatchers`, `add_event_dispatcher`
- 커스텀 이벤트 연결: `get_create_event_function`, `set_create_event_function`

`BeginPlay` 같은 기본 이벤트가 그래프에 없으면 `add_event`로 먼저 추가해야 한다. 있다고 가정하지 않는다.

## 클래스 · 기본값

- 새 블루프린트: `create`
- 부모 클래스: `get_parent`, `set_parent`
- 클래스 기본값(CDO): `get_default_object` → 반환된 오브젝트에 `ObjectTools.set_properties`

CDO 프로퍼티를 바꿀 때는 먼저 `ObjectTools.list_properties`로 정확한 이름을 확인한다. 이름을 추측하면 조용히 실패한다.

## 이 프로젝트에서

- 블루프린트는 주로 `Content/01_Blueprint/` 아래에 있다.
- C++ 베이스 클래스는 `Source/ChronoSpace/` 아래에 있고, 그쪽은 일반 파일 도구로 읽는다.
