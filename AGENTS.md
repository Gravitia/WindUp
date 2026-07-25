# Codex Project Notes

## 질문에 대한 답변 규칙
- 블루프린트, `.uasset`, `.umap` 파일에 대한 read/write 요청이 들어오는 경우, 파일을 직접 열지 말고 **반드시 Unreal MCP를 우선 사용하는 것을 확인한다.** 바이너리이므로 직접 편집하면 에셋이 깨진다.
- Unreal MCP는 언리얼 에디터가 실행 중일 때만 응답한다. 응답이 없으면 우회하지 말고 에디터 실행을 요청한다.

## Project
- Unreal Engine 5.8 프로젝트: `ChronoSpace.uproject`
- 워크스페이스 루트: 이 저장소의 루트 디렉터리
- 게임 모듈: `Source/ChronoSpace` (Runtime, 단일 모듈)
- 모듈 의존성: `Engine`, `GameplayAbilities`, `UMG`, `AIModule`, `Slate`, `SlateCore`
- 데디케이티드 서버 기반 멀티플레이 구성

## Top-Level Structure
- `.claude/`: Claude Code 설정. `rules/`(항상 적용되는 규칙), `skills/`(작업별 MCP 툴셋 가이드).
- `.codex/`: Codex 로컬 설정.
- `.github/`: 현재 비어 있음.
- `.mcp.json`: MCP 서버 등록 (`unreal-mcp` → `http://127.0.0.1:8000/mcp`).
- `CLAUDE.md`: Claude Code가 자동 로드하는 프로젝트 지침. `.claude/rules/`를 import한다.
- `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`: 언리얼 빌드/에디터 생성물. 요청이 없으면 수동 편집하지 않는다. 단, 아래 "커밋되는 바이너리" 참고.
- `Build/`: 언리얼 빌드 리소스.
- `Config/`: `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`, `DefaultEditor.ini`, `DefaultEditorPerProjectUserSettings.ini`, `DefaultGameplayTags.ini`, `Layouts/`, `UserSettings.json`.
- `Content/`: 언리얼 에셋.
- `Docs/`: 설계/운영 문서.
- `Plugins/`: 프로젝트 플러그인.
- `Source/`: C++ 코드 및 타깃/빌드 파일.

## 커밋되는 바이너리
`.uproject` 더블클릭만으로 에디터가 실행되도록 **에디터 DLL 일부를 의도적으로 커밋**한다. `.gitignore`가 `Binaries/`를 막은 뒤 다음만 되살린다 (마지막 매치가 이기는 순서에 의존하므로 규칙 순서를 바꾸지 않는다).

- `Binaries/Win64/UnrealEditor-ChronoSpace.dll`
- `Binaries/Win64/UnrealEditor.modules`
- `Binaries/Win64/ChronoSpaceEditor.target`
- `Plugins/*/Binaries/Win64/UnrealEditor-*.dll`, `UnrealEditor.modules`
- `Plugins/*/Binaries/Win64/EOSSDK-Win64-Shipping.dll`

빌드 후 `git status`에 이 DLL들이 뜨는 것은 정상이며 커밋 대상이다.

## Source Layout (`Source/ChronoSpace`)
- `Actor`: 게임플레이 액터
- `ActorComponent`: 재사용 액터 컴포넌트
- `Animation`: 애니메이션 코드
- `Attribute`: GAS 어트리뷰트/스탯
- `BT`: 비헤이비어 트리 노드
- `Character`: 캐릭터 클래스
- `Common`: 공용 코드
- `DataAsset`: C++ 데이터 에셋 타입
- `Debug`: 디버깅 헬퍼
- `GA`: 게임플레이 어빌리티
- `Game`: 게임 모드/스테이트 등 코어
- `Interface`: 인터페이스
- `Physics`: 물리 관련
- `Player`: 플레이어 컨트롤러/스테이트/입력
- `Save`: 세이브 게임
- `Setting`: 사용자 설정 (`CSUserSettings`) — 플레이어가 바꾸는 값
- `Settings`: 개발자 설정 (`CSAudioRoutingSettings`, `CSStageDataSettings`) — 프로젝트 설정에 노출되는 값
- `Subsystem`: 언리얼 서브시스템
- `UI`: C++ UI 클래스

`Setting`과 `Settings`는 이름이 비슷하지만 역할이 다르다. 통합하지 말고 목적에 맞는 쪽에 넣는다.

빌드 설정은 `Source/ChronoSpace/ChronoSpace.Build.cs`, 타깃은 `Source/ChronoSpace.Target.cs`와 `Source/ChronoSpaceEditor.Target.cs`.

## Content Layout
프로젝트 에셋 (번호 접두 폴더):
- `01_Blueprint`: 블루프린트
- `02_Map`, `Maps`: 맵/레벨
- `03_Input`: 입력 에셋
- `04_DataAssets`, `20_Data`: 데이터 에셋
- `10_BehaviorTree`: AI 비헤이비어 트리
- `11_Camera`: 카메라
- `12_Render`: 렌더링
- `21_Animation`: 애니메이션 에셋
- `30_Mesh`, `31_Material`, `32_PhysicsMaterials`: 메시/머티리얼
- `34_Niagara`: Niagara VFX
- `35_Font`, `40_Audio`, `90_Movies`: 폰트/오디오/미디어
- `50_LevelInstance`: 레벨 인스턴스
- `60_Character`: 캐릭터 에셋
- `99_Asset`: 기타

서드파티/임포트 팩 (**원본 수정 금지, 복제해서 사용**):
- `05_ThirdPerson`, `07_LyraCharacter`, `EasyGameUI`, `DemonicUI`, `AdvancedMenu`, `USCS`, `Realistic_Starter_VFX_Pack_Vol2`, `Jet_engine_effects`

에디터 관리 데이터 (직접 편집 금지):
- `__ExternalActors__`, `__ExternalObjects__`: World Partition 관리. 레벨 저장 시 함께 변경되는 것이 정상이다.
- `Collections`, `Developers`, `Localization`

## Plugins
`.uproject`에서 활성화된 것:
- 엔진 플러그인: `GameplayAbilities`, `ModelingToolsEditorMode`(에디터 전용), `ActorPalette`, `BlockoutToolsPlugin`, `JsonBlueprintUtilities`, `BlueprintFileUtils`
- **MCP: `ModelContextProtocol`, `MCPClientToolset`, `AllToolsets`** — Unreal MCP 연결의 실체다. 비활성화하면 MCP가 끊긴다.
- 프로젝트 플러그인: `EOSIntegrationKit`(`Plugins/EOSIntegrationKit-Version5`), `AdvancedSessions`

비활성화된 것: `AdvancedSteamSessions`, `Fab`, `Bridge`, `MetaHumanSDK`

주의: `Plugins/unreal-mcp/`는 `.uplugin`도 `Source`도 없는 **빌드 잔여물이며 git에 추적되지 않는다.** 로드되지 않으므로 MCP 동작과 무관하다. MCP 관련 조사를 이 폴더에서 하지 않는다.

## Docs
- `Docs/DedicatedServer_Migration.md`, `Docs/DedicatedServer_Runbook.md`: 데디케이티드 서버 구성/운영
- `Docs/SplitScreen_Architecture_Review.md`: 스플릿스크린 구조

## Working Guidelines
- 코드나 에셋을 추가할 때 기존 폴더 소유권과 프로젝트 컨벤션을 따른다. 애매하면 비슷한 기존 파일 위치를 먼저 찾는다.
- 생성 디렉터리는 작업이 명시적으로 요구하지 않는 한 수동 편집하지 않는다.
- C++ 변경은 빌드 타깃이나 모듈 설정을 손댈 이유가 없는 한 `Source/ChronoSpace` 안에서 끝낸다.
- 프로젝트 설정, 입력, 게임플레이 태그 변경은 `Config/`를 먼저 확인한다.
- 게임플레이 코드는 싱글플레이를 가정하지 않는다. 서버/클라이언트 권한과 리플리케이션을 확인한다.
- 블루프린트 수정 후에는 컴파일과 저장까지 해야 반영된다.
