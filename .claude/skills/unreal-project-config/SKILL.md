---
name: unreal-project-config
description: 언리얼 프로젝트 단위 설정을 다룰 때 사용한다. 프로젝트 세팅 섹션 조회와 편집, 플러그인 생성·활성화·조회, 게임 피처 플러그인 활성화/비활성화, 피직스 에셋 생성, PCG 그래프 편집 등.
---

# 프로젝트 설정 · 플러그인

주 툴셋:
- `ConfigSettingsToolset.ConfigSettingsToolset` — 프로젝트 세팅 섹션 조회/검사/편집
- `PluginToolset.PluginToolset` — 플러그인 생성, 편집, 활성화, 조회
- `GameFeaturesToolset.GameFeaturesToolset` — 게임 피처 플러그인 목록/활성화/비활성화
- `PhysicsToolsets.PhysicsAssetToolset` — 피직스 에셋 생성/관리
- `PCGToolset.PCGToolset`, `PCGToolset.PCGSpatialToolset` — PCG 그래프

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 설정 변경은 영향 범위가 넓다

`Config/`의 값은 프로젝트 전체와 팀원 모두에게 영향을 준다. 변경 전에:

1. `ConfigSettingsToolset`으로 **현재 값을 먼저 읽는다.**
2. 무엇을 무엇으로 바꾸는지 사용자에게 알린다.
3. 변경 후 `Config/` 아래 어떤 ini가 바뀌었는지 확인하고 커밋 대상으로 알린다.

일부 설정은 에디터 재시작이 필요하다. 변경했는데 반영이 안 되면 재시작을 의심한다.

## ConfigSettingsToolset vs 파일 직접 편집

- 프로젝트 세팅 UI에 있는 항목 → `ConfigSettingsToolset`이 안전하다 (유효한 섹션/키를 알려준다)
- `DefaultEngine.ini` 등을 세밀하게 손봐야 하면 → `Config/` 아래 ini를 일반 Read/Edit로 다뤄도 된다. 텍스트 파일이다.

## 플러그인

- `PluginToolset`으로 활성화 상태를 확인/변경하면 `ChronoSpace.uproject`가 바뀐다.
- 플러그인 활성화/비활성화는 에디터 재시작과 리빌드를 유발할 수 있다. 진행 전에 사용자에게 알린다.

## 이 프로젝트의 플러그인

활성화(`.uproject` 기준):

- MCP: **`ModelContextProtocol`, `MCPClientToolset`, `AllToolsets`** — Unreal MCP 연결의 실체다. **비활성화하지 않는다.** 끊기면 이 툴셋들 전부를 못 쓴다.
- 엔진: `GameplayAbilities`, `ModelingToolsEditorMode`(에디터 전용), `ActorPalette`, `BlockoutToolsPlugin`, `JsonBlueprintUtilities`, `BlueprintFileUtils`
- 프로젝트: `EOSIntegrationKit`, `AdvancedSessions`

비활성화: `AdvancedSteamSessions`, `Fab`, `Bridge`, `MetaHumanSDK`

`Plugins/` 폴더에 있다고 해서 활성 상태인 것은 아니다. `PluginToolset`이나 `.uproject`로 실제 상태를 확인하고 판단한다. 특히 `Plugins/unreal-mcp/`는 `.uplugin`이 없는 빌드 잔여물이라 로드되지 않는다.

세션/EOS 플러그인은 데디케이티드 서버 및 매치메이킹과 얽혀 있다. 관련 설정을 바꾸기 전에 `Docs/DedicatedServer_Migration.md`, `Docs/DedicatedServer_Runbook.md`를 확인한다.
