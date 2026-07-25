---
name: unreal-umg
description: 언리얼 UMG UI 위젯을 만들거나 수정할 때 사용한다. 위젯 블루프린트 생성, 위젯 트리 조작, 버튼·텍스트·이미지 등 위젯 추가와 배치, 슬롯 설정, 앵커·패딩·정렬 조정 등 UI 작업.
---

# UMG 위젯 작업

주 툴셋: `UMGToolSet.UMGToolSet`
필수 짝: `editor_toolset.toolsets.object.ObjectTools`

공통 규칙은 `unreal-mcp` 스킬 참고.

## 반드시 지킬 순서

UMG 툴셋이 반환하는 **모든 위젯과 슬롯**에 대해 프로퍼티를 다루려면:

1. `ObjectTools.list_properties(widget)` — 정확한 프로퍼티 이름을 먼저 확인한다.
2. `ObjectTools.get_properties(widget, [...])` — 현재 값을 읽는다.
3. `ObjectTools.set_properties(widget, {...})` — 값을 설정한다.

**1번을 건너뛰면 안 된다.** 프로퍼티 이름은 위젯 클래스마다 다르고 추측할 수 없다. 틀린 이름으로 `set_properties`를 호출하면 에러 없이 조용히 실패하거나 엉뚱한 프로퍼티가 바뀐다. 이건 UMG 툴셋 자체가 명시한 요구사항이다.

## 참조 전달

UMG 툴셋은 UObject 포인터를 `{"refPath": "..."}` 형태로 돌려준다. 반환된 Widget/Slot/Parent 참조를 그대로 `ObjectTools`나 UMG 툴셋의 다음 호출에 넘긴다. 경로 문자열을 새로 만들어 넣지 않는다.

## 위젯 트리 다루기

위젯은 트리 구조다. 부모 위젯(Canvas Panel, Vertical Box, Overlay 등)에 자식을 붙이면 **슬롯 객체**가 생기고, 위치·크기·앵커·패딩은 위젯이 아니라 그 **슬롯**의 프로퍼티다. 위젯에서 위치 프로퍼티를 찾다 없으면 슬롯을 보라는 신호다.

## 표준 작업 순서

1. `AssetTools.find_assets`로 위젯 블루프린트를 찾는다.
2. UMG 툴셋으로 위젯 트리를 조회해 구조를 파악한다.
3. 위젯을 추가/이동한다.
4. 위 3단계 규칙대로 위젯과 슬롯 프로퍼티를 설정한다.
5. 로직(바인딩, 클릭 이벤트)은 `BlueprintTools`로 그래프를 편집한다 → `unreal-blueprint`
6. `BlueprintTools.compile_blueprint` → `AssetTools.save_assets`

## 이 프로젝트에서

- UI C++ 베이스 클래스는 `Source/ChronoSpace/UI`에 있다.
- `Content/EasyGameUI`는 서드파티 UI 팩이다. 수정 전에 프로젝트 코드에서 파생해 쓰는지 확인한다.
- 과거에 UI 블루프린트에서 핀이 잘못 연결되거나 `BeginPlay`가 누락되는 문제가 있었다. 그래프 편집 후 `read_graph_dsl`로 결과를 다시 읽어 확인한다.
