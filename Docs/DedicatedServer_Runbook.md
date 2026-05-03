# ChronoSpace — Dedicated Server 생성·테스트 Runbook

> 본 문서는 **순서대로 따라가며 각 단계마다 검증**하는 실행 가이드다.
> 아키텍처 배경과 결정 근거는 [DedicatedServer_Migration.md](DedicatedServer_Migration.md) 를 참고.
> 각 Phase 는 **이전 Phase 가 검증 통과한 상태에서만** 시작한다. 절대 여러 Phase 를 한꺼번에 적용하지 말 것.

---

## 진행 원칙

1. **하나씩, 검증 가능한 단위로**: 각 Phase 끝에 "다음으로 가도 되는 조건" 이 있다. 통과 못 하면 다음으로 안 간다.
2. **롤백 가능하게**: Phase 마다 git commit. 문제 나면 단일 Phase 만 `git revert`.
3. **로그가 진실**: UE 로그(`Saved/Logs/ChronoSpace.log`, 데디는 `ChronoSpaceServer.log`) 와 EOS 로그를 항상 같이 본다. 본 프로젝트는 [`LogOnline=VeryVerbose`](../Config/DefaultEngine.ini#L242) 가 켜져 있어 EIK 흐름이 다 찍힌다.
4. **테스트 환경부터 분리**: 첫 검증은 **PIE 데디 모드**, 그다음 **localhost 직결**, 그다음 **LAN**, 마지막 **공인 IP**. 단계 건너뛰지 않는다.

---

## Phase 0 — 사전 준비

### 0.1 환경 확인

```
✔ Unreal Engine 5.5 (Source 빌드 또는 Launcher 빌드 — 둘 다 가능)
✔ Visual Studio 2022 (16.x 이상) + "Game development with C++" 워크로드
✔ Windows SDK 10.0.20348.0 이상
✔ Git 정상 작동 ( c:/Git/WindUp 이 워킹 트리)
✔ ChronoSpace.sln 이 정상 빌드되는 상태
```

### 0.2 시작 전 백업

```bash
cd c:/Git/WindUp
git checkout -b feature/dedi-server   # 작업 브랜치 분리
git status                             # clean 인지 확인
```

> 본 작업은 모두 **`feature/dedi-server`** (또는 별도 브랜치) 위에서 진행. 현재 브랜치 `feature/dedi` 위에 그대로 쌓아도 무방.

### 0.3 검증 — Phase 0 통과 조건

- [ ] 솔루션이 Development Editor 컨피그로 빌드 통과
- [ ] 에디터 PIE 가 **Listen Server** 모드로 정상 동작 (현재 상태)
- [ ] 첫 커밋 이후 변경사항 없음 (`git status` clean)

다음으로 가도 되는 조건: **세 항목 모두 ✔**

---

## Phase 1 — Server Target 추가 (빌드만 통과시키기)

목표: **코드 동작은 건드리지 않고**, 데디 서버 빌드 자체만 가능하게 만든다.

### 1.1 `ChronoSpaceServer.Target.cs` 신규 작성

위치: `c:/Git/WindUp/Source/ChronoSpaceServer.Target.cs`

```csharp
// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class ChronoSpaceServerTarget : TargetRules
{
    public ChronoSpaceServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion  = EngineIncludeOrderVersion.Latest;

        ExtraModuleNames.AddRange(new string[] { "ChronoSpace" });

        bUseLoggingInShipping = true;
    }
}
```

### 1.2 솔루션 재생성

`ChronoSpace.uproject` 우클릭 → **Generate Visual Studio project files**.

이후 `ChronoSpace.sln` 을 다시 열면 솔루션 컨피그에 다음이 추가되어야 한다:

```
Development Server | Win64
DebugGame Server   | Win64
Shipping Server    | Win64
```

### 1.3 첫 데디 서버 빌드

VS 상단에서 **Development Server | Win64** 선택 → `ChronoSpace` 프로젝트 빌드.

### 1.4 결과 확인

```
c:/Git/WindUp/Binaries/Win64/ChronoSpaceServer.exe        ← 생긴다
c:/Git/WindUp/Binaries/Win64/ChronoSpaceServer.target     ← 생긴다
c:/Git/WindUp/Binaries/Win64/ChronoSpaceServer.modules    ← 생긴다
```

### 1.5 자주 나오는 빌드 에러

| 에러 | 원인 | 처치 |
|------|------|------|
| `LNK2019 unresolved external symbol ... Slate ...` | Slate 코드가 서버 빌드에 컴파일됨 | 문제 없음. Phase 6 에서 `#if !UE_SERVER` 가드 추가 |
| `Cannot open include file 'Engine.h'` | Build.cs 의존 누락 | 본 프로젝트는 `Engine` 의존 있음. 솔루션 재생성 |
| `Plugin ... is not enabled for target ChronoSpaceServer` | 플러그인이 Server 타겟 미지원 | 해당 플러그인의 `.uplugin` 에 `Modules.Type=Server` 추가 또는 Server 타겟에서 disable |

> EIK 는 데디 서버를 공식 지원하므로 1.5 의 마지막 케이스가 발생하지 않아야 한다. 만약 발생하면 EIK 버전을 Marketplace 최신으로 갱신.

### 1.6 검증 — Phase 1 통과 조건

- [ ] `Binaries/Win64/ChronoSpaceServer.exe` 가 존재
- [ ] 빌드 출력에 에러 0개 (경고는 무시 가능)
- [ ] **이 시점에 코드 변경 0줄**. 오로지 새 `.Target.cs` 1개만 추가됨

```bash
git add Source/ChronoSpaceServer.Target.cs
git commit -m "Add dedicated server target"
```

---

## Phase 2 — 첫 데디 서버 부팅 (코드 수정 없이)

목표: **빌드된 데디 exe 가 부팅되어 첫 맵을 로드하는지** 확인. 클라 접속은 아직 안 함.

### 2.1 실행 명령

PowerShell 또는 cmd:

```bat
cd c:\Git\WindUp\Binaries\Win64
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777
```

옵션 의미:
- 첫 인자 `/Game/02_Map/L_StageSize` — 시작 맵. **`?listen` 절대 붙이지 않는다.**
- `-log` — 콘솔 로그 창을 띄움 (Development 빌드에서)
- `-port=7777` — 게임 UDP 포트. 방화벽이 막으면 첫 부팅에서 Windows 가 허용 다이얼로그를 띄움

### 2.2 정상 부팅 시 보이는 로그 (발췌)

```
LogInit: Build Configuration: Development
LogInit: Net Mode: DedicatedServer
LogWorld: Bringing World /Game/02_Map/L_StageSize.L_StageSize up for play
LogNet: NetworkProfiler: Initialized.
LogOnline: Verbose: OSS: Created online subsystem instance for: EIK
LogGameMode: GameMode initialized
LogNet: GameNetDriver IpNetDriver_X listening on port 7777
```

핵심 확인:
- `Net Mode: DedicatedServer` 가 찍히는가
- 맵이 로드되는가 (`Bringing World ... up for play`)
- NetDriver 가 7777 에 listening 되는가

### 2.3 자주 나오는 부팅 에러

| 증상 | 원인 | 처치 |
|------|------|------|
| `Failed to find object 'Class /Game/.../BP_CSGameMode'` | DefaultEngine.ini 의 GameMode 경로 (BP) 가 데디 빌드에 cook 안 됨 | Phase 5 에서 `GlobalDefaultServerGameMode` 설정. 또는 **C++ GameMode** 를 직접 쓰도록 임시 변경 |
| `Couldn't spawn player: Failed to spawn player controller` | GameMode 의 PlayerControllerClass nullptr | C++ 상위 GameMode 가 `APlayerController` 디폴트라 일반적으론 OK |
| `LogNet: Error: NetDriver creation failed` | EIK NetDriver 가 데디 환경에서 EOS 로그인 전 listen 시도 | 일단 무시. Phase 11 에서 데디 인증 후 재검증 |
| 콘솔 창이 그냥 닫힘 | `-log` 누락 또는 PAK 미존재 (Cooked 빌드만 PAK 필요) | Development 빌드는 PAK 불필요. 명령어 다시 확인 |
| 맵 로드 무한 대기 | 맵 경로 오타 | `/Game/02_Map/L_StageSize` 정확한지 |

### 2.4 데디 서버 종료

```
콘솔 창에서 Ctrl+C
또는 작업 관리자에서 ChronoSpaceServer.exe 종료
```

### 2.5 검증 — Phase 2 통과 조건

- [ ] `Net Mode: DedicatedServer` 로그 확인
- [ ] 첫 맵 정상 로드
- [ ] NetDriver 가 7777 포트에 listening
- [ ] 크래시 없이 30초 이상 idle 유지

> **이 시점에서 클라 접속을 시도하지 마라.** Phase 3 부터 클라이언트 접속을 검증한다.

```bash
git commit -am "Phase 2: dedicated server boots into first map (no code changes)"
```

---

## Phase 3 — PIE Dedicated 모드 검증

목표: 에디터 PIE 의 데디 서버 모드로 **클라 2개 + 데디 1개** 가 동시에 뜨는지 확인.

### 3.1 PIE 설정

에디터에서 **Edit → Editor Preferences → Level Editor → Play → Multiplayer Options**:

```
Play Net Mode             : Play As Client
Number of Players         : 2
Run Dedicated Server      : ✔ (체크)
Use Single Process        : ✔ (단일 프로세스로 동작 — 디버깅 쉬움)
Editor Multiplayer Mode   : Play In New Process (선택사항)
```

### 3.2 PIE 시작

레벨 에디터에서 시작 맵을 `L_StageSize` 로 두고 Play 버튼.

윈도우 3개가 뜬다:
- 데디 서버 윈도우 (콘솔만, 그래픽 없음)
- 클라이언트 1
- 클라이언트 2

### 3.3 이 단계에서 확인할 것

- 두 클라가 **같은 서버에 연결되는가** (둘 다 다른 캐릭터 보임)
- 서버 콘솔에 `PostLogin: Player1`, `PostLogin: Player2` 가 찍히는가
- Replicated Movement 정상 (한쪽이 움직이면 다른쪽 클라에서도 보임)

### 3.4 이 단계에서 깨질 수 있는 것 (정상)

다음 증상은 **현재 코드의 ListenServer 가정** 때문이고 Phase 5~7 에서 해결한다. 지금은 **확인만** 한다:

| 증상 | 원인 | 해결 Phase |
|------|------|------------|
| 클라 화면이 검은 채로 멈춤 | `?listen` 가 없으면 ServerTravel 가 다른 모드로 잡힘 | Phase 5 |
| GameMode SplitScreen 셋업 시도 후 에러 로그 | `bAutoEnableSplitScreen=true` + 데디 환경 | Phase 7 |
| `GEngine->GameViewport` nullptr 경고 | 데디에서 GameViewport 없음 | Phase 7 |
| `IsLocalController()` 분기 진입 안 함 | 데디는 LocalController 없음 | Phase 6 |

### 3.5 검증 — Phase 3 통과 조건

- [ ] PIE 가 데디 모드로 시작됨
- [ ] 데디 콘솔 로그에 `Net Mode: DedicatedServer` 찍힘
- [ ] 클라 2개가 같은 서버에 연결 (PlayerController 가 두 개 생성됨)

> 이 시점에서 게임플레이가 100% 정상일 필요 없다. **연결 자체** 만 되면 통과.

```bash
git commit --allow-empty -m "Phase 3: PIE dedicated mode verified"
```

---

## Phase 4 — `?listen` 토큰 제거 (3개 파일)

목표: 코드 안의 `?listen` 옵션을 모두 제거. 이게 남아 있으면 데디 서버가 ServerTravel 후 ListenServer 모드로 강제 전환되어 NetDriver 가 잘못 잡힌다.

### 4.1 변경 파일 1: `CSEIKSubsystem.cpp`

[Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp:164](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L164)

**Before:**
```cpp
if (World && World->GetAuthGameMode())
{
    UE_LOG(LogCS, Log, TEXT("ServerTravel - L_StageSize"));
    World->ServerTravel("/Game/02_Map/L_StageSize?listen");
}
```

**After:**
```cpp
if (World && World->GetAuthGameMode())
{
    UE_LOG(LogCS, Log, TEXT("ServerTravel - L_StageSize"));
    // 데디는 ?listen 옵션 금지. 서버는 이미 서버.
    World->ServerTravel(TEXT("/Game/02_Map/L_StageSize"));
}
```

### 4.2 변경 파일 2: `CSLabyrinthKeyAltar.cpp`

[Source/ChronoSpace/Actor/CSLabyrinthKeyAltar.cpp:116](../Source/ChronoSpace/Actor/CSLabyrinthKeyAltar.cpp#L116)

**Before:**
```cpp
void ACSLabyrinthKeyAltar::ChangeLevel()
{
    if ( HasAuthority() && GetWorld() )
    {
        GetWorld()->ServerTravel(TEXT("BaseMap?listen"));
    }
}
```

**After:**
```cpp
void ACSLabyrinthKeyAltar::ChangeLevel()
{
    if ( HasAuthority() && GetWorld() )
    {
        GetWorld()->ServerTravel(TEXT("BaseMap"));
    }
}
```

### 4.3 변경 파일 3: `SCSServerTravelWidget.cpp`

이건 **에디터/디버그 전용 위젯**이므로 데디 서버에서는 의미가 없다. 두 가지 처리 옵션 중 하나 선택:

**옵션 A — `?listen` 만 제거 (위젯 그대로 사용):**

[Source/ChronoSpace/UI/SCSServerTravelWidget.cpp:89](../Source/ChronoSpace/UI/SCSServerTravelWidget.cpp#L89)

```cpp
// Before
const FString Command = FString::Printf(TEXT("%s?listen"), *StageName);

// After
const FString Command = StageName;  // 데디 서버에서 호출 시 그대로 ServerTravel
```

**옵션 B — 위젯 자체를 에디터/디버그에서만 컴파일** (권장, 더 깔끔):

[Source/ChronoSpace/UI/SCSServerTravelWidget.h](../Source/ChronoSpace/UI/SCSServerTravelWidget.h) 와 `.cpp` 양쪽을 가드:

```cpp
// SCSServerTravelWidget.h 상단
#pragma once

#if !UE_SERVER  // 데디 빌드에서는 이 클래스 제외
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
// ... 기존 내용 ...
#endif // !UE_SERVER
```

`.cpp` 도 동일하게 전체를 `#if !UE_SERVER ... #endif` 로 감싼다.

본 가이드는 **옵션 A** 로 간단히 진행한다. (위젯이 어차피 디버그용이라 데디에서 호출될 일이 없으면 옵션 B 가 안전)

### 4.4 검증 — Phase 4 통과 조건

```bash
# 데디 서버 빌드 다시
# Visual Studio: Development Server | Win64 → ChronoSpace 빌드
```

- [ ] 데디 서버 빌드 통과
- [ ] Phase 2 와 동일한 방식으로 데디 부팅 → 정상 로그
- [ ] grep 으로 `?listen` 검색 시 코드 안에 0개 (Plugins 폴더 제외)

```bash
git commit -am "Phase 4: remove ?listen tokens from ServerTravel calls"
```

---

## Phase 5 — `.ini` GameMode 매핑 + Travel 보정

목표: 데디 서버가 부팅 시 어떤 GameMode 로 시작할지 명시.

### 5.1 `DefaultEngine.ini` 수정

[Config/DefaultEngine.ini:11-13](../Config/DefaultEngine.ini#L11-L13)

**Before:**
```ini
GlobalDefaultGameMode=/Game/01_Blueprint/Game/BP_CSGameMode.BP_CSGameMode_C
GlobalDefaultServerGameMode=None
GameInstanceClass=/Game/01_Blueprint/Game/BP_CSGameInstance.BP_CSGameInstance_C
```

**After:**
```ini
GlobalDefaultGameMode=/Game/01_Blueprint/Game/BP_CSGameMode.BP_CSGameMode_C
GlobalDefaultServerGameMode=/Game/01_Blueprint/Game/BP_CSGameMode.BP_CSGameMode_C
GameInstanceClass=/Game/01_Blueprint/Game/BP_CSGameInstance.BP_CSGameInstance_C
```

> `None` 으로 두면 데디는 `AGameModeBase` 디폴트로 부팅돼서 본 프로젝트의 PostLogin/Pawn 셋업이 동작 안 한다.

### 5.2 BP GameMode 가 데디 빌드에 cook 되는지 확인

`Project Settings → Packaging → Asset Manager`:
- "Cook everything in the project content directory" 가 체크되어 있으면 OK
- "List of maps & directories to cook" 에 `/Game/01_Blueprint/Game` 포함 확인

> Phase 1 직후 Phase 2 부팅에서 `Failed to find object 'Class /Game/.../BP_CSGameMode'` 가 떴다면 이 항목 때문이다.

### 5.3 검증 — Phase 5 통과 조건

데디 서버 부팅 후 로그:
```
LogGameMode: Initializing game GameMode = /Game/01_Blueprint/Game/BP_CSGameMode.BP_CSGameMode_C
```

- [ ] 위 로그가 찍힘 (None 이 아닌 BP_CSGameMode 로 부팅)
- [ ] `Failed to find object 'Class ...'` 에러 없음

```bash
git commit -am "Phase 5: set GlobalDefaultServerGameMode for dedicated boot"
```

---

## Phase 6 — `NM_ListenServer` 분기 정리

목표: 코드의 ListenServer 가정을 데디 친화적으로 변경. 의미적으로 **"서버 + 로컬 플레이어 동시 존재"** 분기를 명시적으로 처리.

### 6.1 변경 파일: `CSGameMode.cpp` 의 SplitScreen 가드

[Source/ChronoSpace/Game/CSGameMode.cpp:578-587](../Source/ChronoSpace/Game/CSGameMode.cpp#L578-L587)

**Before:**
```cpp
void ACSGameMode::TrySplitScreenSetup()
{
    if (!bAutoEnableSplitScreen) return;
    if (GetWorld()->GetNetMode() != NM_ListenServer) return;
    if (ConnectedPlayers.Num() != 2) return;
    if (DummyPlayerController) return; // 이미 설정됨

    UE_LOG(LogCS, Log, TEXT("TrySplitScreenSetup: Starting split screen setup..."));
    SetupOnlineSplitScreen();
}
```

**After:**
```cpp
void ACSGameMode::TrySplitScreenSetup()
{
    if (!bAutoEnableSplitScreen) return;

    // 데디 서버는 로컬 플레이어가 없으므로 호스트 측 SplitScreen 자체가 무의미
    if (GetNetMode() == NM_DedicatedServer)
    {
        UE_LOG(LogCS, Log, TEXT("TrySplitScreenSetup: skipped (dedicated server)"));
        return;
    }

    // ListenServer 시절의 동작은 그대로 유지 (호스트 + 1명 클라가 화면 분할)
    if (GetNetMode() != NM_ListenServer) return;
    if (ConnectedPlayers.Num() != 2) return;
    if (DummyPlayerController) return;

    UE_LOG(LogCS, Log, TEXT("TrySplitScreenSetup: Starting split screen setup..."));
    SetupOnlineSplitScreen();
}
```

> 의미: **"데디면 일찍 빠진다 → ListenServer 면 기존 흐름 유지 → Standalone/Client 는 통과 안 함"**.

### 6.2 변경 파일: `CSGameMode::BeginPlay`

[Source/ChronoSpace/Game/CSGameMode.cpp:34-47](../Source/ChronoSpace/Game/CSGameMode.cpp#L34-L47)

**Before:**
```cpp
void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (bAutoEnableSplitScreen)
    {
        UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>();

        if ( CSSplitSubsystem )
        {
            CSSplitSubsystem->EnableSplitScreen();
        }
    }
}
```

**After:**
```cpp
void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    // 데디 서버에는 GameViewport 자체가 없어서 SplitScreenSubsystem 호출이 무의미
    if (GetNetMode() == NM_DedicatedServer) return;

    if (bAutoEnableSplitScreen)
    {
        if (UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>())
        {
            CSSplitSubsystem->EnableSplitScreen();
        }
    }
}
```

### 6.3 검증 — Phase 6 통과 조건

데디 서버 부팅 시 로그:
```
LogCS: Log: TrySplitScreenSetup: skipped (dedicated server)
```

- [ ] PIE 데디 모드에서 위 로그 확인
- [ ] **`GEngine->GameViewport` nullptr** 관련 경고 사라짐
- [ ] PIE 클라 2개가 여전히 정상 접속 (게임플레이는 아직 깨질 수 있음 — Phase 7 에서 해결)

```bash
git commit -am "Phase 6: skip listen-server-only paths on dedicated"
```

---

## Phase 7 — PlayerController 의 Client SplitScreen 가드 강화

목표: PlayerController 안의 client 전용 split-screen 로직이 **데디 서버 프로세스 안에서 실행되지 않도록** 명시적 가드. (현재도 `IsLocalController()` 체크는 있으나 `NM_Client` 체크는 명시적으로 박혀 있어 개선)

### 7.1 변경 파일: `CSPlayerController.cpp` BeginPlay

[Source/ChronoSpace/Player/CSPlayerController.cpp:30-82](../Source/ChronoSpace/Player/CSPlayerController.cpp#L30-L82)

현재 코드는 `if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())` 로 잘 가드되어 있어 **데디 환경에서 안전**하다.

다만 명시성 차원에서 **추가 보호**:

**Before (BeginPlay 본문):**
```cpp
if (!bIsDummyController)
{
    UE_LOG(LogCS, Log, TEXT("PlayerController BeginPlay - IsLocalController: %s"),
        IsLocalController() ? TEXT("true") : TEXT("false"));

    // 클라이언트에서 로컬 컨트롤러인 경우 스플릿 스크린 설정
    if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
    {
        // ...
    }
}
```

**After:**
```cpp
// 데디 서버 프로세스에서는 어떤 PlayerController BeginPlay 도 클라 UI 흐름을 타지 않도록 명시
if (GetNetMode() == NM_DedicatedServer)
{
    return;
}

if (!bIsDummyController)
{
    UE_LOG(LogCS, Log, TEXT("PlayerController BeginPlay - IsLocalController: %s"),
        IsLocalController() ? TEXT("true") : TEXT("false"));

    if (GetWorld()->GetNetMode() == NM_Client && IsLocalController())
    {
        // ... 기존 로직 유지
    }
}
```

> **주의:** `Super::BeginPlay()` 호출은 위 early return 보다 위에 있어야 한다. 현재 코드 32라인이 그렇다 — OK.

### 7.2 검증 — Phase 7 통과 조건

PIE 데디 모드에서:
- [ ] 클라 2개의 캐릭터가 정상 스폰
- [ ] 클라가 서로 보임 (replication 정상)
- [ ] `EnableSplitScreen` 호출이 클라 측에서만 발생 (데디 측 로그에는 없음)
- [ ] UI 위젯이 클라 측에서 정상 생성

```bash
git commit -am "Phase 7: explicit dedicated-server guard in PlayerController BeginPlay"
```

---

## Phase 8 — localhost 직결 멀티 검증 (PIE 외부)

목표: **에디터가 아닌 실제 빌드된 exe** 로 데디 서버를 띄우고, 클라 2개가 직결 IP 로 접속.

### 8.1 클라이언트 빌드

VS 컨피그 **Development Editor** 가 아니라 **Development | Win64** 의 `ChronoSpace` 프로젝트를 빌드.

산출물: `Binaries/Win64/ChronoSpace.exe`

### 8.2 데디 서버 띄우기

터미널 1:
```bat
cd c:\Git\WindUp\Binaries\Win64
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777
```

### 8.3 클라이언트 1 띄우기

터미널 2:
```bat
cd c:\Git\WindUp\Binaries\Win64
ChronoSpace.exe 127.0.0.1:7777 -log -WINDOWED -ResX=1280 -ResY=720
```

### 8.4 클라이언트 2 띄우기

터미널 3:
```bat
cd c:\Git\WindUp\Binaries\Win64
ChronoSpace.exe 127.0.0.1:7777 -log -WINDOWED -ResX=1280 -ResY=720
```

### 8.5 정상 시 보이는 흐름

데디 서버 콘솔:
```
LogNet: Join request: ?Name=Player1
LogGameMode: PostLogin: Player1
LogCS: Player logged in: PlayerController_0
...
LogNet: Join request: ?Name=Player2
LogGameMode: PostLogin: Player2
LogCS: Player logged in: PlayerController_1
```

### 8.6 자주 나오는 실패

| 증상 | 원인 | 처치 |
|------|------|------|
| 클라가 `Connection timeout` | 방화벽이 7777 차단 | Windows 방화벽에 ChronoSpaceServer.exe 허용 |
| 클라 화면 검정 | GameMode 의 PlayerControllerClass 설정 누락 | Phase 5 의 BP_CSGameMode 가 PlayerControllerClass = ACSPlayerController 확인 |
| 클라 접속 즉시 끊김 | EIK NetDriver 가 인증 안 된 상태로 listen | Phase 11 까지 미완성. 현재는 Fallback `IpNetDriver` 가 동작해야 함 |
| `LogNet: Connect to remote ... is using protocol version mismatch` | 클라/서버 빌드 버전 불일치 | 둘 다 동일 빌드로 다시 |

### 8.7 EIK NetDriver vs IpNetDriver 분기 임시 처리

본 프로젝트는 `GameNetDriver=NetDriverEIK` 인데, **데디 서버는 EOS 인증 전이라 EIK NetDriver 가 부팅 못 할 수 있다.** 임시로 IpNetDriver 사용하려면 데디 부팅 시:

```bat
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777 ^
  -NetDriverOverrides="OnlineSubsystemUtils.IpNetDriver"
```

또는 `Config/DefaultEngine.ini` 의 NetDriverDefinitions 의 Fallback 우선순위를 활용하기 위해 EIK 를 일시적으로 disable. **Phase 11 에서 데디 인증을 추가하면 다시 EIK NetDriver 로 돌릴 수 있다.**

### 8.8 검증 — Phase 8 통과 조건

- [ ] 데디 서버 + 클라 2개가 별도 프로세스로 동시 동작
- [ ] 클라 2명이 서로 캐릭터를 볼 수 있음
- [ ] 입력/움직임이 정상 replicate
- [ ] 데디 서버 30분 이상 안정적으로 유지 (메모리 누수 검증)

```bash
git commit --allow-empty -m "Phase 8: localhost direct-IP multiplayer verified"
```

---

## Phase 9 — EIK Session 분리 (클라 / 서버)

목표: 지금까지는 **직결 IP** 로 접속했는데, 이제 EIK 세션을 통해 매칭하도록 한다. 데디 측은 세션을 만들고, 클라 측은 검색해서 join.

### 9.1 `CSEIKSubsystem` 의 책임 분리

현재 [`UCSEIKSubsystem`](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.h) 은 클라/서버 양쪽 흐름이 섞여 있다. 두 가지 접근:

- **A. 단일 클래스에 분기 추가** (작은 변경)
- **B. `UCSEIKClientSubsystem` / `UCSEIKServerSubsystem` 두 클래스로 분리** (깔끔하지만 변경 큼)

본 가이드는 **A** 채택.

### 9.2 헤더에 데디 메서드 추가

[Source/ChronoSpace/Subsystem/CSEIKSubsystem.h](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.h) 의 `// Create Session` 섹션:

**추가:**
```cpp
public:
    // 데디 서버 부팅 시 호출 — bIsDedicated=true 세션 생성
    UFUNCTION()
    void CreateDedicatedSession();

    // 데디 서버 종료 시 호출 — 세션 정리
    UFUNCTION()
    void DestroyDedicatedSession();
```

### 9.3 cpp 에 구현 추가

[Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp) 끝에:

```cpp
void UCSEIKSubsystem::CreateDedicatedSession()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem) { UE_LOG(LogCS, Error, TEXT("No EIK Subsystem")); return; }

    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    if (!Session.IsValid()) { UE_LOG(LogCS, Error, TEXT("Invalid IOnlineSessionPtr")); return; }

    OnCreateSessionCompleteDelegate = FOnCreateSessionCompleteDelegate::CreateUObject(
        this, &UCSEIKSubsystem::OnCreateSessionComplete);
    OnCreateSessionCompleteDelegateHandle =
        Session->AddOnCreateSessionCompleteDelegate_Handle(OnCreateSessionCompleteDelegate);

    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch           = false;
    Settings.NumPublicConnections  = 8;
    Settings.bShouldAdvertise      = true;
    Settings.bUsesPresence         = false;        // 데디는 presence 없음
    Settings.bAllowJoinInProgress  = true;
    Settings.bIsDedicated          = true;         // <-- 핵심
    Settings.bAllowInvites         = false;
    Settings.bUseLobbiesIfAvailable = false;       // 데디는 일반 세션
    Settings.Set(FName("MAPNAME"), FString("L_StageSize"),
                 EOnlineDataAdvertisementType::ViaOnlineService);

    Session->CreateSession(0, NAME_GameSession, Settings);
}

void UCSEIKSubsystem::DestroyDedicatedSession()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem) return;
    IOnlineSessionPtr Session = Subsystem->GetSessionInterface();
    if (!Session.IsValid()) return;

    Session->EndSession(NAME_GameSession);
    Session->DestroySession(NAME_GameSession);
}
```

> `OnCreateSessionComplete` 는 기존 함수를 그대로 재사용해도 되고, 데디 전용 콜백을 별도로 두어도 된다. 처음엔 재사용으로 간단히.

### 9.4 클라 측 `FindSessions` 의 검색 키 수정

[Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp:184-187](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L184-L187)

**Before:**
```cpp
SessionSearch = MakeShareable(new FOnlineSessionSearch);
SessionSearch->MaxSearchResults = 20;
SessionSearch->bIsLanQuery = false;
SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);
```

**After:**
```cpp
SessionSearch = MakeShareable(new FOnlineSessionSearch);
SessionSearch->MaxSearchResults = 20;
SessionSearch->bIsLanQuery = false;
// 데디 세션은 presence 없음 → DEDICATED_ONLY 로 검색
SessionSearch->QuerySettings.Set(SEARCH_DEDICATED_ONLY, true, EOnlineComparisonOp::Equals);
```

### 9.5 데디 부팅 시 자동 세션 생성

데디 서버 측에서 **부팅이 완료되고 첫 맵에 진입한 직후** `CreateDedicatedSession()` 을 호출해야 한다. 가장 자연스러운 위치는 `ACSGameMode::BeginPlay`:

[Source/ChronoSpace/Game/CSGameMode.cpp::BeginPlay](../Source/ChronoSpace/Game/CSGameMode.cpp#L34)

```cpp
void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (GetNetMode() == NM_DedicatedServer)
    {
        // 데디 서버 부팅 시 EIK 세션 생성
        if (auto* GI = GetGameInstance())
        {
            if (auto* EIK = GI->GetSubsystem<UCSEIKSubsystem>())
            {
                EIK->CreateDedicatedSession();
            }
        }
        return;  // 데디는 SplitScreen 셋업 안 함
    }

    if (bAutoEnableSplitScreen) { /* 기존 흐름 */ }
}
```

### 9.6 데디 종료 시 세션 정리

`ACSGameMode::EndPlay` 또는 별도 시그널 핸들러:

[Source/ChronoSpace/Game/CSGameMode.cpp::EndPlay](../Source/ChronoSpace/Game/CSGameMode.cpp#L511)

```cpp
void ACSGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (GetNetMode() == NM_DedicatedServer)
    {
        if (auto* GI = GetGameInstance())
        {
            if (auto* EIK = GI->GetSubsystem<UCSEIKSubsystem>())
            {
                EIK->DestroyDedicatedSession();
            }
        }
    }

    Super::EndPlay(EndPlayReason);

    if (GetWorld())
    {
        GetWorldTimerManager().ClearTimer(RotationSyncTimerHandle);
    }

    UE_LOG(LogCS, Log, TEXT("CSGameMode::EndPlay - cleared rotation sync timer"));
}
```

### 9.7 검증 — Phase 9 통과 조건

- [ ] 데디 서버 부팅 후 로그에 `Create Session Success: GameSession`
- [ ] EOS 개발자 포털에서 세션 표시 (또는 클라가 `FindSessions` 로 발견)
- [ ] 데디 종료 시 `EndSession`/`DestroySession` 콜백 정상

```bash
git commit -am "Phase 9: dedicated session create/destroy via EIK"
```

---

## Phase 10 — Dedicated Server 인증 추가

목표: 데디 서버 프로세스가 EOS 에 **`dedicated_server` credentials** 로 로그인. 클라 측 `device_id` 와 분리.

> EIK 의 `dedicated_server` 자격 증명 흐름은 EIK 버전마다 약간 다르다. 본 가이드는 EIK ≥ 3.0 기준.

### 10.1 헤더에 메서드 추가

```cpp
public:
    UFUNCTION()
    void LoginAsDedicatedServer();
```

### 10.2 cpp 구현

```cpp
void UCSEIKSubsystem::LoginAsDedicatedServer()
{
    IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(TEXT("EIK"));
    if (!Subsystem) return;

    IOnlineIdentityPtr Identity = Subsystem->GetIdentityInterface();
    if (!Identity.IsValid()) return;

    Identity->OnLoginCompleteDelegates->AddUObject(this, &UCSEIKSubsystem::OnLoginComplete);

    FOnlineAccountCredentials Creds;
    Creds.Type  = TEXT("dedicated_server");   // EIK 가 인식하는 데디 인증 타입
    Creds.Id    = TEXT("");
    Creds.Token = TEXT("");
    Identity->Login(0, Creds);
}
```

### 10.3 `Initialize()` 에서 분기

[Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp:13-19](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L13-L19) 의 현재 코드는 `return;` 으로 막혀 있다. 정상 동작하도록 복구하면서 분기:

**After:**
```cpp
void UCSEIKSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    if (IsRunningDedicatedServer())
    {
        LoginAsDedicatedServer();
    }
    else
    {
        // 클라 자동 로그인은 호출자가 결정 (현재처럼 명시 호출 유지)
        // LoginWithDeviceId();
    }
}
```

### 10.4 데디 부팅 → 로그인 → 세션 생성 → ServerTravel 순서 보장

순서가 중요하다:

```
1) 프로세스 시작
2) GameInstance Subsystem Initialize() → LoginAsDedicatedServer()
3) OnLoginComplete (성공 콜백) → CreateDedicatedSession()
4) OnCreateSessionComplete → StartSession (기존 흐름)
5) OnStartSessionComplete → ServerTravel("/Game/.../FirstMap")
```

`ACSGameMode::BeginPlay` 에서 직접 `CreateDedicatedSession` 호출했던 Phase 9 의 코드를 **`OnLoginComplete` 데디 분기 안으로 이동**:

```cpp
void UCSEIKSubsystem::OnLoginComplete(int32 LocalUserNum, bool bWasSuccessful, const FUniqueNetId& UserId, const FString& Error)
{
    if (!bWasSuccessful)
    {
        UE_LOG(LogCS, Error, TEXT("Login Failed: %s"), *Error);
        return;
    }

    UE_LOG(LogCS, Log, TEXT("Login Success: %s"), *UserId.ToString());

    if (IsRunningDedicatedServer())
    {
        CreateDedicatedSession();
    }
}
```

### 10.5 EIK Artifact 분리 (운영에서 필수, 개발 단계는 선택)

[Config/DefaultEngine.ini](../Config/DefaultEngine.ini) 의 `[/Script/EOSIntegrationKit.EIKSettings]` 섹션:

운영 직전이라면 클라/서버 ClientId·Secret 를 분리한 `+Artifacts=...` 항목을 추가하고:
```ini
DefaultArtifactName=ChronoSpaceClient
DedicatedServerArtifactName=ChronoSpaceServer
```

지금 단계에서는 동일 Artifact 사용해도 동작한다.

### 10.6 검증 — Phase 10 통과 조건

데디 서버 로그:
```
LogOnline: Verbose: OSS: Logging in user 0 with credentials type dedicated_server
LogOnline: Verbose: OSS: Login successful for user 0
LogCS: Login Success: ...
LogCS: Create Session Success: GameSession
LogCS: ServerTravel - L_StageSize  (있다면)
```

- [ ] 로그인 성공
- [ ] 세션 생성 성공
- [ ] **EIK NetDriver** 로 클라 접속 가능 (Phase 8.7 의 IpNetDriver override 제거 후 재테스트)

```bash
git commit -am "Phase 10: dedicated server EOS authentication"
```

---

## Phase 11 — 클라 매칭 흐름 검증 (Find/Join)

목표: 클라가 EIK 세션 검색 → join → ClientTravel 까지 자동 흐름.

### 11.1 클라 흐름 시나리오

기존 [`UCSEIKSubsystem::FindSessions`](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L168) 가 첫 결과로 자동 `JoinSession` 하는 구조. Phase 9 의 `SEARCH_DEDICATED_ONLY` 로 검색이 데디 세션만 잡히게 변경됐으니 그대로 동작해야 한다.

### 11.2 테스트 시퀀스

터미널 1: 데디 서버
```bat
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777
```

터미널 2: 클라 1 (로비 맵으로 시작)
```bat
ChronoSpace.exe /Game/02_Map/L_Title -log -WINDOWED
```

클라에서 (UI 또는 디버그 키로) `LoginWithDeviceId` → `FindSessions` 호출.

### 11.3 정상 시 클라 로그

```
LogCS: Find Session Success: <session_id>
LogCS: Join Session Success: GameSession
LogNet: ClientTravel: <ConnectInfo>
LogNet: Join request: ?Name=...
```

이후 데디 서버 측에 `PostLogin` 찍힘.

### 11.4 검증 — Phase 11 통과 조건

- [ ] 클라 1·2가 별도로 부팅해서 EIK 세션 검색만으로 데디에 접속
- [ ] 직결 IP 주소 입력 없이 멀티 가능
- [ ] 두 클라가 같은 매치에 들어옴

```bash
git commit -am "Phase 11: client matchmaking via EIK sessions verified"
```

---

## Phase 12 — LAN 테스트 (별도 PC)

목표: 같은 LAN 의 다른 PC 에서 클라 접속.

### 12.1 데디 서버 PC

```bat
:: 방화벽 인바운드 7777/UDP 허용
netsh advfirewall firewall add rule name="ChronoSpaceServer" dir=in action=allow protocol=UDP localport=7777
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777
```

서버 PC 의 LAN IP 확인: `ipconfig`, 예: `192.168.0.10`.

### 12.2 클라 PC

EIK 세션 매칭이면 IP 입력 불필요. 직결 테스트 시:
```bat
ChronoSpace.exe 192.168.0.10:7777 -log
```

### 12.3 검증 — Phase 12 통과 조건

- [ ] 다른 PC 의 클라가 EIK 매칭으로 접속 (서버는 EOS Relay 통과)
- [ ] 같은 LAN 직결 (`192.168.x.x:7777`) 도 정상
- [ ] 입력 지연 체감 없음 (LAN 핑 1~5ms)

```bash
git commit --allow-empty -m "Phase 12: LAN multi-PC test verified"
```

---

## Phase 13 — 공인 IP / 인터넷 테스트

목표: 외부 친구가 데디 서버에 접속.

### 13.1 옵션 A — EOS Relay 사용 (NAT 뒤 호스팅)

`bIsUsingP2PSockets=true` ([DefaultEngine.ini:191-192](../Config/DefaultEngine.ini#L191-L192)) 인 상태로 그대로 둔다. EOS 가 Relay 로 연결을 중계한다. 포트 포워딩 불필요.

장점: 설정 0
단점: Relay 트래픽이 EOS 데이터센터 경유 → 핑 +20~40ms

### 13.2 옵션 B — 공인 IP + 포트 포워딩 (낮은 지연)

라우터 설정:
- 외부 UDP 7777 → 내부 데디 PC 의 7777
- 외부 UDP 27015 (QueryPort) → 내부 27015

`DefaultEngine.ini` 변경:
```ini
[/Script/OnlineSubsystemEIK.NetDriverEIK]
bIsUsingP2PSockets=false
```

데디 서버 부팅:
```bat
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777 -QueryPort=27015
```

### 13.3 검증 — Phase 13 통과 조건

- [ ] 외부 PC(다른 ISP) 에서 EIK 매칭으로 데디 접속
- [ ] 핑이 합리적 (옵션 A: 50~100ms, 옵션 B: 30~50ms 한국 내)
- [ ] 30분 이상 안정 (Relay/직결 모두)

```bash
git commit -am "Phase 13: internet-facing dedicated server verified"
```

---

## Phase 14 — Split Screen 정책 결정 및 죽은 코드 정리

목표: 데디 모델이 안정 동작하는 상태에서 **ListenServer 시절의 죽은 코드** 를 처리.

### 14.1 결정

[DedicatedServer_Migration.md §6.2](DedicatedServer_Migration.md) 의 결정 A/B 중 하나 선택:

- **결정 A — Split Screen 폐기**: `bUseSplitscreen=false`, 관련 클래스 제거
- **결정 B — 클라 측 Split Screen 유지**: 현재 코드의 클라이언트 분기를 강화

본 Runbook 은 **결정 B** 가정.

### 14.2 데디에서 절대 실행되지 않는 GameMode 멤버 정리

[CSGameMode.cpp::CreateProxiesForPlayer](../Source/ChronoSpace/Game/CSGameMode.cpp#L556-L575) 의 `IsLocalController()` 분기는 데디에서 영원히 false. 코드 자체는 ListenServer 시 정상 동작이므로 **삭제하지 않고 유지**.

`CreateDummyLocalPlayer`/`AttachDummySpectatorToClient`/`SyncDummyRotationWithProxy`/`SetupOnlineSplitScreen` 도 마찬가지. Phase 6 의 `TrySplitScreenSetup` 가드가 막아주므로 데디에선 호출 안 됨.

### 14.3 운영 환경 로그 레벨 정리

[Config/DefaultEngine.ini:241-244](../Config/DefaultEngine.ini#L241-L244) 의 로그 레벨은 **개발 시점에는 유지**, **Shipping 빌드용 ini 분기 추가**:

`Config/DefaultEngine.ini` 옆에 `Config/Shipping/DefaultEngine.ini` 또는 ProjectSettings 의 Engine - Logging 에서:
```ini
[Core.Log]
LogOnline=Warning
LogEOSVoiceChat=Warning
LogTemp=Warning
```

> Verbose 로그는 데디 운영에서 디스크/CPU 를 잡아먹는다. 반드시 Shipping 에서는 Warning 이하.

### 14.4 SIGTERM 그레이스풀 셧다운

운영 환경에서 데디 인스턴스를 종료할 때 `EndSession` 이 호출되어야 EOS 에 좀비 세션이 남지 않는다. UE 의 `FCoreDelegates::OnExit` 또는 `UEngineSubsystem::Deinitialize` 시점에 `DestroyDedicatedSession` 보장.

이미 Phase 9 의 `EndPlay` 에서 처리했지만, **프로세스가 SIGTERM 으로 강제 종료되면 EndPlay 가 안 탈 수 있다**. 추가 보호:

```cpp
// CSEIKSubsystem::Initialize 끝에
FCoreDelegates::OnExit.AddUObject(this, &UCSEIKSubsystem::OnEngineExit);

// 새 멤버
void UCSEIKSubsystem::OnEngineExit()
{
    if (IsRunningDedicatedServer())
    {
        DestroyDedicatedSession();
    }
}
```

### 14.5 검증 — Phase 14 통과 조건

- [ ] 데디 서버 콘솔에서 Ctrl+C → `Destroy Session` 콜백 로그 확인
- [ ] EOS 콘솔/디버그 도구에서 좀비 세션 0개
- [ ] Shipping 빌드 시 LogOnline 이 Warning 이하

```bash
git commit -am "Phase 14: graceful shutdown + log levels for shipping"
```

---

## Phase 15 — 부하 / 안정성 테스트

목표: 실서비스 직전, 사용자 시나리오에서 데디가 무너지지 않는지 검증.

### 15.1 시나리오

1. **장시간 매치**: 8명 1시간 연속 → 메모리 누수 검증 (`stat memory`)
2. **연속 매치**: ServerTravel 5회 반복 → GC/리소스 누수 검증
3. **불안정 클라**: 한 명이 강제 종료 → `Logout` 처리 정상, 좀비 PC 없음
4. **다중 인스턴스**: 한 박스에 데디 4개 동시 → 박스 자원 측정

### 15.2 측정

콘솔 명령:
```
stat unit          → Server FPS / Game thread
stat net           → 네트워크 비용
stat memory        → RAM 점유
stat startfile     → 프로파일링 기록
```

### 15.3 검증 — Phase 15 통과 조건 (목표값)

- [ ] Server FPS 30 이상 유지 (60Hz 목표면 60 유지)
- [ ] Game thread time < 16ms (60Hz 목표) 또는 < 33ms (30Hz)
- [ ] 1시간 매치 후 RAM 증가량 < 50MB
- [ ] 클라 강제 종료 시 서버 콘솔에 정상 `Logout` 로그

---

## Phase 16 — 운영 자동화 (선택)

정식 출시 직전 단계. 본 Runbook 에서는 항목만 나열:

- [ ] systemd unit 으로 데디 인스턴스 자동 재시작
- [ ] Prometheus exporter 또는 자체 메트릭 엔드포인트
- [ ] Grafana 대시보드 (Server FPS, instance count, RAM, 매치 평균 수명)
- [ ] 크래시 덤프 자동 수집 (Sentry/CrashReportClient)
- [ ] 빌드 → 배포 CI (GitHub Actions / Jenkins → 호스팅 박스 SCP)
- [ ] 매치메이킹 라우팅 (리전 분리)

자세한 구성은 [DedicatedServer_Migration.md §9.11~§9.12](DedicatedServer_Migration.md) 참조.

---

## 부록 A — 단계별 git 브랜칭 전략

```
main                  ← 항상 안정. 데디 출시 전엔 ListenServer 모드만 안정 동작
└─ feature/dedi       ← 현재 브랜치
   └─ phase/01-target           # Phase 1 끝나면 머지
   └─ phase/02-first-boot
   └─ phase/04-listen-removal
   └─ phase/06-09-session
   └─ phase/10-auth
   └─ phase/14-cleanup
```

각 Phase 가 끝나면 **별도 PR** 로 리뷰 후 `feature/dedi` 에 머지. `feature/dedi` 자체가 안정되면 `main` 으로 머지.

---

## 부록 B — 빠른 참조 명령어

### 빌드

```bat
:: VS 에서 직접
"Development Server | Win64" → ChronoSpace 빌드
"Development | Win64"        → ChronoSpace 빌드

:: 또는 UAT
RunUAT BuildCookRun ^
  -project="C:\Git\WindUp\ChronoSpace.uproject" ^
  -platform=Win64 -serverconfig=Development ^
  -server -noclient -build -cook -stage -pak ^
  -archive -archivedirectory="C:\Git\WindUp\Build\Server"
```

### 실행

```bat
:: 데디
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777

:: 클라 (직결)
ChronoSpace.exe 127.0.0.1:7777 -log -WINDOWED -ResX=1280 -ResY=720

:: 클라 (매칭)
ChronoSpace.exe /Game/02_Map/L_Title -log -WINDOWED
```

### 로그 위치

```
Saved/Logs/ChronoSpaceServer.log     ← 데디 서버 로그
Saved/Logs/ChronoSpace.log           ← 클라 로그
```

### 자주 보는 콘솔 명령

```
stat unit
stat net
stat memory
stat fps
showdebug net
LogOnline Verbose
```

---

## 진행 체크리스트 요약

| Phase | 내용 | 통과 |
|-------|------|------|
| 0 | 사전 준비, 브랜치 분기 | ☐ |
| 1 | Server Target 추가, 빌드 통과 | ☐ |
| 2 | 데디 첫 부팅 (코드 수정 없이) | ☐ |
| 3 | PIE 데디 모드 검증 | ☐ |
| 4 | `?listen` 토큰 3곳 제거 | ☐ |
| 5 | `GlobalDefaultServerGameMode` 설정 | ☐ |
| 6 | `NM_ListenServer` → 데디 가드 추가 | ☐ |
| 7 | PlayerController 데디 가드 강화 | ☐ |
| 8 | localhost 직결 멀티 검증 | ☐ |
| 9 | EIK Session 분리 (클라/서버) | ☐ |
| 10 | Dedicated Server 인증 | ☐ |
| 11 | 클라 Find/Join 매칭 검증 | ☐ |
| 12 | LAN 다중 PC 테스트 | ☐ |
| 13 | 공인 IP / 인터넷 테스트 | ☐ |
| 14 | Split Screen 정책 + 운영 로그 정리 | ☐ |
| 15 | 부하 / 안정성 테스트 | ☐ |
| 16 | 운영 자동화 (선택) | ☐ |

각 Phase 가 끝날 때마다 **이 표의 체크박스를 채우면서** 진행. 막히면 그 Phase 의 "자주 나오는 실패" 표를 먼저 본다.
