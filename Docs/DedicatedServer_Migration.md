# ChronoSpace — Listen Server → Dedicated Server 전환 가이드

> 대상 엔진: **UE 5.5**
> 대상 OSS: **EIK (EOS Integration Kit)**
> 작성일: 2026-04-26
> 범위: 빌드 타겟, 코드, 설정(.ini), 세션/네트워크, 빌드/배포, 코드 수정 포인트까지의 **End-to-End** 전환 절차

---

## 0. 한눈에 보는 전환 결정 트리

| 항목 | Listen Server (현재) | Dedicated Server (목표) |
|------|----------------------|-------------------------|
| 호스트 | 플레이어 1명이 서버 + 클라 동시 수행 | 별도 프로세스 (헤드리스) |
| 빌드 타겟 | `ChronoSpace` (Game) 1개만 존재 | `ChronoSpace` + **`ChronoSpaceServer` 추가** |
| Travel URL | `/Game/.../Map?listen` | `/Game/.../Map` (옵션 없음) |
| `Settings.bIsDedicated` | `false` | `true` |
| 서버측 LocalPlayer | 존재함 (`IsLocalController()==true`) | **존재하지 않음** |
| 렌더/오디오/입력 | 모두 동작 | 모두 비활성, `nullptr` 가정 필요 |
| `NM_ListenServer` 체크 | 의미 있음 | **`NM_DedicatedServer`** 로 교체 |
| Split Screen | 서버 호스트가 두 명 분 처리 | 클라이언트 단일 윈도우만 의미 있음 |

핵심 원칙:

- **데디 서버에는 화면이 없다.** ViewportClient, GameViewport, LocalPlayer, GEngine->GameViewport 등은 전부 `nullptr` 또는 동작하지 않음을 가정해야 한다.
- **서버는 더 이상 게임 플레이어 1명이 아니다.** `IsLocalController()`, `GetLocalPlayer()`, `UGameplayStatics::GetPlayerController(World, 0)` 같은 호출이 서버에서는 항상 실패한다고 가정.
- **서버 권한 코드는 `HasAuthority()` 로**, 클라 단독 코드는 `IsNetMode(NM_Client)` 로 분기. `NM_ListenServer` 만 검사하던 로직은 **반드시 `IsServer()` 또는 `HasAuthority()` 기반**으로 교체.
- **EIK의 데디 서버 통신 모델은 P2P가 아니라 Listen 소켓 기반**이므로 `bIsUsingP2PSockets` 의 의미와 BeaconPort/포트 노출 정책을 다시 정의해야 한다.

---

## 1. 빌드 타겟 (Server Target) 추가

### 1.1 신규 파일 생성

`Source/ChronoSpaceServer.Target.cs` 를 새로 만든다.

```csharp
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

        // 서버 빌드는 클라 전용 모듈을 빼서 용량/의존성 축소
        bUseLoggingInShipping = true;     // 운영 로그 필요
        bUsesSlate            = false;    // SCSServerTravelWidget 처럼 Slate 의존이면 false 불가 → 1.3 참조
    }
}
```

> **주의:** 현재 코드에 `Source/ChronoSpace/UI/SCSServerTravelWidget.cpp` 등 **Slate 위젯**이 게임 모듈 안에 들어 있다. 데디 서버 빌드 시 이 코드들도 같이 컴파일된다. `WITH_SERVER_CODE` 와 `#if WITH_EDITOR`/`#if !UE_SERVER` 가드를 사용해 클라이언트 전용 코드 경로는 서버 빌드에서 빠지게 해야 한다. (5.3 참조)

### 1.2 `.uproject` 모듈 정의 점검

현재 `ChronoSpace.uproject` 의 `Modules` 배열은 `Type: Runtime` 단일 모듈이라 그대로 두면 된다. 다만 클라 전용 의존(예: 모델링 툴) 플러그인은 **이미 `TargetAllowList: ["Editor"]`** 로 잘 막혀 있으니 추가 작업 불필요.

EIK 플러그인은 데디 서버 타겟을 자체적으로 지원한다 (`DefaultEngine.ini` 에 `DedicatedServerArtifactName=ChronoSpace` 가 이미 있음).

### 1.3 모듈 `Build.cs` 의 의존 점검

`Source/ChronoSpace/ChronoSpace.Build.cs` 의 `PublicDependencyModuleNames` 에 `Slate`, `SlateCore`, `UMG`, `Niagara` 가 들어 있다. 이는 **데디 서버 빌드에서도 컴파일**되므로 다음 중 하나를 택한다:

1. **권장:** Slate/UMG/Niagara 를 사용하는 클래스 전체에 `#if !UE_SERVER` 가드를 두어 코드 경로를 비활성화. 모듈 의존은 그대로 유지. (가장 변경 폭이 작음)
2. UI 전용 클래스를 `ChronoSpaceUI` 같은 별도 모듈로 분리하고 서버 타겟에서 제외. (구조적으로 깔끔하지만 작업량 큼)

본 문서는 **(1) 가드 방식**을 기본으로 진행한다.

---

## 2. 빌드/실행 검증

### 2.1 데디 서버 빌드 명령

```bat
:: Engine 루트에서
RunUAT BuildCookRun ^
  -project="C:\Git\WindUp\ChronoSpace.uproject" ^
  -noP4 -platform=Win64 -clientconfig=Development -serverconfig=Development ^
  -server -serverplatform=Win64 -noclient ^
  -cook -allmaps -build -stage -pak -archive ^
  -archivedirectory="C:\Git\WindUp\Build\Server"
```

또는 Visual Studio 솔루션 `ChronoSpace.sln` 에서 **`ChronoSpaceServer`** Configuration 을 선택해 빌드.

빌드 산출물 경로(예시): `Binaries/Win64/ChronoSpaceServer.exe` (Development), `Binaries/Win64/ChronoSpaceServer-Win64-Shipping.exe` (Shipping).

### 2.2 데디 서버 실행

```bat
ChronoSpaceServer.exe /Game/02_Map/L_StageSize -log -port=7777 -QueryPort=27015 ^
  -ServerName="ChronoSpace_Dev_01" -SteamServerName=ChronoSpace
```

옵션 설명:

- 첫 인자 = **시작 맵 경로**. ListenServer의 `?listen` 같은 옵션 **금지**.
- `-log` : 콘솔 로그 윈도우 표시 (Development).
- `-port=7777` : 게임 포트(기본 7777).
- `-QueryPort=27015` : OSS 가 사용하는 쿼리/스팀 포트. EIK 도 동일 개념.
- 추가 옵션은 `?game=...?MaxPlayers=...` 형태로 첫 인자 뒤에 붙여 전달.

### 2.3 클라이언트로 접속

```bat
ChronoSpace.exe 127.0.0.1:7777 -log
```

또는 EIK 세션 매칭 흐름(아래 4장)으로 접속.

### 2.4 PIE 에서 데디 모드 시뮬레이션

에디터 → **Editor Preferences → Play → Multiplayer Options**

- `Net Mode` = **Play As Client**
- `Number of Players` = 2
- `Run Dedicated Server` = **체크**

이 상태에서 PIE 를 누르면 데디 서버 1 + 클라 2 가 실행되어 인게임 동작을 검증할 수 있다.

---

## 3. 코드 수정 포인트

> 아래 항목들은 **현재 리포지터리 코드에서 직접 발견된 문제**들이다.
> 라인 번호는 작성 시점 기준이므로 변경되었을 수 있다.

### 3.1 ServerTravel `?listen` 제거

데디 서버는 이미 서버이므로 `?listen` 옵션은 **틀린 신호**다. 제거하지 않으면 ListenServer 모드가 강제로 켜져 NetDriver 가 잘못 잡힌다.

| 파일 | 현재 코드 | 수정 |
|------|-----------|------|
| [Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp:164](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L164) | `World->ServerTravel("/Game/02_Map/L_StageSize?listen");` | `World->ServerTravel("/Game/02_Map/L_StageSize");` |
| [Source/ChronoSpace/Actor/CSLabyrinthKeyAltar.cpp:116](../Source/ChronoSpace/Actor/CSLabyrinthKeyAltar.cpp#L116) | `GetWorld()->ServerTravel(TEXT("BaseMap?listen"));` | `GetWorld()->ServerTravel(TEXT("BaseMap"));` |
| [Source/ChronoSpace/UI/SCSServerTravelWidget.cpp:89](../Source/ChronoSpace/UI/SCSServerTravelWidget.cpp#L89) | `FString::Printf(TEXT("%s?listen"), *StageName);` | 디버그 위젯은 데디 서버에는 의미 없음 → **편집기/디버그 클라 전용**으로 분리하거나 `#if WITH_EDITOR` 가드. ServerTravel 자체는 클라가 호출해서 안 됨. |

### 3.2 `NM_ListenServer` 분기 → 일반화

[Source/ChronoSpace/Game/CSGameMode.cpp:581](../Source/ChronoSpace/Game/CSGameMode.cpp#L581) 의 split-screen 셋업 가드:

```cpp
if (GetWorld()->GetNetMode() != NM_ListenServer) return;
```

이 분기는 **"서버에 로컬 플레이어가 있을 때만"** 의 의미였다. 데디 서버에서는 이 함수 자체를 **타지 말아야** 한다. 전체 `SetupOnlineSplitScreen`/`CreateDummyLocalPlayer`/`AttachDummySpectatorToClient` 흐름은 **클라이언트 쪽으로 이전**되어야 한다(자세한 내용 6장).

수정 방향:

```cpp
void ACSGameMode::TrySplitScreenSetup()
{
    if (!bAutoEnableSplitScreen) return;

    // 데디 서버에서는 더미 LocalPlayer 만들기 자체가 무의미
    if (GetNetMode() == NM_DedicatedServer) return;

    if (ConnectedPlayers.Num() != 2) return;
    if (DummyPlayerController) return;

    SetupOnlineSplitScreen();
}
```

`CSCheckPoint::DebugNetworkInfo()` 의 switch는 그대로 두면 된다(읽기 전용 분기).

### 3.3 GameMode `BeginPlay`/`PostLogin` 의 GameInstance Subsystem 호출

[Source/ChronoSpace/Game/CSGameMode.cpp:34-46](../Source/ChronoSpace/Game/CSGameMode.cpp#L34-L46) 의 `bAutoEnableSplitScreen` → `UCSSplitScreenSubsystem::EnableSplitScreen()` 흐름을 보자.

```cpp
UCSSplitScreenSubsystem* CSSplitSubsystem = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>();
if (CSSplitSubsystem)  CSSplitSubsystem->EnableSplitScreen();
```

`UCSSplitScreenSubsystem::EnableSplitScreen()` 내부([Source/ChronoSpace/Subsystem/CSSplitScreenSubsystem.cpp:31](../Source/ChronoSpace/Subsystem/CSSplitScreenSubsystem.cpp#L31)) 는

```cpp
GEngine->GameViewport->SetForceDisableSplitscreen(false);
GEngine->GameViewport->MaxSplitscreenPlayers = MaxSplitScreenPlayers;
```

를 직접 호출한다. **데디 서버는 `GEngine->GameViewport` 가 항상 `nullptr`** 이다. 현재 가드가 있으니 크래시는 안 나지만 **호출 자체가 무의미한 일을 시작하는 것** 이므로 GameMode 단에서 데디일 때 호출을 막는 게 맞다.

수정:

```cpp
void ACSGameMode::BeginPlay()
{
    Super::BeginPlay();

    if (GetNetMode() == NM_DedicatedServer) return;       // 추가

    if (bAutoEnableSplitScreen)
    {
        if (auto* SplitSub = GetGameInstance()->GetSubsystem<UCSSplitScreenSubsystem>())
            SplitSub->EnableSplitScreen();
    }
}
```

### 3.4 `CreateProxiesForPlayer` 의 ServerCamProxy

[Source/ChronoSpace/Game/CSGameMode.cpp:556-575](../Source/ChronoSpace/Game/CSGameMode.cpp#L556-L575):

```cpp
if (NewPlayer->IsLocalController() && !ServerCamProxy)
{
    // ...ServerCamProxy 생성 (ListenServer POV)
}
```

데디 서버에는 `IsLocalController()==true` 인 PC 가 없으므로 이 블록은 **결코 실행되지 않는다.** 코드는 그대로 두어도 되지만, **ServerCamProxy 가 nullptr 인 상태**가 데디의 정상 상태가 된다. 이를 사용하는 모든 호출처(`SyncDummyRotationWithProxy()` 등)는 이미 null 가드가 있는지 확인. 없으면 추가.

원격 클라 Proxy 생성 블록 (`!IsLocalController()` 분기) 은 데디에서도 **그대로 동작**한다(모든 PC 가 원격이므로). OK.

### 3.5 ConnectedPlayers 카운트 기반 SplitScreen 트리거

[Source/ChronoSpace/Game/CSGameMode.cpp:578-587](../Source/ChronoSpace/Game/CSGameMode.cpp#L578-L587):

```cpp
if (ConnectedPlayers.Num() != 2) return;
```

데디에서는 의미 없음. 3.2 의 `NM_DedicatedServer` 가드로 막힌다.

### 3.6 PlayerController의 클라이언트 Split Screen 로직

[Source/ChronoSpace/Player/CSPlayerController.h:131-186](../Source/ChronoSpace/Player/CSPlayerController.h#L131-L186) 의 `SetupClientSplitScreen`/`CreateClientDummyPawn`/`StartClientDummySync` 등은 **클라이언트에서 실행되는 코드** 다. 데디 서버 전환 후에도 그대로 살려야 하지만, **서버 권한이 없는 상태에서의 동작**을 분리해야 한다.

특히 다음 함수들은 **각각 명시적 RoleCheck** 를 추가:

```cpp
void ACSPlayerController::SetupClientSplitScreen()
{
    if (!IsLocalController()) return;       // 데디 서버에서는 절대 실행 X
    // ... 기존 로직
}
```

### 3.7 EIK Session 설정의 `bIsDedicated`

[Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp:108-117](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L108-L117):

```cpp
FOnlineSessionSettings Settings;
Settings.bIsLANMatch          = false;
Settings.NumPublicConnections = 2;
Settings.bShouldAdvertise     = true;
Settings.bUsesPresence        = true;
Settings.bAllowJoinInProgress = true;
Settings.bIsDedicated         = false;       // <— 변경 대상
```

**ListenServer 시절**에는 클라이언트가 직접 `CreateSession` 을 호출하고 자기 프로세스를 서버로 띄웠다. **데디 모델**에서는 두 가지 길이 있다:

- **A. 매치메이킹 / 공개 세션**: 클라가 세션을 만드는 게 아니라, **데디 서버 프로세스가 부팅될 때** 자기 자신을 세션으로 등록 (`bIsDedicated=true`). 클라는 `FindSessions` 후 `JoinSession` 만 한다.
- **B. 서버 리스트 모델**: 데디 서버가 자체적으로 세션을 만들고 advertise. 클라는 단순 IP 접속 또는 세션 검색.

본 가이드는 **A 방식**을 권장한다. `UCSEIKSubsystem` 을 분기하거나 **두 개의 별도 메서드** 로 분리:

```cpp
// 데디 서버 부팅 시 호출
void UCSEIKSubsystem::CreateDedicatedSession()
{
    FOnlineSessionSettings Settings;
    Settings.bIsLANMatch          = false;
    Settings.NumPublicConnections = 8;          // 데디는 더 큰 세션도 허용
    Settings.bShouldAdvertise     = true;
    Settings.bUsesPresence        = false;      // 데디는 presence 없음
    Settings.bAllowJoinInProgress = true;
    Settings.bIsDedicated         = true;       // <-- 핵심
    Settings.bAllowInvites        = false;
    Settings.bUseLobbiesIfAvailable = false;    // 데디는 일반 세션
    Settings.Set(FName("MAPNAME"), FString("L_StageSize"),
                 EOnlineDataAdvertisementType::ViaOnlineService);

    Session->CreateSession(0, NAME_GameSession, Settings);
}
```

**클라 측 `FindSessions`** 의 `QuerySettings.Set(SEARCH_PRESENCE, true, ...)` 도 바꿔야 한다(데디 세션에는 presence 없음):

```cpp
SessionSearch->QuerySettings.Set(SEARCH_DEDICATED_ONLY, true, EOnlineComparisonOp::Equals);
```

### 3.8 `OnStartSessionComplete` 의 ServerTravel

현재([Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp:157-165](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L157-L165)):

```cpp
if (World && World->GetAuthGameMode())
{
    World->ServerTravel("/Game/02_Map/L_StageSize?listen");
}
```

데디 서버는 `World->GetAuthGameMode()` 가 항상 유효하다. `?listen` 만 빼면 된다. **단, 이 메서드는 데디 서버 프로세스 안에서만 호출되어야 한다.** 클라가 호출하면 무시되지만 분기로 명확히:

```cpp
if (!IsRunningDedicatedServer()) return;  // global helper, UE 제공
World->ServerTravel("/Game/02_Map/L_StageSize");
```

### 3.9 `LoginWithDeviceId` — 데디 서버 인증

EIK 의 `LoginWithDeviceId` 는 **클라이언트용 익명 로그인**이다. 데디 서버는 별도의 **Server Auth** 가 필요하다. EOS Dev Auth Tool 을 통한 client_credentials 인증, 또는 EOS Dedicated Server credentials 를 사용:

```cpp
FOnlineAccountCredentials Creds;
Creds.Type  = TEXT("dedicated_server");      // EIK 가 인식하는 타입
Creds.Id    = TEXT("");
Creds.Token = TEXT("");
Identity->Login(0, Creds);
```

호출 분기는 `Initialize()` 에서:

```cpp
if (IsRunningDedicatedServer())
    LoginAsDedicatedServer();
else
    LoginWithDeviceId();   // 기존 흐름 유지
```

`Artifact` 도 다른 것을 쓰는 게 일반적이다 — `DefaultEngine.ini` 의 `DedicatedServerArtifactName` 키가 이미 그 용도. 운영 시점엔 클라/서버 ClientId·Secret 분리 권장.

---

## 4. 세션 / 매칭 흐름 재설계

### 4.1 시퀀스 비교

**Listen Server (현재)**

```
[Client A] → CreateSession(bIsDedicated=false) → ServerTravel("Map?listen") → 서버 + 본인 클라가 됨
[Client B] → FindSessions → JoinSession → ClientTravel(ConnectInfo)
```

**Dedicated Server (목표)**

```
[Server Process] 부팅 → LoginAsDedicatedServer → CreateSession(bIsDedicated=true) → ServerTravel("Map")
[Client A]      → FindSessions(SEARCH_DEDICATED_ONLY) → JoinSession → ClientTravel(ConnectInfo)
[Client B]      → 동일
```

### 4.2 세션 라이프사이클 정리

- **세션 생성 주체**: GameInstanceSubsystem 이 아닌 **데디 서버 프로세스의 GameMode 또는 `UEngineSubsystem`** 에서 부팅 시점에 한 번만 호출. 클라 코드와 명시적으로 분리할 것.
- **세션 종료**: `EndSession`/`DestroySession` 을 서버 셧다운 직전 호출. 안 하면 EOS 콘솔에 좀비 세션이 남음.
- **맵 변경 시**: `ServerTravel` 후 새 GameMode 에서 `UpdateSession`(가능하면) 또는 `DestroySession`+`CreateSession` 으로 갱신.

---

## 5. `.ini` 설정 변경

### 5.1 `Config/DefaultEngine.ini` — NetDriver

현재 ([Config/DefaultEngine.ini:97-102](../Config/DefaultEngine.ini#L97-L102)):

```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="OnlineSubsystemEIK.NetDriverEIK",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")
```

**그대로 유지 가능.** EIK NetDriver 는 데디 서버 모드에서도 동작한다. 단, `[/Script/OnlineSubsystemEIK.NetDriverEIK] bIsUsingP2PSockets=true` ([Config/DefaultEngine.ini:191-192](../Config/DefaultEngine.ini#L191-L192)) 는 운영 환경에 따라 결정:

- 데디 서버에 **공인 IP / 개방 포트** 가 있다 → `bIsUsingP2PSockets=false` 권장 (직접 UDP, 지연 적음).
- NAT 뒤에 둔다 → `true` 유지 (EOS Relay 사용).

### 5.2 `Config/DefaultGame.ini` — 서버 전용 GameMode 매핑

```ini
[/Script/Engine.GameMapsSettings]
GameDefaultMap=/Game/02_Map/L_Title.L_Title
ServerDefaultMap=/Game/02_Map/L_StageSize.L_StageSize
GlobalDefaultGameMode=/Game/01_Blueprint/Game/BP_CSGameMode.BP_CSGameMode_C
GlobalDefaultServerGameMode=/Game/01_Blueprint/Game/BP_CSGameMode.BP_CSGameMode_C
```

`DefaultEngine.ini` 의 `GlobalDefaultServerGameMode=None` ([Config/DefaultEngine.ini:12](../Config/DefaultEngine.ini#L12)) 도 같은 GameMode 로 채워야 데디가 부팅 시 올바른 GameMode 로 시작한다.

### 5.3 `Config/DefaultEngine.ini` — Lobby 분리 (선택)

데디 모델에서는 보통 "로비 맵 = 매치메이킹 UI 띄우는 클라 전용 맵" 이다. `ACSLobbyGameMode` 가 매치메이킹 페이즈에서만 쓰인다면, 이 GameMode 는 데디 서버 측에서는 **부팅하지 않는 것이 맞다.**

권장: 클라이언트는 `L_Lobby` (오프라인) → `FindSessions` → `JoinSession` → 데디 서버의 게임 맵으로 ClientTravel. 서버는 `L_StageSize` (또는 매치당 맵) 만 호스트.

### 5.4 EIK Artifact 분리 (운영 단계)

```ini
[/Script/EOSIntegrationKit.EIKSettings]
DefaultArtifactName=ChronoSpaceClient
DedicatedServerArtifactName=ChronoSpaceServer
+Artifacts=(ArtifactName="ChronoSpaceClient", ClientId=..., ClientSecret=..., ...)
+Artifacts=(ArtifactName="ChronoSpaceServer", ClientId=..., ClientSecret=..., ...)
```

현재는 동일 Artifact 를 쓴다. 보안상 운영에서는 분리하는 것이 표준.

---

## 6. Split Screen 시스템 — 가장 큰 변환 이슈

이 프로젝트의 `CSGameMode` / `CSPlayerController` / `CSSplitScreenSubsystem` / `CSCameraViewProxy` / `CSSpectatorPawn` 묶음은 **"ListenServer 호스트가 화면을 둘로 쪼개 두 명을 보여주는"** 구조에 강하게 종속되어 있다.

### 6.1 현재 동작 요약

- `ACSGameMode::SetupOnlineSplitScreen`: ListenServer 호스트에 더미 LocalPlayer 를 추가하고, **원격 클라**의 카메라를 모방하는 더미 SpectatorPawn 을 호스트 화면에 띄움.
- `ACSCameraViewProxy`: 클라의 카메라 정보를 서버에 보내는 Replicated Actor. 호스트는 이걸 읽어 더미 SpectatorPawn 에 적용.
- `ACSPlayerController::SetupClientSplitScreen`: 클라이언트 측에서도 동일하게, **원격 호스트의 캐릭터**를 모방하는 더미 SpectatorPawn 을 자기 화면에 띄움.

### 6.2 데디 전환의 의미

데디 서버에는 **로컬 플레이어가 0명**이다. 따라서 **"호스트 화면의 절반"** 이라는 개념이 사라진다. 두 가지 결정이 필요:

#### 결정 A: 진짜 멀티플레이로만 가고, Split Screen 폐기

가장 깔끔. `bAutoEnableSplitScreen=false` 를 기본으로 하고, `CSGameMode` 에서 split screen 관련 코드를 모두 데디 가드(`if (GetNetMode() == NM_DedicatedServer) return;`) 뒤에 두면 끝. 클라이언트들은 각자 자기 화면 하나만 본다.

#### 결정 B: "각 클라이언트 내부에서만" Split Screen 유지

각 클라이언트에서 `Player 0 = 본인 캐릭터`, `Player 1 = 다른 클라의 캐릭터를 영상화한 더미` 형태. 즉 **Split Screen 로직 전체를 PlayerController/Client 측으로 옮긴다.** 서버는 권한 처리만 하고, 클라가 둘로 쪼갠 화면을 자기 책임 하에 그린다.

이미 `ACSPlayerController::SetupClientSplitScreen` 같은 클라 측 흐름이 일부 구현되어 있으므로 **결정 B 가 점진적 마이그레이션에 더 적합**.

### 6.3 결정 B 채택 시 작업 목록

1. `ACSGameMode::CreateDummyLocalPlayer`, `AttachDummySpectatorToClient`, `SyncDummyRotationWithProxy`, `SetupOnlineSplitScreen` 호출을 **모두 NM_DedicatedServer 에서 스킵**.
2. `ACSGameMode::CreateProxiesForPlayer` 의 **`ServerCamProxy` 분기는 죽은 코드**가 됨 — 의도된 죽은 코드로 두거나 삭제.
3. `ACSPlayerController::SetupClientSplitScreen` 흐름을 클라 부팅 후 자동 실행되도록 강화. 현재 GameMode 가 트리거하는 부분이 있다면 이를 PlayerController **자체**가 결정하게 변경.
4. `ACSCameraViewProxy` 의 서버측 `ServerUpdateClientCamera`(클라 → 서버) 와 Replicated `RepCam`(서버 → 모든 클라) 흐름은 그대로 유효. 데디 모델에서도 각 클라가 자기 카메라를 서버에 보내고, 서버는 다른 클라에게 복제만 한다.

### 6.4 결정 A 채택 시 작업 목록

1. `Config/DefaultEngine.ini` 의 `bUseSplitscreen=False` 로 변경.
2. `CSGameMode` 의 SplitScreen 관련 멤버/함수 전부 제거 또는 stub.
3. `CSSplitScreenSubsystem`, `CSGameViewportClient::LayoutPlayers`, `CSCameraViewProxy`, `CSSpectatorPawn` 도 미사용 → 정리.

---

## 7. 죽은/문제될 코드 (체크리스트)

| 위치 | 증상 | 처치 |
|------|------|------|
| [CSEIKSubsystem.cpp:13-19](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L13-L19) | `Initialize()` 첫 줄 `return;` 으로 사실상 비활성 | 데디/클라 분기 후 정상 호출하도록 복구 |
| [CSGameMode.cpp:289-352](../Source/ChronoSpace/Game/CSGameMode.cpp#L289-L352) | `CreateDummyLocalPlayer` — 데디에서는 항상 실패 | 6장 결정에 따라 가드 또는 제거 |
| [CSGameMode.cpp:556-575](../Source/ChronoSpace/Game/CSGameMode.cpp#L556-L575) | `IsLocalController()` 분기 → 데디에서 영원히 false | 6장 결정에 따라 처리 |
| [CSPlayerController.h:130-186](../Source/ChronoSpace/Player/CSPlayerController.h#L130-L186) | Client SplitScreen 변수들 | 클라 전용으로 명시 |
| [SCSServerTravelWidget.cpp](../Source/ChronoSpace/UI/SCSServerTravelWidget.cpp) | 디버그 위젯이 `?listen` 으로 ServerTravel | `WITH_EDITOR` 가드 + `?listen` 제거 |

---

## 8. 빌드/배포 파이프라인

### 8.1 폴더 구조 (권장)

```
Build/
├── Server/                  ← UAT archivedirectory 결과
│   └── WindowsServer/
│       ├── ChronoSpaceServer.exe
│       └── ChronoSpace/
└── Client/
    └── Windows/
        └── ChronoSpace.exe
```

### 8.2 운영 시 실행 스크립트 예시

`Build/Server/start_server.bat`:

```bat
@echo off
set PORT=7777
set MAP=/Game/02_Map/L_StageSize
ChronoSpaceServer.exe %MAP% -port=%PORT% -log -nullrhi -unattended ^
  -SERVERTAG=ChronoSpace_KR_01
```

- `-nullrhi` : 렌더 백엔드를 명시적으로 끄기(이미 데디 빌드는 끔. 안전장치).
- `-unattended` : Cook/Build/Boot 시 다이얼로그 없음.
- `-log` 는 Development 에서만, Shipping 에서는 빼고 파일 로그 사용.

### 8.3 컨테이너/리눅스 (선택)

리눅스 데디로 갈 경우 `LinuxArm64Server` / `LinuxServer` 타겟 추가 빌드 필요. `DefaultEngine.ini` 의 `[/Script/LinuxTargetPlatform.LinuxTargetSettings]` 에 이미 SF_VULKAN 설정이 있는데, 데디는 RHI 가 의미 없으므로 무시해도 무방. Docker 베이스는 `ubuntu:22.04` 에 `libssl`, `libcurl`, `xdg-utils` 정도면 충분.

---

## 9. 하드웨어 / 호스팅 선택

서버 사양은 **게임 워크로드 → 인스턴스 1개의 자원 요구 → 박스 1대당 인스턴스 밀도 → 호스팅 모델** 순으로 결정한다. 추측 말고 측정해야 하는 값들이 있으니, 9.10 의 측정 절차를 먼저 보고 9.1~9.8 은 시작점 가이드로 사용할 것.

### 9.1 ChronoSpace 의 워크로드 특성 (사이징 입력값)

본 프로젝트 코드를 기준으로 본 사이징에 영향을 주는 요소들:

| 요소 | 본 프로젝트 값 | 영향 |
|------|----------------|------|
| 동시 접속 / 매치 | 현재 [`NumPublicConnections=2`](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp#L110), 향후 ~8명 가정 | CPU/Net 거의 선형 증가 |
| 게임 모델 | 매치 기반 (로비 → 스테이지) | 인스턴스 생성/소멸 빈번 → **stateless ephemeral 적합** |
| 물리 부하 | 중력 조작([CSCustomGravityDirComponent](../Source/ChronoSpace/ActorComponent/CSCustomGravityDirComponent.cpp), [CSGravityCoreSphere](../Source/ChronoSpace/Actor/CSGravityCoreSphere.cpp)), GAS | CPU 단일 스레드 부하 큼 |
| 틱레이트 | 카메라 동기 타이머가 [60Hz](../Source/ChronoSpace/Game/CSGameMode.cpp#L506) (`0.016f`) | 일반 30Hz 데디보다 1.5~2배 무거움 |
| Replicated 액터 | Player Character + [`CSCameraViewProxy`](../Source/ChronoSpace/Actor/CSCameraViewProxy.cpp) (인당 1) + GAS attribute | 네트워크는 가벼움 (인당 ~30 KB/s) |
| 콘텐츠 크기 | 다수 스테이지 맵 (Stage_01_01~Stage_03_01 등) | 디스크 / 첫 로드 시 RAM |
| 안티치트 | 사용 안 함 (확인됨, OSS 만 EIK) | OS 자유롭게 선택 가능 |

**한 줄 요약**: 매치 기반 + 물리 무거움 + 60Hz + 안티치트 없음 = **단일 스레드 빠른 x86 리눅스 박스에 인스턴스 다중 띄우기** 모델이 가장 잘 맞는다.

### 9.2 인스턴스 1개의 자원 요구 (시작 추정치)

UE 5.5 데디 일반적인 값 + 위 워크로드 보정:

| 자원 | 8인 매치 1개 (피크) | 8인 매치 1개 (평균) | 헤드룸 |
|------|---------------------|---------------------|--------|
| CPU | 단일 코어 100% | 단일 코어 30~50% | +30% |
| RAM | 2.5~4 GB | 2 GB | +30% |
| Disk(런타임) | 거의 0 (RAM 캐시) | — | — |
| Disk(설치) | pak + 콘텐츠 ~수 GB | — | — |
| Net 업/다운 | ~5 Mbps | ~2 Mbps | +50% |

> **현재 `NumPublicConnections=2` 라면 위 값의 30~40% 수준.** 본 프로젝트가 코업 2~4인 위주라면 인스턴스 자체는 매우 가볍다.

### 9.3 박스 1대당 인스턴스 밀도

```
박스의 vCPU = (목표 인스턴스 수 × 평균 코어 사용 0.4) + OS 헤드룸 1
박스의 RAM  = (목표 인스턴스 수 × 2.5 GB) + OS 1 GB
```

예시 매핑 (8인 매치 기준):

| 박스 | 사양 | 권장 동시 인스턴스 |
|------|------|--------------------|
| AWS `c7i.2xlarge` | 8 vCPU / 16 GB | 5~6 |
| AWS `c7i.4xlarge` | 16 vCPU / 32 GB | 10~12 |
| Hetzner AX52 | Ryzen 7900 (12C/24T) / 64 GB | 15~20 |
| OVH GAME-1 | Xeon E-2388G (8C/16T) / 64 GB | 12~14 |
| Linode Dedicated 32GB | 16 vCPU / 32 GB | 8~10 |

매치당 2인이라면 위 값 × 2~3 까지 늘릴 수 있다.

### 9.4 CPU — "단일 스레드가 전부"

UE 데디 서버는 **게임 스레드가 사실상 단일 스레드**이며, 물리/AI/네트워크는 부분적으로 워커로 분산된다. 코어 수가 많아도 단일 매치의 처리 속도가 빨라지지는 **않는다**. 따라서:

| 우선순위 | CPU | 비고 |
|----------|-----|------|
| 최우선 | AMD Ryzen 9000 / 7000 (Zen 4/5) | 단일 스레드 IPC 최강. Hetzner AX 라인 |
| 좋음 | Intel Xeon E-2400 (Raptor Lake-E) | 베어메탈 게임 호스팅에서 흔함. OVH GAME |
| 좋음 | AWS Graviton3 / 3E (ARM) | 클럭은 낮지만 가격/성능 비 우수. **다만 UE5 ARM Linux 데디 빌드 검증 필수** |
| 무난 | Intel Xeon Scalable (Ice Lake / Sapphire Rapids) | AWS c6i / c7i, GCP n2 |
| 피할 것 | 구형 EPYC 7001 (Naples), 저주파 멀티코어 Xeon | 코어 많고 단일 스레드 약함. 단가 싸 보여도 인스턴스당 처리 떨어짐 |

**숫자 한 줄**: Ryzen 7900X 한 코어 vs EPYC 7551 한 코어 = 게임 스레드 속도 약 2.2~2.5배 차이 (UE 5.x 일반 벤치 기준). 인스턴스 밀도가 그대로 2배 차이로 직결된다.

### 9.5 RAM / Disk / Network 가이드

- **RAM**: 인스턴스당 2~4 GB 잡고 시작. RAM 부족하면 OOM kill 로 매치 통째로 죽으니 **메모리는 살짝 과다하게**.
- **Disk**: 1대당 SSD 100~200 GB 충분. NVMe 권장이지만 게임 로직상 IOPS 중요하지 않음 — 첫 로드 후 RAM에서 동작.
- **Network**:
  - 박스당 100 Mbps ~ 1 Gbps 회선이면 충분.
  - **공인 IP + 직결 UDP** 가 핵심. NAT/Relay 뒤에 있으면 EOS Relay 의존 → 지연 +20~40ms.
  - DDoS 가 빈번하면 OVH GAME(`Anti-DDoS GAME` 포함), Cloudflare Spectrum, GCP Cloud Armor.

### 9.6 OS — Linux 권장

| 항목 | Ubuntu 22.04 LTS | Windows Server 2022 |
|------|------------------|---------------------|
| 라이선스 비용 | 무료 | ~$25~40/월 (호스팅 옵션 포함 시) |
| 메모리 풋프린트 | 작음 (~300 MB OS) | 큼 (~1.5 GB OS) |
| 컨테이너/오케스트레이션 | 매우 좋음 (Docker, k8s) | 어려움 |
| UE5 ShipServer 빌드 | Cross-compile 또는 리눅스 호스트 | 네이티브, 가장 빠름 |
| 안티치트 호환 (EAC/BE) | 지원 | 지원 |
| 운영 자동화 도구 | 풍부 (systemd, ansible) | 제한적 |

**권장: Ubuntu 22.04 LTS.** 본 프로젝트는 안티치트도 없고, 콘솔 출시 계획이 명확하지 않다면 리눅스 단일 OS 로 시작이 표준.

리눅스 데디 빌드를 위해서는 [Engine 의 Linux Cross-Compile Toolchain](https://docs.unrealengine.com/5.5/en-US/linux-development-requirements-for-unreal-engine/) 을 윈도우 빌드 머신에 설치해야 한다. 1번만 설정하면 기존 솔루션 컨피그에 `LinuxServer` 가 추가된다.

### 9.7 호스팅 모델 비교

| 모델 | 예 | 인스턴스 단가(8인 매치) | 장점 | 단점 | 적합 단계 |
|------|----|--------------------------|------|------|-----------|
| **베어메탈 임대** | Hetzner, OVH GAME, i3D | $5~10/월 | 단일 스레드 최강, 단가 최저 | DevOps 자가, 자동 스케일 X | 소프트런칭, 안정적 동접 |
| **VPS** | Linode, Vultr, DigitalOcean, Vultr Bare-Metal | $10~25/월 | 셋업 빠름, 리전 다양 | CPU 가상화 오버헤드 | 클로즈드 베타, QA |
| **클라우드 IaaS** | AWS EC2 / GCP / Azure | $30~80/월 | 자동 스케일, 리전 풍부, IAM | 비싸고 단일 스레드 약함 | 변동 동접 + 자동화 필요 |
| **게임 매니지드** | AWS GameLift, Hathora, Edgegap, Multiplay | 변동(매치당 과금) | 스케일링/매치메이킹 자동, 글로벌 엣지 | 가장 비쌈, 락인 가능 | 정식 출시, 글로벌 |
| **P2P + Relay** | EOS Relay 만 (서버 없음) | 무료 | 호스팅 비용 0 | 호스트 PC 사양 의존, NAT 이슈, 치팅 | 프로토타입, 친구방 |

**비용 감각** (8인 매치 가정):
- 정식 출시 동접 1,000 명(매치 125개) 기준
  - Hetzner AX52 × 8대 ≈ **$640/월**
  - AWS GameLift Spot ≈ **$1,500~2,500/월**
  - Hathora ≈ **$2,000~3,500/월**
- 단가 vs 운영 자동화의 트레이드오프. **소규모 인디라면 베어메탈, 변동 큰 글로벌 출시면 매니지드.**

### 9.8 단계별 추천 구성

| 단계 | 추천 | 비용 | 비고 |
|------|------|------|------|
| 개발/내부 QA | 사무실 PC + 포트포워딩, 또는 VPS $5/월 | ~$0~5/월 | EOS Dev Tool 인증 |
| 클로즈드 베타 (10~50 동접) | VPS 1대 (Linode 4GB / Vultr High-Freq) | $10~25/월 | 단일 리전 |
| 소프트런칭 (100~500 동접) | Hetzner AX52 1~2대 + Cloudflare DDoS | $80~160/월 | KR or JP 리전 |
| 정식 출시 (500~5,000 동접) | Hetzner 5~10대 + 매니지드 매치메이킹(GameLift FleetIQ 또는 Hathora) | $500~3,000/월 | 다중 리전 |
| 글로벌 / 변동 큼 | GameLift / Hathora 풀매니지드 | 매치당 과금 | 자동 스케일 |

본 프로젝트는 일단 **VPS 1대 → Hetzner AX 1대 → 매니지드 검토** 순서를 권장.

### 9.9 리전 / 네트워크 정책

- **한국 게임 가정**: KR(서울 LG U+ / KINX), JP(도쿄), US-West(LA), EU(프랑크푸르트) 4리전이 일반적인 출발점.
- **목표 핑**: 같은 국가 <30ms, 같은 대륙 <80ms, 대륙 간 <150ms.
- **매칭 정책**: 리전 분리 매칭이 기본. EIK Session 의 `SEARCH_REGION` 같은 커스텀 키로 분리하거나, 호스팅 레이어가 라우팅.
- **포트**:
  - 게임 UDP: 7777 (개당), 매치마다 +1
  - EOS QueryPort: 보통 27015~27050 범위 사용
  - SSH/관리: 22 (변경 권장)
  - 박스마다 인스턴스 N개면 **N개 UDP 포트 개방 필요** — 보안그룹/iptables 자동화 필수

### 9.10 사이징 측정 절차 (실측이 우선)

위의 모든 추정치는 **출발점**일 뿐. 한 번이라도 실측해서 자기 게임 워크로드의 실수치를 가져야 한다.

```
1) Development server 빌드 후 단일 인스턴스 띄움
2) 실제 8인(또는 목표 인원) 봇/QA 접속
3) 콘솔 명령:
   stat unit         → Server FPS, Game thread time
   stat unitgraph    → 그래프
   stat net          → Replication 비용
   stat memory       → RAM 점유
4) 30분 매치 시뮬레이션 후
   - 평균 game thread ms (목표: 60Hz=16ms 이하, 30Hz=33ms 이하)
   - 피크 메모리
   - 평균/피크 인당 bandwidth
5) 위 값을 9.2 표 대신 사용하여 9.3 밀도 재계산
```

**측정 없이 무지성 클라우드 인스턴스부터 띄우면 거의 항상 과/저 사양으로 가서 한 달 안에 다시 사이징한다.**

### 9.11 컨테이너 vs 베어 프로세스

- **베어 프로세스 + systemd**: 가장 단순, 박스 1대 = 수동 또는 ansible 로 N개 인스턴스 띄움. 인디 규모 충분.
- **Docker**: 인스턴스 격리, 로그/리소스 제한 일관. CI 와 통합 쉬움. 약간의 성능 오버헤드(~2~5%).
- **Kubernetes**: 자동 스케일/롤링 업데이트 필요할 때. **Agones** (Google) 가 게임 데디 전용 쿠버 오퍼레이터로 표준이 되어 가는 중. 정식 출시 단계 이상에서 검토.

본 프로젝트 단계라면 **systemd 단일 박스 N프로세스** 부터 시작하고, 수요 증가 후 Docker → Agones 로 가는 점진적 경로가 합리적.

### 9.12 모니터링 / 운영 필수 항목

- **메트릭**: Server FPS, Game thread ms, 인스턴스 수, RAM, 매치 평균 수명, 크래시율. Prometheus + Grafana 또는 클라우드 자체.
- **로그**: stdout 을 `journald` 또는 파일 → ELK / Loki 로. UE 의 `LogOnline VeryVerbose` 는 운영 시 **반드시 끄기** (디스크/CPU 잡아먹음). 현재 [DefaultEngine.ini:241-244](../Config/DefaultEngine.ini#L241-L244) 가 켜져 있음 — Shipping 빌드용 ini 분기 필요.
- **셧다운 훅**: SIGTERM 받으면 `EndSession` → `DestroySession` 후 그레이스풀 종료. 안 하면 EOS 좀비 세션이 누적.
- **헬스체크**: `--healthcheckport=NNNN` 같은 옵션을 직접 구현하거나, `EOS_Sessions_GetSessionDetails` 폴링.

---

## 10. 단계별 마이그레이션 로드맵

작은 단위로 끊어서 진행하길 권장.

| 단계 | 작업 | 검증 |
|------|------|------|
| 1 | `ChronoSpaceServer.Target.cs` 추가, 빌드만 통과시키기 | 솔루션에서 Server 컨피그 컴파일 OK |
| 2 | `?listen` 모두 제거, `NM_ListenServer` → 적절히 분기 | PIE 데디 모드로 부팅, 첫 맵 로드 OK |
| 3 | `CSEIKSubsystem` 데디/클라 분기, `bIsDedicated=true` 세션 생성 | 데디 부팅 시 EOS 콘솔에 세션 표시 |
| 4 | 클라 `FindSessions` 가 데디 세션을 찾고 Join 성공 | 두 클라가 동일 세션 들어옴 |
| 5 | Split Screen 결정(A/B) 적용 | 인게임에서 의도된 화면 출력 |
| 6 | 데디 인증 (DedicatedServer credentials) 운영용으로 교체 | 운영 EOS 환경에서 정상 인증 |
| 7 | 서버 셧다운 시 `EndSession`/`DestroySession` 호출 추가 | EOS 콘솔에 좀비 세션 없음 |
| 8 | Shipping 빌드 + 운영 머신 배포 | 외부 클라가 정상 접속 |

각 단계 완료 시점마다 **클라/서버 로그 양쪽** 다 확인. 특히 `LogOnline VeryVerbose` ([DefaultEngine.ini:241-244](../Config/DefaultEngine.ini#L241-L244)) 가 켜져 있으니 EOS 호출 결과를 추적할 수 있다.

---

## 11. 트러블슈팅 자주 나오는 함정

1. **"클라가 세션을 찾는데 안 보임"** — `bUsesPresence`/`bUseLobbiesIfAvailable` 의 일관성. 데디 세션은 `false`/`false`, 클라 검색도 `SEARCH_DEDICATED_ONLY=true` 로 맞춰야 함.
2. **"서버 ServerTravel 후 클라가 떨어짐"** — Travel 시점에 `ConnectInfo` 가 변하지 않는데 클라가 자동으로 따라가는지 확인. SeamlessTravel 사용 여부도 확인 (`ACSLobbyGameMode::bUseSeamlessTravel = false` 로 되어 있음 → 정상).
3. **"GameViewport nullptr 크래시"** — `GEngine->GameViewport->...` 호출 코드 패스가 데디에서도 동작하면 nullptr 체크 추가. `IsRunningDedicatedServer()` 글로벌 헬퍼 활용.
4. **"세션이 만들어졌는데 NetDriver 가 안 뜸"** — 데디 부팅 시 첫 맵 로드 후에 `CreateSession` 을 호출해야 EIK NetDriver 가 정확히 잡힘. `OnStartSessionComplete` 에서 `ServerTravel` 호출 순서 유지.
5. **"클라이언트가 연결되지만 GameMode 가 PostLogin 을 못 받음"** — GameMode 가 클라 빌드에 들어 있는지 확인. 같은 모듈에 들어 있으니 자동 OK 이지만, 모듈 분리 시 주의.

---

## 12. 참고 자료

- Epic 공식: *Setting Up Dedicated Servers* (UE 5.x)
- EOS Integration Kit 문서 — Dedicated Server 섹션 (`bIsDedicated`, `dedicated_server` credentials)
- `Engine/Source/Runtime/Engine/Classes/Engine/GameInstance.h` — `IsDedicatedServerInstance`
- 본 리포 내 관련 클래스 진입점:
  - [CSGameMode.cpp](../Source/ChronoSpace/Game/CSGameMode.cpp), [CSGameMode.h](../Source/ChronoSpace/Game/CSGameMode.h)
  - [CSEIKSubsystem.cpp](../Source/ChronoSpace/Subsystem/CSEIKSubsystem.cpp)
  - [CSPlayerController.cpp](../Source/ChronoSpace/Player/CSPlayerController.cpp)
  - [CSCameraViewProxy.cpp](../Source/ChronoSpace/Actor/CSCameraViewProxy.cpp)
  - [CSSplitScreenSubsystem.cpp](../Source/ChronoSpace/Subsystem/CSSplitScreenSubsystem.cpp)
  - [DefaultEngine.ini](../Config/DefaultEngine.ini)
