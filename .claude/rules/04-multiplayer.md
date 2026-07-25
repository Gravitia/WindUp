# 멀티플레이 · 데디케이티드 서버

이 프로젝트는 데디케이티드 서버 구성이다. 게임플레이 코드를 쓸 때 싱글플레이를 가정하지 않는다.

## 항상 확인할 것

- 이 로직이 **서버에서 도는가, 클라이언트에서 도는가.** 권한(Authority) 체크 없이 상태를 바꾸지 않는다.
- 상태 변수를 추가했으면 리플리케이션이 필요한지 판단한다. 블루프린트 변수는 `BlueprintTools.set_variable_replication`을 잊지 않는다.
- 클라이언트에서만 보이는 연출(사운드, VFX, UI)과 서버가 소유해야 하는 상태를 섞지 않는다.

## 디버깅

증상이 "가끔 안 된다", "호스트만 된다", "클라이언트에서 다르다" 계열이면 리플리케이션이나 권한 문제를 먼저 의심한다.

PIE 로그를 볼 때 **서버 로그인지 클라이언트 로그인지 구분**한다. 구분하지 않고 읽으면 잘못된 결론에 도달한다.

## 관련 문서 · 코드

바꾸기 전에 읽는다.

- `Docs/DedicatedServer_Migration.md`
- `Docs/DedicatedServer_Runbook.md`
- `Docs/SplitScreen_Architecture_Review.md`
- `Source/ChronoSpace/Game`, `Source/ChronoSpace/Player`

## 세션 플러그인

현재 활성 경로는 **EOS Integration Kit**(`Plugins/EOSIntegrationKit-Version5`)이며, `AdvancedSessions`가 함께 있다.

`AdvancedSteamSessions`는 `.uproject`에서 **비활성화 상태**다. 폴더가 있다고 해서 쓰이고 있다고 판단하지 않는다. 루트의 `steam_appid.txt`도 마찬가지로 현재 활성 경로를 뜻하지 않는다.

세션 플러그인의 활성화 상태나 관련 `Config/` 설정을 바꾸면 접속이 통째로 깨질 수 있다. 변경 전에 사용자에게 알린다.

## MCP 플러그인은 끄지 않는다

Unreal MCP 연결의 실체는 `.uproject`에 활성화된 엔진 플러그인 `ModelContextProtocol`, `MCPClientToolset`, `AllToolsets`다. 이 중 하나라도 비활성화하면 MCP가 끊긴다.

`Plugins/unreal-mcp/`는 `.uplugin`도 `Source`도 없는 빌드 잔여물이고 git에 추적되지 않는다. 로드되지 않으므로 MCP 동작과 무관하다. MCP 문제를 이 폴더에서 조사하지 않는다.
