---
name: unreal-mcp
description: 언리얼 에디터 작업(블루프린트, .uasset, 레벨, 액터, 머티리얼, 애니메이션, UI, GAS, 나이아가라 등)을 시작할 때 어떤 unreal-mcp 툴셋을 쓸지 고르는 라우팅 인덱스와 모든 툴셋 공통 호출 규칙. 언리얼 관련 작업이면 먼저 이 스킬을 읽는다.
---

# unreal-mcp 기본 규칙 · 툴셋 라우팅

## 전제 조건

- `.mcp.json`의 `unreal-mcp` 서버(`http://127.0.0.1:8000/mcp`)는 **언리얼 에디터가 실행 중일 때만** 응답한다.
- 연결 확인은 `list_toolsets` 한 번이면 충분하다. 에러가 나면 툴 문제가 아니라 에디터가 꺼진 것이다.
- **블루프린트, `.uasset`, `.umap` 등 언리얼 에셋은 Read/Edit/Write로 직접 다루지 않는다.** 반드시 이 MCP를 통한다. (바이너리이며 직접 편집 시 에셋이 깨진다.)
- 예외: `Source/`의 C++ 코드, `Config/`의 `.ini`는 일반 파일 도구로 다룬다.

## 호출 형식

모든 툴은 `call_tool` 하나로 호출한다.

```
call_tool(
  toolset_name = "editor_toolset.toolsets.blueprint.BlueprintTools",
  tool_name    = "compile_blueprint",          # 툴셋 접두사 없이 마지막 이름만
  arguments    = { ... }
)
```

- `tool_name`에 전체 경로를 넣으면 실패한다. 마지막 세그먼트만 쓴다.
- UObject/UClass 파라미터는 항상 `{"refPath": "/Game/01_Blueprint/BP_Foo.BP_Foo"}` 형태의 객체다. 문자열을 그대로 넣으면 안 된다.
- 툴이 UObject를 반환하면 `{"refPath": "..."}`로 돌아온다. 그 값을 그대로 다음 툴에 넘긴다.

## 스키마 확인 시 주의

`describe_toolset`은 출력이 매우 크다 (`BlueprintTools`는 약 72,000자로 컨텍스트 한도를 넘겨 파일로 덤프된다).

- 도구 **이름만** 알고 싶으면 덤프 파일을 Grep으로 `"name":"<toolset>\.([a-z_]+)"` 패턴 검색한다.
- 특정 도구의 인자 스키마만 필요하면 덤프 파일에서 그 도구 이름 주변을 Grep한다.
- 작은 툴셋(`ObjectTools`, `AssetTools` 등)은 그냥 `describe_toolset` 해도 된다.

## 작업 후 마무리

1. 블루프린트를 수정했으면 `BlueprintTools.compile_blueprint`로 컴파일한다.
2. `AssetTools.save_assets`로 저장한다. 빈 리스트를 넘기면 dirty 에셋 전체가 저장된다.
3. 저장하지 않으면 에디터를 닫을 때 변경분이 사라진다.

## 어떤 작업에 어떤 스킬/툴셋인가

| 하려는 작업 | 스킬 | 주요 툴셋 |
|---|---|---|
| 블루프린트 그래프/변수/함수/이벤트 편집 | `unreal-blueprint` | `BlueprintTools`, `ObjectTools` |
| 에셋 찾기·이동·복제·삭제·저장·의존성 조회 | `unreal-asset` | `AssetTools`, `SemanticSearchToolset` |
| 레벨 열기, 액터 배치·트랜스폼·컴포넌트 | `unreal-level-actor` | `SceneTools`, `ActorTools`, `PrimitiveTools` |
| UMG 위젯 트리, UI 블루프린트 | `unreal-umg` | `UMGToolSet`, `ObjectTools` |
| GAS 어빌리티·어트리뷰트·큐, 게임플레이 태그 | `unreal-gas` | `GASToolsets.*`, `GameplayTagsToolset` |
| DataTable/CurveTable/DataAsset/StringTable | `unreal-data` | `DataTableTools`, `CurveTableTools`, `DataAssetTools`, `StringTableTools` |
| 머티리얼·머티리얼 인스턴스·텍스처 | `unreal-material` | `MaterialTools`, `MaterialInstanceTools`, `TextureTools` |
| 시퀀서·컨트롤리그·스켈레탈/스태틱 메시 | `unreal-animation` | `SequencerTools`, `ControlRigTools`, `SkeletalMeshTools` |
| 나이아가라 VFX | `unreal-niagara` | `NiagaraToolset_*` |
| 비헤이비어 트리·스테이트 트리·대화 | `unreal-ai` | `BehaviorTreeTools`, `StateTreeTools`, `ConversationTools` |
| 로그 확인, 콘솔 변수, PIE, 뷰포트, 에디터 UI 조작 | `unreal-editor-debug` | `EditorAppToolset`, `LogsToolset`, `SlateInspectorToolset` |
| 자동화 테스트 실행 | `unreal-test` | `AutomationTestToolset` |
| 프로젝트 설정·플러그인·게임 피처·피직스 에셋·PCG | `unreal-project-config` | `ConfigSettingsToolset`, `PluginToolset`, `GameFeaturesToolset` |
| **타임라인 커브 키 값, 커스텀 이벤트 리플리케이션 플래그** | `unreal-blueprint` | `BlueprintInternalsToolset` |

여러 툴을 한 흐름으로 묶어야 하면 `editor_toolset.toolsets.programmatic.ProgrammaticToolset`으로 배치 실행할 수 있다.

## 프로젝트 자체 툴셋 — `BlueprintInternalsToolset`

`Plugins/BlueprintInternalsToolset`은 이 프로젝트에서 만든 에디터 플러그인이다. 엔진 툴셋이 못 닿는 블루프린트 내부를 다룬다.

**왜 필요한가.** 아래 필드들은 엔진에서 맨 `UPROPERTY()`로 선언돼 있어 `ObjectTools.get_properties`로도, Python으로도 **보이지 않는다.** 값을 못 읽는 게 아니라 툴에 노출이 안 된 것이다.

- `UBlueprint::Timelines` → `FloatTracks`/`VectorTracks` → `CurveFloat` → `FloatCurve` (타임라인 커브 키)
- `K2Node_Event::FunctionFlags` (커스텀 이벤트의 Replicates·Reliable)

| 툴 | 용도 |
|---|---|
| `GetTimelines(Blueprint)` | 타임라인별 Length·LengthMode·AutoPlay/Loop/Replicated + 모든 트랙(Float·Vector·LinearColor·Event)의 키 (Time, Value, InterpMode) |
| `SetTimelineKeyValue(Blueprint, TimelineName, TrackName, Component, KeyIndex, NewValue)` | 키 값 하나 수정. `Component`는 float이면 `""`, vector면 `"X"/"Y"/"Z"`, 컬러면 `"R"/"G"/"B"/"A"` |
| `GetCustomEventReplication(Node)` | 커스텀 이벤트의 리플리케이션 모드·Reliable 조회 |
| `SetCustomEventReplication(Node, Replication, bReliable)` | 설정. `Replication`은 `NotReplicated`/`Multicast`/`RunOnServer`/`RunOnOwningClient` |

**타임라인 커브 값이 궁금하면 스크린샷으로 눈금을 읽지 말고 `GetTimelines`를 쓴다.** 외부 커브 애셋을 쓰는 트랙은 `SetTimelineKeyValue`가 일부러 거부한다 — 그 애셋을 쓰는 다른 곳까지 바뀌기 때문이다.

쓰기 툴은 블루프린트를 structurally modified로 표시만 한다. **`compile_blueprint` → `save_assets`는 직접 해야 한다.**
