---
name: unreal-asset
description: 언리얼 프로젝트의 에셋을 찾거나 관리할 때 사용한다. 에셋 검색, 경로 확인, 이름 변경·이동·복제·삭제, 저장, 폴더 생성, 의존성/참조 조회, 메타데이터 태그, 소스 컨트롤 체크아웃 상태 확인 등.
---

# 에셋 검색 · 관리

주 툴셋: `editor_toolset.toolsets.asset.AssetTools`
보조 툴셋: `SemanticSearchToolset.SemanticSearchToolset` (의미 기반 검색)

공통 규칙은 `unreal-mcp` 스킬 참고.

## 에셋 찾기

```
find_assets(folder_path, name, asset_type?, recursive?, tags?)
```

- `folder_path`에 빈 문자열을 주면 `/Game/`과 모든 플러그인 콘텐츠까지 전부 뒤진다. 느리므로 폴더를 좁혀 준다.
- `name`은 대소문자 구분 없는 부분 일치다.
- `asset_type`은 `{"refPath": "/Script/Engine.StaticMesh"}` 같은 클래스 참조다.
- 이름을 정확히 모르고 "이런 역할을 하는 에셋"을 찾는 상황이면 `SemanticSearchToolset`의 하이브리드 검색이 더 낫다.

경로가 맞는지 애매하면 `exists`, `list_folders`로 먼저 확인한다.

## 조회

- `get_asset_class` — 에셋 타입 확인. 어떤 툴셋을 쓸지 정할 때 먼저 부른다.
- `load_asset` — 에셋을 로드해 `{"refPath": ...}`를 얻는다. `ObjectTools`에 넘길 때 필요.
- `get_dependencies` / `get_referencers` — 삭제·이동 전에 **반드시** 확인한다.
- `get_asset_tags`, `get_metadata_tags`, `update_metadata_tags`

## 변경 (되돌리기 어려움 — 먼저 확인할 것)

- `move` — 이동/이름 변경
- `duplicate` — 복제
- `delete` — 에셋/폴더 삭제
- `create_folder`

`move`와 `delete`는 참조가 깨질 수 있다. 실행 전에 `get_referencers`로 참조하는 에셋을 확인하고, 결과를 사용자에게 알린 뒤 진행한다.

## 저장

```
save_assets([])        # dirty 에셋 전체 저장
save_assets([path, ...])  # 지정 에셋만 저장
```

에셋을 수정하는 모든 작업의 마지막 단계다. `is_dirty`로 미저장 여부를 확인할 수 있다.

## 소스 컨트롤

- `can_edit_asset` — 다른 사람이 잠갔는지 확인. 편집 전에 부르면 헛수고를 막는다.
- `is_checked_out`

## 파일 읽기/쓰기

`read_file` / `write_file`은 `/Game/`, 활성 플러그인의 `Content/`, 프로젝트 `Saved/` 아래의 **plain text 파일만** 다룬다. `.uasset`에는 쓸 수 없다. 프로젝트 소스나 Config를 다룰 거면 그냥 일반 Read/Edit 도구를 쓴다.

## 이 프로젝트의 콘텐츠 배치

`Content/01_Blueprint`(BP), `02_Map`·`Maps`(레벨), `03_Input`, `04_DataAssets`, `10_BehaviorTree`, `20_Data`, `30_Mesh`, `31_Material`, `34_Niagara`, `40_Audio`, `60_Character` 등 번호 접두 폴더로 나뉜다. 검색 시 이 폴더로 범위를 좁힌다.

`Content/__ExternalActors__`, `__ExternalObjects__`는 에디터가 관리하는 데이터다. 직접 건드리지 않는다.
