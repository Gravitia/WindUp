---
name: unreal-material
description: 언리얼 머티리얼과 텍스처를 다룰 때 사용한다. 머티리얼 그래프 노드 편집, 머티리얼 함수, 머티리얼 인스턴스 생성과 파라미터 오버라이드(색상·스칼라·텍스처), 텍스처 에셋 임포트와 압축/샘플러 설정 등.
---

# 머티리얼 · 텍스처 작업

주 툴셋:
- `editor_toolset.toolsets.material.MaterialTools` — Material, MaterialFunction 생성/편집
- `editor_toolset.toolsets.material_instance.MaterialInstanceTools` — MaterialInstanceConstant 생성/수정
- `editor_toolset.toolsets.texture.TextureTools` — 텍스처 에셋

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 머티리얼 vs 머티리얼 인스턴스

**색상, 강도, 텍스처만 바꾸는 요청이면 원본 머티리얼을 고치지 말고 머티리얼 인스턴스를 쓴다.**

- 원본 머티리얼 편집은 그 머티리얼을 쓰는 모든 곳에 영향을 주고, 셰이더 재컴파일이 오래 걸린다.
- 변경 전 `AssetTools.get_referencers`로 얼마나 많은 에셋이 이 머티리얼을 참조하는지 확인한다. 참조가 많으면 사용자에게 알리고 인스턴스 방식을 제안한다.
- 노드 구조나 새 파라미터 자체가 필요한 경우에만 원본을 건드린다.

## 표준 작업 순서

1. `AssetTools.find_assets`로 대상을 찾고 `get_asset_class`로 Material인지 MaterialInstance인지 확인한다.
2. 인스턴스라면 `MaterialInstanceTools`로 파라미터를 오버라이드한다.
3. 원본이라면 `MaterialTools`로 그래프를 편집한다.
4. `AssetTools.save_assets`로 저장한다.

## 주의

- 머티리얼 파라미터 이름은 원본 머티리얼이 노출한 것만 유효하다. 인스턴스에서 없는 이름을 설정하면 반영되지 않는다. 먼저 원본의 파라미터 목록을 확인한다.
- 세부 프로퍼티는 `ObjectTools.list_properties` → `set_properties` 경로가 확실하다.
- 텍스처 압축 설정(sRGB, Compression Settings)은 용도에 따라 다르다. 노멀맵/마스크를 컬러 설정으로 두면 시각적으로 깨진다.

## 이 프로젝트에서

- 머티리얼은 `Content/31_Material`, 피직스 머티리얼은 `Content/32_PhysicsMaterials`에 있다.
- `Content/Realistic_Starter_VFX_Pack_Vol2` 등 서드파티 팩의 머티리얼은 직접 수정하지 말고 복제해서 쓴다.
