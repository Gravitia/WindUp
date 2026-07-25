---
name: unreal-level-actor
description: 언리얼 레벨과 액터를 다룰 때 사용한다. 레벨 열기·저장, 액터 배치·삭제·복제, 트랜스폼(위치/회전/스케일) 변경, 액터 라벨과 아웃라이너 정리, 부모-자식 어태치, 컴포넌트 추가·조회, 레벨 카메라 이동, 프리미티브 지오메트리 배치 등.
---

# 레벨 · 액터 작업

주 툴셋:
- `editor_toolset.toolsets.scene.SceneTools` — 레벨 로드, 액터 배치/제거, 아웃라이너, 레벨 카메라
- `editor_toolset.toolsets.actor.ActorTools` — 액터 트랜스폼, 라벨, 부모-자식, 컴포넌트
- `editor_toolset.toolsets.primitive.PrimitiveTools` — 프리미티브 지오메트리 컴포넌트 추가

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 어떤 툴셋을 쓸지

- "레벨에 무언가를 놓거나 없앤다", "어떤 레벨을 연다" → `SceneTools`
- "이미 있는 액터의 위치/이름/계층/컴포넌트를 바꾼다" → `ActorTools`
- "큐브·스피어 같은 임시 지오메트리를 붙인다" → `PrimitiveTools`

## 표준 작업 순서

1. `SceneTools`로 대상 레벨을 연다. 지금 열린 레벨을 그대로 쓸 거면 생략.
2. `SceneTools`로 현재 레벨의 액터를 조회해 대상 액터 참조를 얻는다.
3. `ActorTools`로 트랜스폼·프로퍼티를 수정한다.
4. 세부 프로퍼티는 `ObjectTools.list_properties` → `get_properties` → `set_properties` 순서로 다룬다.
5. `AssetTools.save_assets`로 레벨을 저장한다.

## 주의

- **레벨 변경은 되돌리기 번거롭다.** 액터를 삭제하거나 레벨을 통째로 바꾸기 전에 무엇을 지울지 사용자에게 알린다.
- 액터 프로퍼티 이름은 클래스마다 다르다. `ObjectTools.list_properties`로 확인하고 쓴다. 추측한 이름으로 `set_properties`를 호출하면 조용히 실패한다.
- 이 프로젝트는 World Partition/External Actors를 쓴다. 레벨을 저장하면 `Content/__ExternalActors__` 아래 파일이 함께 변한다. 정상이며 직접 편집하지 않는다.
- 레벨 에셋은 `Content/02_Map`, `Content/Maps`, 레벨 인스턴스는 `Content/50_LevelInstance`에 있다.

## 관련 스킬

- 배치할 액터의 블루프린트 자체를 고쳐야 하면 → `unreal-blueprint`
- 플레이 중 상태를 확인하려면 → `unreal-editor-debug` (PIE 제어, 로그)
