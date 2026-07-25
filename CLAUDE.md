# ChronoSpace

Unreal Engine 5.8 프로젝트. 게임 모듈은 `Source/ChronoSpace` (단일 Runtime 모듈), 프로젝트 파일은 `ChronoSpace.uproject`.

- 데디케이티드 서버 기반 멀티플레이. 게임플레이 코드에서 싱글플레이를 가정하지 않는다.
- GAS(`GameplayAbilities`), UMG, AIModule을 사용한다.
- 온라인 세션은 **EOS Integration Kit** 경로다. `AdvancedSteamSessions`는 비활성화되어 있다.
- 실행 중인 에디터를 Unreal MCP로 직접 조작할 수 있다. 연결 실체는 엔진 플러그인 `ModelContextProtocol` / `MCPClientToolset` / `AllToolsets`이며, `.mcp.json`이 `http://127.0.0.1:8000/mcp`를 가리킨다. 에디터가 꺼져 있으면 응답하지 않는다.

## 규칙

아래 규칙은 항상 적용된다.

@.claude/rules/01-unreal-assets.md
@.claude/rules/02-generated-output.md
@.claude/rules/03-project-layout.md
@.claude/rules/04-multiplayer.md
@.claude/rules/05-editor-workflow.md

## 스킬

언리얼 에디터 작업은 `.claude/skills/` 아래 스킬을 사용한다. 어떤 툴셋을 쓸지 모르겠으면 **`unreal-mcp` 스킬의 라우팅 표를 먼저 본다.**

`unreal-blueprint` · `unreal-asset` · `unreal-level-actor` · `unreal-umg` · `unreal-gas` · `unreal-data` · `unreal-material` · `unreal-animation` · `unreal-niagara` · `unreal-ai` · `unreal-editor-debug` · `unreal-test` · `unreal-project-config`

## 참고

- `AGENTS.md` — 폴더별 상세 인벤토리(Source/Content/Plugins 전체 목록). 구조를 파악해야 할 때 본다.
- `Docs/DedicatedServer_Migration.md`, `Docs/DedicatedServer_Runbook.md` — 데디케이티드 서버 구성/운영
- `Docs/SplitScreen_Architecture_Review.md` — 스플릿스크린 구조
