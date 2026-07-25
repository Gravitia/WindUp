---
name: unreal-data
description: 언리얼의 데이터 에셋을 다룰 때 사용한다. DataTable 행 추가·수정·조회, CurveTable 커브 편집, Data Asset 생성과 값 설정, StringTable 현지화 문자열, Data Registry 조회 등 밸런싱 수치나 테이블 데이터 작업.
---

# 데이터 에셋 작업

주 툴셋:
- `editor_toolset.toolsets.data_table.DataTableTools` — DataTable 생성/편집
- `editor_toolset.toolsets.curve_table.CurveTableTools` — CurveTable 생성/편집
- `editor_toolset.toolsets.data_asset.DataAssetTools` — Data Asset
- `editor_toolset.toolsets.string_table.StringTableTools` — StringTable
- `DataRegistryToolset.DataRegistryTools` — Data Registry 조회/검사

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 어떤 툴셋을 쓸지

- 행/열 형태의 테이블 → `DataTableTools`
- 레벨별 수치처럼 커브로 보간되는 값 → `CurveTableTools`
- 단일 설정 묶음 객체 → `DataAssetTools`
- UI 표시 문자열, 현지화 → `StringTableTools`
- 여러 소스를 묶어 런타임에 조회하는 레지스트리 → `DataRegistryTools`

## 표준 작업 순서

1. `AssetTools.get_asset_class`로 대상이 정말 그 타입인지 확인한다.
2. 해당 툴셋으로 현재 내용을 **먼저 읽는다.** 기존 행 구조와 컬럼 이름을 파악하지 않고 쓰면 깨진 행이 생긴다.
3. 수정한다.
4. `AssetTools.save_assets`로 저장한다.

## 주의

- DataTable은 **행 구조체(Row Struct)**에 묶여 있다. 구조체에 없는 컬럼은 쓸 수 없다. 새 컬럼이 필요하면 `Source/ChronoSpace`의 구조체 정의부터 고쳐야 하고, 그건 C++ 빌드가 필요한 작업이다.
- 행을 덮어쓰기 전에 기존 값을 읽어 사용자에게 무엇이 바뀌는지 알린다. 밸런싱 수치는 되돌리기 어렵다.
- Data Asset의 세부 프로퍼티는 `ObjectTools.list_properties` → `set_properties`로 다루는 편이 확실하다.

## 이 프로젝트에서

- `Content/04_DataAssets`, `Content/20_Data`에 데이터 에셋이 있다.
- C++ 데이터 에셋 타입은 `Source/ChronoSpace/DataAsset`에 정의된다.
