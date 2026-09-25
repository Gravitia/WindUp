# NarshaMCP · 읽기 전용 경계

NarshaMCP(Fab 이름 NarshaADK)는 **에디터 밖에서 도는 별도 MCP 서버**다. 소스와 PDB, `.uasset`을 디스크에서 직접 파싱해 인덱싱한다. 에디터가 꺼져 있어도 조회가 된다.

`unreal-mcp`(엔진 `ModelContextProtocol`)와 역할이 다르다. **기능이 겹쳐 보여도 섞지 않는다.**

## 없으면 그냥 `unreal-mcp` 로 간다

NarshaMCP 는 팀 필수가 아니다. 쓰는 사람만 Fab 에서 받는다.
**세션에 `mcp__narshamcp__*` 툴이 안 보이면 이 문서의 나머지는 적용되지 않는다.**

아래 역할 분담표를 무시하고 원래대로 한다.

- `.uasset` 조회 → 에디터를 켜고 `unreal-mcp`
- C++ 심볼 검색 → Grep / Glob
- 빌드 에러 → 빌드 로그를 직접 읽는다

없는 툴을 있는 것처럼 호출하지 않는다.

## 역할 분담

| 작업 | 도구 |
|---|---|
| C++ 심볼 검색, 호출 그래프, 리네임 영향 범위 | NarshaMCP |
| 빌드 에러 진단 | NarshaMCP |
| `.uasset` **읽기** (BP 그래프, 머티리얼, 나이아가라 등) | NarshaMCP |
| C++ ↔ 블루프린트 경계 추적 | NarshaMCP |
| **에셋 쓰기 전부** (BP·레벨·UMG·DataTable·머티리얼) | `unreal-mcp` |
| 컴파일 · 저장 | `unreal-mcp` |
| PIE, 로그, CVar, 뷰포트 스크린샷, 자동화 테스트 | `unreal-mcp` |
| 타임라인 커브 키, 커스텀 이벤트 리플리케이션 쓰기 | `BlueprintInternalsToolset` |

## NarshaMCP로 쓰지 않는다

NarshaMCP에는 `.uasset`을 디스크에서 직접 고치는 경로와, 액터 스폰·컴파일·테스트를 도는 Editor Control이 있다. **둘 다 쓰지 않는다.**

에디터를 켜 둔 채 디스크의 `.uasset`을 바꾸면 에디터는 그 변경을 모른 채 자기 메모리 버전을 들고 있다가 저장할 때 덮어쓴다. 작업이 조용히 사라지거나 에셋이 깨진다.

**"Editor Control은 어차피 동작 안 할 것"이라고 가정하지 않는다.** `NarshaMCP.uplugin`이 `RemoteControl`과 `WebSocketNetworking`을 의존 플러그인으로 선언해 두어서, NarshaMCP를 켜면 **함께 자동 활성화된다.** 안 쓰는 것은 규칙으로 지킨다.

## Remote Control 웹서버는 꺼 둔다

`RemoteControl` 은 NarshaMCP 의 C++ 의존이 아니다. `NarshaMCP.Build.cs` 가 명시한다 —
`RemoteControl: RUNTIME_ONLY via NarshaMCP.uplugin. No C++ #include; HTTP API is the Rust daemon path.`
Rust 데몬이 HTTP 경로로 쓰려고 켜는 것이고, 에디터 모듈은 RemoteControl 헤더를 하나도 include 하지 않는다.

우리는 Editor Control 을 쓰지 않으므로 그 HTTP 경로도 쓸 일이 없다. 기본값이면 에디터를 켤 때마다
포트가 두 개 열린다.

| 포트 | 용도 | 바인딩 |
|---|---|---|
| 30010 | Remote Control HTTP | `127.0.0.1` |
| 30020 | Remote Control WebSocket | **`0.0.0.0`** — 같은 네트워크의 다른 기기에서 접근 가능 |

Remote Control 은 인증 없이 에디터 오브젝트의 프로퍼티를 읽고 쓸 수 있는 API 다. 그래서
`Config/DefaultRemoteControl.ini` 에서 자동 시작을 껐다.

```ini
[/Script/RemoteControlCommon.RemoteControlSettings]
bAutoStartWebServer=False
bAutoStartWebSocketServer=False
```

narsha 의 자체 WebSocket(포트 30011, `127.0.0.1`)은 별개이며 그대로 동작한다. 이건 `WebSocketNetworking`
기반이고 narsha 가 `IWebSocketServer` 를 직접 쓰는 **진짜 의존**이다.

## `impact_analysis` 는 상속을 걷지 않는다

`depth` 파라미터를 받지만 **블루프린트 상속 링크는 따라가지 않는다.** 컴포넌트를 직접
선언한 클래스만 반환하고 그 자식은 빠진다.

실측 (`UCSMeshPulledByBlackhole`):

| | 결과 |
|---|---|
| `impact_analysis` (depth 1·2·3 전부) | **13개** |
| 실제 컴포넌트를 가진 에셋 | **45개 이상** |
| `get_hierarchy` 로 본 자식 (부모 1개 기준) | 67개 |

문제는 누락 자체보다 **출력이 완전해 보인다**는 것이다. `total_affected: 13`,
`risk_level: "high"`, `success: true` 로 끝나고 truncation 표시가 없다.

**리네임·삭제 영향 범위를 볼 때는 `impact_analysis` 결과를 그대로 믿지 않는다.**

- `get_hierarchy(direction="down", recursive=true)` 로 각 결과의 자식을 보강하거나
- `ue_grep(reference_recall=true)` 의 `[bp_var]` 행을 쓴다 (이쪽은 45개를 정확히 찾는다)

## 조회하기 전에 저장한다

NarshaMCP는 **디스크**를 읽는다. `unreal-mcp`로 고친 내용은 저장 전까지 **에디터 메모리에만** 있다.

```
BP 수정 (unreal-mcp)  →  에디터 메모리만 변경
                          디스크 .uasset = 옛 내용
                          NarshaMCP 조회 = 옛 답
     ↓ AssetTools.save_assets
                          디스크 갱신 → 인덱스 갱신 → 정상
```

그래서 `unreal-mcp`로 에셋을 수정한 뒤 NarshaMCP로 조회하려면 **`AssetTools.save_assets`를 먼저 한다.** 이건 인덱스가 게을러서가 아니라 원리적인 것이다. 저장 전 변경은 디스크에 없다.

낡은 답은 "모른다"보다 나쁘다. 확신이 안 서면 `unreal-mcp`로 다시 확인한다.

## 활성화 상태

엔진 설치본(`Engine/Plugins/Marketplace/`)이고 **Fab 계정별 라이선스**다. 머신 ID에 묶인 토큰을 쓰므로 플러그인 폴더를 복사해 팀에 배포할 수 없다. 쓸 사람이 각자 받는다.

`.uproject` 항목에 `"Optional": true`가 붙어 있어 **플러그인이 없는 팀원은 조용히 무시된다.** 없다고 에러가 나거나 프로젝트가 안 열리지 않는다.

`.mcp.json`은 git 추적 대상이 아니다. NarshaMCP가 자기 엔트리를 머신별 절대경로(플러그인·프로젝트·엔진·홈 경로)로 써넣기 때문이다. 팀이 공유하는 `unreal-mcp` 엔트리는 에디터 시작 시 `Source/ChronoSpace/Editor/CSMcpConfigBootstrap.cpp`가 채운다.
