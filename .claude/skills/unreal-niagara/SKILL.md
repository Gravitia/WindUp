---
name: unreal-niagara
description: 언리얼 나이아가라 VFX를 다룰 때 사용한다. 나이아가라 시스템과 이미터·모듈 편집, 파티클 파라미터 조정, 나이아가라 컴포넌트의 유저 변수 오버라이드, 나이아가라 스크립트/모듈 에셋 검색, 나이아가라 이펙트를 감싸는 블루프린트 액터 생성 등.
---

# 나이아가라 VFX 작업

주 툴셋:
- `NiagaraToolsets.NiagaraToolset_System` — 시스템/이미터/모듈 편집 (나이아가라의 기본 진입점)
- `NiagaraToolsets.NiagaraToolset_Component` — 레벨/블루프린트의 나이아가라 컴포넌트, 런타임 조작, 유저 변수 오버라이드
- `NiagaraToolsets.NiagaraToolset_Assets` — 나이아가라 스크립트/모듈 에셋 검색
- `NiagaraToolsets.NiagaraToolset_Blueprint` — 나이아가라 시스템을 감싸는 블루프린트 액터 생성
- `NiagaraToolsets.NiagaraToolset_Info` — enum 값 조회, 타입 관련 참고 정보

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 툴셋 고르기

- 이펙트 에셋 자체의 구조를 바꾼다 → `NiagaraToolset_System`
- 이미 배치된 이펙트의 색/크기/속도만 조정한다 → `NiagaraToolset_Component`의 유저 변수 오버라이드
- 쓸 만한 모듈을 찾는다 → `NiagaraToolset_Assets`
- enum 값이나 타입이 헷갈린다 → `NiagaraToolset_Info`를 먼저 부른다

## 표준 작업 순서

1. `NiagaraToolset_System`의 스키마/토폴로지 조회로 **현재 구조를 먼저 읽는다.** Summary와 Topology 조회가 그 용도다.
2. 필요한 모듈을 `NiagaraToolset_Assets.FindNiagaraScripts`로 찾는다.
3. `NiagaraToolset_System.AddModule`로 스택에 넣는다.
4. 파라미터를 설정한다.
5. `AssetTools.save_assets`로 저장한다.

## 주의

- `FindNiagaraScripts` 결과에는 메타데이터가 빠져 있다. `FAssetData` JSON 직렬화가 태그 맵을 포함하지 않기 때문이다. 디코딩된 메타데이터가 필요하면 결과 행을 `GetNiagaraScriptDigest`로 한 번 더 조회해야 한다.
- 시스템 구조를 모른 채 모듈을 추가하면 스택 순서가 꼬여 이펙트가 안 보인다. 반드시 먼저 읽는다.
- 의존성 확인은 `GetSystemDependencies`, 다이내믹 인풋 체인은 `GetDynamicInputChain`으로 추적한다.

## 이 프로젝트에서

- 나이아가라 에셋은 `Content/34_Niagara`에 있다.
- `Content/Realistic_Starter_VFX_Pack_Vol2`는 서드파티 VFX 팩이다. 직접 수정하지 말고 복제해서 쓴다.
