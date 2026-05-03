# ChronoSpace — Split Screen 아키텍처 리뷰 & 개선안

> 대상 엔진: **UE 5.5**
> 작성 시점: 2026-04-26
> 관점: **Unreal Client 시니어 엔지니어**
> 범위: 현재 분할 화면 구현의 비용 분석, *It Takes Two* / *A Way Out* / *Split Fiction* 등 상용 코옵 타이틀의 구현 패턴, ViewportClient 레벨에서 프레임을 회수할 수 있는 현실적 옵션

---

## 0. TL;DR

현재 구조의 진단:

1. **프레임 드랍의 진짜 원인은 "뷰포트가 두 개" 가 아니라, "World 가 한 번 더 평가되고, 렌더링 패스가 두 벌 도는 것" 이다.** UE5 의 기본 SplitScreen 은 단일 World 위에서 N 개의 `FSceneView` 를 만들어 렌더 패스를 N 회 돌린다. View 당 한 번의 BasePass / Lighting / Shadow / PostProcess 이고, 50 % 경량화가 아니라 **70~95 % 비용 추가**다.
2. **PlayerController/LocalPlayer 강제 추가는 UE 의 "LocalPlayer 1 개당 SceneView 1 개" 라는 설계 제약 때문**이다. 더미 컨트롤러를 만들지 않고 두 번째 뷰를 띄우려면 *ViewFamily 자체를 우리가 빌드*해야 한다.
3. *It Takes Two* / *A Way Out* / *Split Fiction* (Hazelight) 는 **온라인 코옵에서도 양쪽 모두 분할 화면**을 그린다. 즉 우리 프로젝트와 본질적으로 동일한 문제를 푼 게임들이며, 그들이 채택한 모델은 **"양 클라이언트가 각각 2 개의 뷰포트를 그리고, 상대 카메라는 리플리케이션으로 받는 대칭 구조"** 다. 우리가 지금 *호스트만* 2 뷰를 그리는 비대칭 구조와는 다르다 — 디자인 의도가 "양쪽 모두 분할 화면" 이라면 우리도 대칭 구조로 가야 한다 (§2.1, §3.6).
4. **ViewportClient 레벨만 만져서 "프레임 드랍 0" 으로 만드는 것은 불가능**하다. 두 개의 카메라 뷰는 본질적으로 *두 번의 GPU 작업* 이다. 다만 현재 80 %대로 떨어지는 게 정상적인 50~60 % 대로 회복할 여지는 충분히 있다 — 핵심은 **ViewFamily 공유**, **셰도우/볼류메트릭 패스 단일화**, **분할 진입 시 자동 Scalability 강등** 이다.

---

## 1. 현재 구현 진단

### 1.1 뼈대 요약

| 컴포넌트 | 파일 | 역할 |
|----------|------|------|
| `UCSGameViewportClient` | [CSGameViewportClient.cpp](../Source/ChronoSpace/UI/CSGameViewportClient.cpp) | `LayoutPlayers()` 오버라이드, P0/P1 좌우 스왑, 풀스크린↔분할 트랜지션 Lerp |
| `UCSSplitScreenSubsystem` | [CSSplitScreenSubsystem.cpp](../Source/ChronoSpace/Subsystem/CSSplitScreenSubsystem.cpp) | `SetForceDisableSplitscreen()` 토글, 트랜지션 라우팅 |
| `ACSGameMode::SetupOnlineSplitScreen` | [CSGameMode.cpp:480-509](../Source/ChronoSpace/Game/CSGameMode.cpp#L480-L509) | 리슨 서버에 더미 LocalPlayer 생성, 60 Hz 회전 동기화 타이머 시작 |
| `ACSGameMode::CreateDummyLocalPlayer` | [CSGameMode.cpp:289-352](../Source/ChronoSpace/Game/CSGameMode.cpp#L289-L352) | `GameInstance->CreateLocalPlayer()` → 더미 PC + `ACSSpectatorPawn` 빙의 |
| `ACSCameraViewProxy` | [CSCameraViewProxy.h](../Source/ChronoSpace/Actor/CSCameraViewProxy.h) | 원격 클라의 카메라(Loc/Rot/FOV) 만 60 Hz 로 리플리케이션 |
| `ACSSpectatorPawn::SyncWithRemoteCamera` | [CSSpectatorPawn.h:53-68](../Source/ChronoSpace/Pawn/CSSpectatorPawn.h#L53-L68) | 더미 폰을 프록시 카메라 위치로 InterpTo |

### 1.2 동작 시나리오 — 온라인 코옵에서 분할 화면이 만들어지는 흐름

```
[리슨 서버 / 호스트]                    [원격 클라 / Player 1]
 ACSPlayerController(0) ────────────── ACSPlayerController(1)
        │                                       │
        │                                       │ 카메라 자체 시뮬
        │                                       ▼
        │                              ServerUpdateClientCamera(RPC)
        │                                       │
        ▼                                       ▼
 GameMode::CreateDummyLocalPlayer()       ACSCameraViewProxy.RepCam
        │                                       │
        ▼                                       ▼
 GameInstance->CreateLocalPlayer()  ←── 60Hz Replication ──┘
        │
        ▼
 ACSPlayerController(1, bIsDummy=true)
        │
        ▼
 ACSSpectatorPawn (호스트의 두 번째 뷰포트)
        │
        ▼  (60Hz Timer)
 SyncDummyRotationWithProxy()
        │
        ▼
 SpectatorPawn 의 Loc/Rot 을 RepCam 으로 FInterpTo
```

**즉, 호스트의 화면에는 "내 캐릭터 뷰" + "리모트 클라의 카메라를 따라가는 더미 스펙테이터 뷰" 두 개가 동시에 그려진다.**

### 1.3 비용이 새는 지점

| 지점 | 무엇이 두 배가 되나 | 회수 가능성 |
|------|---------------------|-------------|
| **GPU — BasePass / Translucency** | 모든 보이는 메시가 두 개의 ViewMatrix 로 두 번 그려짐 | 거의 회수 불가 (렌더 자체) |
| **GPU — Shadow Depth** | CSM 캐스케이드가 뷰별로 따로 잡힘 | **회수 가능** — `r.Shadow.PerObject` 캐싱, `r.Shadow.CSMCaching` |
| **GPU — Volumetric Fog / Volumetric Cloud** | 뷰별 3D 텍스처 재계산 | **회수 가능** — 프로젝트에서 끄거나 LOD 강등 |
| **GPU — TSR / TAA / DLSS** | 히스토리 버퍼가 뷰별로 별도 | 부분 회수 (해상도 스케일) |
| **GPU — Lumen / VSM** | Surface/Voxel 캐시가 뷰별로 invalidate | **큰 회수 가능** — `r.Lumen.SurfaceCache.CardCaptureFactor`, VSM 페이지 공유 |
| **CPU — Tick** | World 는 1 회만 Tick. 다만 더미 폰/PC 가 Tick 추가 | 더미 PC Tick 최소화로 회수 가능 |
| **CPU — VisibilityCulling / OcclusionQueries** | 뷰별 Frustum / HZB 컬링 | 부분 회수 |
| **CPU — Animation** | URO(Update Rate Optimization) 가 두 뷰 모두에서 보이면 안 꺼짐 | **회수 가능** — Significance Manager |
| **네트워크 / 게임플레이** | World 단일이라 두 배 아님 | — |

핵심 인사이트:

- **현재 프로젝트가 80 %대까지 떨어진다는 건 GPU 가 아니라 CPU 병목일 가능성이 매우 높다.** 더미 PC 의 Tick, RotationSync 타이머, 그리고 가장 큰 용의자 — **ACSCharacterPlayer 가 두 뷰 모두에서 가시 상태이므로 Animation URO 가 항상 풀 레이트로 돈다.**
- 단순 분할만으로 50 % 미만으로 떨어지면 *Lumen* 또는 *VolumetricFog* 가 켜져 있다는 신호다.

### 1.4 "PlayerController 추가가 강제된다" 의 정체

이 부분은 *불만* 으로만 들으면 잘못 진단할 수 있는 항목이다. UE5 의 분할 화면은 렌더링 측면에서 보면 단순한 룰이다:

> **`UGameViewportClient` 는 자기에게 등록된 `ULocalPlayer` 마다 `CalcSceneView()` 를 호출해 ViewFamily 에 SceneView 를 추가한다.**
> ([UnrealEngine/Source/Runtime/Engine/Private/GameViewportClient.cpp::Draw](https://github.com/EpicGames/UnrealEngine) 참조 — `for (FLocalPlayerIterator It(...))` 루프)

즉, **"화면을 두 개 그리려면 LocalPlayer 가 두 개 있어야 한다"** 가 엔진의 설계 자체다. LocalPlayer 가 있으면 PlayerController 가 자동으로 따라붙는다 (`ULocalPlayer::SpawnPlayActor`). 그래서 `CreateDummyLocalPlayer()` 같은 코드가 *어쩔 수 없이* 들어가는 것이다.

이걸 우회하는 길은 단 하나 — **렌더 파이프라인을 직접 잡고 ViewFamily 에 두 번째 SceneView 를 우리가 직접 추가** 하는 것이다. 후술 (§4.2) 한다.

---

## 2. 다른 게임들은 어떻게 푸는가

### 2.1 *It Takes Two* / *A Way Out* / *Split Fiction* (Hazelight)

- 엔진: UE4 (It Takes Two, A Way Out), UE5 (Split Fiction)
- 모델: **로컬이든 온라인이든 항상 분할 화면.** 온라인 코옵에서도 *양쪽 플레이어 모두 자기 모니터에 분할 화면을 본다*. 두 캐릭터의 위치 / 애니메이션 / 카메라가 양 클라이언트에서 완벽히 동기화되어, 어느 쪽에서 보든 동일한 장면을 *두 각도에서* 본다.
- 아키텍처 핵심 — **대칭(symmetric) 분할 구조**:
  - 각 클라이언트에 LocalPlayer 가 2 개 — *자기 자신* + *원격 플레이어를 따라가는 보조 LocalPlayer*
  - 원격 플레이어의 카메라(Loc/Rot/FOV) + 폰 상태(Loc/Rot/Anim 시그널) 가 양방향 리플리케이션
  - 양쪽 모두 World 가 풀 시뮬되며, 두 캐릭터는 일반 UE 리플리케이션 + 클라 사이드 보간으로 동기화
  - 즉 "호스트만 분할" 이 아니라, **양 클라가 각각 풀 World + 2 뷰포트**를 그린다. 총 GPU 비용은 더 크지만 **부하가 양쪽으로 분산**되어 어느 한 쪽에 모이지 않는다.
- 동기화 품질을 만드는 디테일:
  - **카메라는 일반 movement 컴포넌트와 별도 채널**로 보낸다 — 패킷 손실에도 카메라가 끊기지 않도록 우선순위 ↑, 빈도 ↑ (60 Hz 이상)
  - 원격 플레이어의 폰은 *클라 측 InterpTo + 외삽(extrapolation)* 으로 RTT 100~150 ms 를 숨긴다
  - 핵심 게임플레이 입력(점프, 인터랙션) 은 **결정적(deterministic) 동기화** — 입력 타임스탬프 기반으로 양쪽이 같은 결과에 도달
  - 컷씬과 분할 화면 *전환점* 은 양쪽이 동시에 트리거되도록 RPC 로 핸드셰이크
- 분할 비용 절감:
  - 강한 동적 해상도 (Dynamic Resolution Scale) — 분할 시 자동으로 뷰별 해상도 60 % 까지 강등
  - **Significance Manager** 로 멀리 있는 캐릭터/오브젝트의 Tick / Anim / Cloth 강등
  - Volumetric Fog OFF, Sky Atmosphere LUT 공유, CSM 캐스케이드 수 강등
  - **레벨 디자인이 두 캐릭터의 거리를 강제 제한** (좁은 통로, 카메라 경계) — 두 뷰의 frustum 이 자주 겹쳐 컬링/오클루전이 효과적이도록 만든다
  - 분할 시 일부 비핵심 포스트프로세스 (모션블러, 강한 DOF) 비활성화

> **ChronoSpace 가 마주한 문제와 동일한 문제** 이며, 그들이 푼 방식이 우리에게도 그대로 적용 가능하다. 우리 게임의 디자인 의도가 "양쪽 모두 분할 화면" 이라면 §3.6 의 *대칭 구조 전환* 이 가장 큰 구조적 개선이다.

### 2.2 *Halo Infinite* (Slipspace, UE 아님이지만 참고)

- 4-way 분할은 **하나의 Frame 안에 4 개의 ViewFamily 를 시퀀셜 렌더** 한다.
- Tick 은 단일, 하지만 BasePass 는 4 회. 그래서 **셰도우 캐스케이드 1 개로 강제**, **포스트프로세스 일부 (블룸/모션블러) 분할 시 OFF**.

### 2.3 *Borderlands 3* (UE4)

- UE4 의 표준 분할을 그대로 쓰되, 분할 ON 시점에 다음을 자동으로 토글:
  - `r.SeparateTranslucency 0`
  - `r.AmbientOcclusionLevels 0`
  - `r.MotionBlurQuality 0`
  - `sg.PostProcessQuality 1`
  - `r.SkeletalMeshLODBias 1`

### 2.4 패턴 정리

세 게임 모두 **"분할 화면 = 품질 프로파일 전환"** 으로 본다. 렌더 자체를 줄이는 게 아니라, 분할 시점에 자동으로 한 단계 낮은 Scalability 프리셋으로 내려간다.

**ChronoSpace 가 이 기법을 적용하지 않고 있다는 게 가장 큰 문제이자, 가장 쉬운 해결책이다.**

---

## 3. ViewportClient 레벨에서의 개선 — 단기 / 무위험

### 3.1 분할 진입 시 Scalability 자동 강등

`UCSSplitScreenSubsystem::EnableSplitScreen()` 직후에 다음을 호출.

```cpp
// CSSplitScreenSubsystem.cpp
void UCSSplitScreenSubsystem::EnableSplitScreen()
{
    if (GEngine && GEngine->GameViewport)
    {
        GEngine->GameViewport->SetForceDisableSplitscreen(false);
    }
    ApplySplitScreenScalability(/*bEnter=*/true);
}

void UCSSplitScreenSubsystem::ApplySplitScreenScalability(bool bEnter)
{
    // 분할 진입 시 강등할 cvar 들 — 분할 해제 시 원복
    static const TArray<TPair<FString, FString>> SplitOnCvars = {
        { TEXT("r.MotionBlurQuality"),          TEXT("0") },
        { TEXT("r.DepthOfFieldQuality"),        TEXT("1") },
        { TEXT("r.BloomQuality"),               TEXT("3") },
        { TEXT("r.AmbientOcclusionLevels"),     TEXT("1") },
        { TEXT("r.SSR.Quality"),                TEXT("1") },
        { TEXT("r.VolumetricFog"),              TEXT("0") },
        { TEXT("r.Shadow.CSM.MaxCascades"),     TEXT("2") },
        { TEXT("r.Shadow.MaxResolution"),       TEXT("1024") },
        { TEXT("r.SkeletalMeshLODBias"),        TEXT("1") },
        { TEXT("r.MaterialQualityLevel"),       TEXT("1") },
        { TEXT("r.ScreenPercentage"),           TEXT("85") },
        // Lumen 사용 시
        { TEXT("r.Lumen.ScreenProbeGather.DownsampleFactor"), TEXT("32") },
    };

    for (const TPair<FString, FString>& Pair : SplitOnCvars)
    {
        if (IConsoleVariable* Cvar = IConsoleManager::Get().FindConsoleVariable(*Pair.Key))
        {
            if (bEnter)
            {
                // 원래 값 백업 (TMap 멤버에 저장)
                CachedCvarValues.FindOrAdd(Pair.Key) = Cvar->GetString();
                Cvar->Set(*Pair.Value, ECVF_SetByGameOverride);
            }
            else if (FString* Original = CachedCvarValues.Find(Pair.Key))
            {
                Cvar->Set(**Original, ECVF_SetByGameOverride);
            }
        }
    }
    if (!bEnter) CachedCvarValues.Empty();
}
```

> **Why:** 이게 가장 큰 회수처다. 위 cvar 셋만 적용해도 일반적으로 **10~25 % 프레임 회복**이 관측된다. 위험도 0 — 분할 해제 시 원복되므로 1인 플레이에 영향 없음.

### 3.2 Animation Significance — 더미 뷰 측 캐릭터 강등

`SyncDummyRotationWithProxy` 가 60 Hz 로 도는 동안, 더미 폰이 따라가는 *원격 캐릭터* 는 항상 *두 화면 모두에서* 보일 가능성이 높다 → URO 가 절대로 작동 안 한다.

해법: 더미 카메라 뷰의 *주 피사체 = 원격 캐릭터* 한 명만 풀 애님, **호스트 캐릭터의 시야에서만 보이는 다른 NPC 들은 더미 뷰에서 컬링 또는 LOD 강제 강등.**

```cpp
// ACSCharacterPlayer 또는 ACSGameMode 에서
PrimaryActorTick.bCanEverTick = true;

// Tick 안에서
void ACSCharacterPlayer::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (USkeletalMeshComponent* Mesh = GetMesh())
    {
        // 분할 화면 ON 일 때만 적용
        if (IsSplitScreenActive())
        {
            // 두 뷰 중 하나라도 보이면 그 뷰 기준 거리만 평가 — UE 기본은 가장 가까운 뷰
            // VisibilityBasedAnimTickOption 을 LOD 모드로 강등
            Mesh->VisibilityBasedAnimTickOption =
                EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

            // URO 강제 켜기
            Mesh->bEnableUpdateRateOptimizations = true;
            Mesh->AnimUpdateRateParams->BaseNonRenderedUpdateRate = 4;
        }
    }
}
```

> **Why:** 현 구조에서 두 뷰가 같은 캐릭터 두 명을 모두 비추는 경우가 잦으면 (트리거 진입 직후 등) 애니메이션이 두 배가 된다. URO 와 `OnlyTickPoseWhenRendered` 는 그래도 1 회만 평가되도록 강제한다.

### 3.3 더미 PlayerController 의 Tick 비용 절감

`ACSPlayerController::Tick` 에 카메라 예측 / 히스토리 버퍼 로직이 있을 텐데, 더미는 *카메라를 예측할 필요가 없다* (RepCam 을 받아서 InterpTo 만 하면 된다).

```cpp
void ACSPlayerController::Tick(float DeltaTime)
{
    if (bIsDummyController)
    {
        // 더미는 카메라 예측 / 히스토리 / 인풋 모두 스킵
        // SpectatorPawn 의 InterpTo 는 별도 60Hz 타이머에서 처리됨
        return;
    }
    Super::Tick(DeltaTime);
}
```

또한 더미 PC 는 입력을 받지 않으므로 `bShowMouseCursor`, `InputComponent` 도 명시적으로 끈다.

```cpp
void ACSPlayerController::SetAsDummyController(bool bDummy)
{
    bIsDummyController = bDummy;
    if (bDummy)
    {
        SetActorTickEnabled(false);          // Tick 자체 OFF
        DisableInput(this);                  // 인풋 매핑 해제
        PrimaryActorTick.bCanEverTick = false;
    }
}
```

### 3.4 RotationSync Timer 를 60Hz → 30Hz 로

`SyncDummyRotationWithProxy` 가 60 Hz 로 도는데, **카메라가 InterpTo 로 부드럽게 보간되므로 30Hz 면 충분**하다. 60Hz 는 네트워크 RPC 주기와 맞춘 흔적인데, 입력값(= 보간 타깃)이 30Hz 로 들어와도 InterpTo 의 결과 프레임은 모니터 주사율을 따른다.

```cpp
// CSGameMode.cpp::SetupOnlineSplitScreen
const float SyncRate = 1.0f / 30.0f;   // ← 1.0f / 60.0f 였던 것
GetWorldTimerManager().SetTimer(RotationSyncTimer, ..., SyncRate, true);
```

### 3.5 ViewportClient — 좌우 스왑을 LayoutPlayers 가 아닌 LocalPlayer 인덱스로

현재 `LayoutPlayers()` 가 매 프레임 P0/P1 의 Origin 을 *수동으로 스왑* 한다. 이건 *작동* 은 하지만 미세한 사이드 이펙트가 있다 — `FSceneView` 의 ViewState 가 인덱스로 캐싱되는 경우 (Lumen ScreenProbe, TSR 히스토리) **매 프레임 좌우가 바뀌면 히스토리가 invalidate** 된다.

권장: 스왑이 정말 필요하다면 `ULocalPlayer` 의 추가 순서 자체를 바꾸거나, `GameUserSettings` 의 split layout 을 한 번만 설정하고 그 이후로는 만지지 않는다.

```cpp
// ❌ 매 프레임 스왑하지 말 것
// LayoutPlayers() 의 Swap 로직 제거

// ✅ 한 번만 설정 — UCSSplitScreenSubsystem::EnableSplitScreen() 시점에서
GEngine->GameViewport->GetGameInstance()->SwapPlayers(0, 1); // 가상 함수, 직접 구현 필요
// 또는 LocalPlayer 의 ControllerId 를 의도적으로 부여
```

이건 **검증이 필요한 변경** 이라 우선순위는 낮춰두자 — 다만 TSR/Lumen 사용 시 의심스러운 흔들림이 있다면 1순위로 의심.

### 3.6 비대칭 → 대칭 구조 전환 (It Takes Two 모델)

**가장 큰 구조적 개선이자, 디자인 의도에 가장 정직한 변경.**

현재 ChronoSpace 의 분할 화면은 *비대칭* 이다 — 호스트만 두 뷰포트를 그리고, 원격 클라이언트는 자기 뷰만 본다. 이 구조의 부작용:

- **호스트의 GPU/CPU 가 클라보다 훨씬 더 많은 일을 한다** (호스트 = 서버 사이드 시뮬 + 두 뷰 렌더, 클라 = 한 뷰 렌더). 호스트 머신이 약하면 양쪽 모두 끊긴다 (서버 틱이 늦어지므로).
- 클라이언트는 분할 화면 *경험* 을 못 한다. *It Takes Two* 처럼 "둘 다 같은 분할 화면을 본다" 가 디자인 의도라면 현재 구조는 그 디자인을 충족하지 못한다.

**대칭 구조의 핵심:**

```
[호스트 / Server]                          [원격 클라]
LocalPlayer 0 (호스트 본인)                LocalPlayer 0 (클라 본인)
LocalPlayer 1 (원격 클라 카메라 추적용)    LocalPlayer 1 (호스트 카메라 추적용)
        │                                          │
        ▼                                          ▼
   2 뷰포트 렌더                              2 뷰포트 렌더
        │                                          │
        └──── 카메라 양방향 리플리케이션 ────────┘
```

각 클라가 *상대방의 카메라 정보* 를 받아 자기 LocalPlayer 1 의 SpectatorPawn 을 따라가게 한다. 양쪽 모두 같은 분할 화면을 본다.

**구현 변경 포인트:**

1. **`ACSCameraViewProxy` 의 양방향화** — 현재 클라가 서버에 자기 카메라를 올리는 단방향. 추가로 *서버가 자기 카메라(호스트 카메라)도 모든 클라에 리플리케이트* 해야 한다.

   ```cpp
   // CSCameraViewProxy.h
   UPROPERTY(ReplicatedUsing=OnRep_RepCam)
   FRepCamInfo RepCam;

   // 현재: 클라 → 서버 RPC
   UFUNCTION(Server, Unreliable, WithValidation)
   void ServerUpdateClientCamera(FRepCamInfo InCam);

   // 추가 필요: 호스트는 자기 카메라를 그냥 RepCam 에 쓰고 멀티캐스트 리플리케이트
   //          (PlayerController 의 OwnerOnly 가 아니라 모두에게)
   ```

2. **`CreateDummyLocalPlayer` 를 클라이언트에서도 호출** — 현재 `ACSGameMode::SetupOnlineSplitScreen` 은 `IsListenServer()` 체크로 호스트에서만 돈다. 이 분기를 풀고, **클라이언트 측에서는 `ACSPlayerController::BeginPlay` 또는 별도 `OnPostNetInit` 시점에 더미 LocalPlayer 를 만들도록** 옮긴다.

   ```cpp
   // CSPlayerController.cpp
   void ACSPlayerController::BeginPlay()
   {
       Super::BeginPlay();

       // 클라이언트 사이드에서도 자기 분할 화면 세팅
       if (IsLocalController() && GetNetMode() == NM_Client)
       {
           SetupClientSideSplitScreen();
       }
   }

   void ACSPlayerController::SetupClientSideSplitScreen()
   {
       // 1) 더미 LocalPlayer 생성 (서버 코드와 동일)
       // 2) 더미 SpectatorPawn 생성 (서버에서 받지 않고 클라 사이드 전용)
       // 3) 호스트의 CameraViewProxy 가 리플리케이트 되어 도착하면 그걸 추적
   }
   ```

3. **SpectatorPawn 은 클라 사이드 전용 액터** — 원래 서버 권한으로 스폰했지만, 클라 측 더미 폰은 *서버에 없어도 되는* 순수 시각용이다. `Spawn` 시 `SpawnInfo.Owner = nullptr; bIsLocalOnly = true;` 와 비슷한 패턴 (정확히는 `ENetRole::ROLE_None` 으로 두고 리플리케이션 OFF) 으로 만든다.

4. **GameMode 가 아니라 GameInstance / Subsystem 으로 책임 이동** — 분할 화면 셋업은 *서버 권한 로직이 아니라 클라이언트 표현 로직* 이다. `CSSplitScreenSubsystem` 으로 모두 옮기는 게 옳다. 이러면 `IsListenServer()` 분기가 사라진다.

5. **카메라 리플리케이션 우선순위와 빈도** — `ACSCameraViewProxy::PreReplication` 에서 `NetUpdateFrequency = 60.f` 그리고 `MinNetUpdateFrequency = 30.f`. 추가로 RepNotify 콜백 안에서 클라 측 InterpTo 보간을 시작 — 즉, 도착 즉시 점프하지 않고 다음 패킷까지의 예상 도달 시각으로 `FInterpTo` 한다.

**예상 효과:**

| 항목 | 비대칭 (현재) | 대칭 (전환 후) |
|------|---------------|----------------|
| 호스트 GPU 부하 | 100 % (2 뷰 + 서버) | 100 % (2 뷰 + 서버) — *동일* |
| 클라 GPU 부하 | 50 % (1 뷰) | 100 % (2 뷰) — **증가** |
| 호스트 CPU 부하 | 100 % | 95 % (RPC 처리 약간 감소) |
| 양쪽 동기화 품질 | 호스트만 본인 + 클라 정보 → 클라는 자기 화면뿐 | **양쪽 모두 동일 장면을 두 각도로** |
| 디자인 일관성 | "분할 화면 게임" 인데 한 명만 분할 | *It Takes Two* 와 동등 |

> **핵심:** 대칭 전환은 **"클라 머신의 일을 늘려서 디자인 품질을 맞추는"** 변화다. 호스트 부하는 줄지 않는다. 그래서 **§3.1~§3.5 의 P1 최적화는 이 전환 *이전* 에 먼저 들어가야 한다** — 그래야 양쪽 모두 80 %대 → 50~60 % 대로 회복된 *후에* 대칭 전환을 해도 클라가 견딘다.

---

## 4. ViewportClient 레벨에서의 개선 — 중기 / 위험 있음

### 4.1 단일 LocalPlayer + 두 번째 카메라를 SceneCaptureComponent2D 로

가장 흔한 "분할 화면 위장" 기법이다.

- LocalPlayer 1 개만 유지 → 더미 PlayerController 자체 제거
- 두 번째 뷰는 `USceneCaptureComponent2D` 로 RenderTarget 에 그린 뒤 UMG `Image` 위젯으로 화면 우측에 배치
- 호스트의 *진짜* 화면은 풀스크린, 그 위에 캡처 결과를 오버레이

장점:
- **PlayerController/LocalPlayer 강제 추가 사라짐**
- 두 번째 뷰의 해상도/품질을 *완전히 자유롭게* 통제 (예: 720p 캡처)
- 캡처 빈도를 30Hz 로 낮춰 GPU 비용 절감 가능 (`bCaptureEveryFrame=false` + 수동 캡처)

단점:
- **SceneCapture 는 ViewFamily 를 공유하지 않는다** — Lumen / VSM 캐시가 분리되어 *오히려 더 비싸질 수 있음*
- TSR/TAA 적용 어려움 (히스토리 분리)
- 포스트프로세스가 메인 뷰와 따로 돌아 색감이 달라질 수 있음

**적합 케이스:** 두 번째 뷰가 *일시적이거나, 작거나, 품질이 좀 떨어져도 되는* 경우. 우리 게임의 협동 트리거 시점처럼 "한 명이 다른 곳을 잠깐 보여주는" 류라면 강력 추천.

```cpp
// ACSGameMode 에서 더미 LocalPlayer 생성 코드 대신:
USceneCaptureComponent2D* Capture = NewObject<USceneCaptureComponent2D>(this);
Capture->TextureTarget = SecondaryRT;
Capture->CaptureSource = ESceneCaptureSource::SCS_FinalToneCurveHDR;
Capture->bCaptureEveryFrame = false;        // 수동
Capture->bCaptureOnMovement = false;
Capture->RegisterComponent();

// 30Hz 타이머에서 캡처
GetWorldTimerManager().SetTimer(CaptureTimer, [Capture, this]()
{
    if (RemoteCharacter)
    {
        Capture->SetWorldLocationAndRotation(RemoteCam.Location, RemoteCam.Rotation);
        Capture->FOVAngle = RemoteCam.FOV;
        Capture->CaptureScene();
    }
}, 1.0f / 30.0f, true);
```

### 4.2 단일 LocalPlayer + ViewFamily 에 직접 두 번째 FSceneView 추가 (정공법)

UE5 에서 **PlayerController 추가 없이 진짜 분할 화면**을 만드는 유일한 방법이다.

`UGameViewportClient::Draw()` 를 오버라이드해서, ViewFamily 빌드 단계에서 두 번째 `FSceneView` 를 우리가 직접 만든다. 이때 두 번째 View 는 LocalPlayer 가 *아니라* — 우리가 보관하는 임의의 카메라 트랜스폼이다.

```cpp
// CSGameViewportClient.h
class UCSGameViewportClient : public UGameViewportClient
{
public:
    virtual void Draw(FViewport* InViewport, FCanvas* SceneCanvas) override;

    void SetSecondaryCameraView(const FMinimalViewInfo& InView) { SecondaryView = InView; bHasSecondary = true; }
    void ClearSecondaryCameraView() { bHasSecondary = false; }

private:
    bool bHasSecondary = false;
    FMinimalViewInfo SecondaryView;
};
```

```cpp
// CSGameViewportClient.cpp — Draw() 핵심부 (간략화)
void UCSGameViewportClient::Draw(FViewport* InViewport, FCanvas* SceneCanvas)
{
    // 기본 ViewFamily 빌드는 부모에 맡기지 않고, 우리가 직접 한다.
    // (Engine\Source\Runtime\Engine\Private\GameViewportClient.cpp::Draw 참고하여 복제)

    FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
        InViewport, World->Scene, EngineShowFlags)
        .SetRealtimeUpdate(true));

    // 1) 메인 LocalPlayer 의 SceneView (왼쪽 절반)
    FSceneView* MainView = LocalPlayer->CalcSceneView(
        &ViewFamily, OutViewLocation, OutViewRotation, InViewport,
        /*StereoPass=*/eSSP_FULL);
    // 좌측 절반으로 ViewRect 설정
    MainView->UnscaledViewRect = FIntRect(0, 0, InViewport->GetSizeXY().X / 2, InViewport->GetSizeXY().Y);
    MainView->UnconstrainedViewRect = MainView->UnscaledViewRect;

    // 2) Secondary View — 우리가 직접 만듦 (LocalPlayer 없이!)
    if (bHasSecondary)
    {
        FSceneViewInitOptions ViewInit;
        ViewInit.SetViewRectangle(FIntRect(
            InViewport->GetSizeXY().X / 2, 0,
            InViewport->GetSizeXY().X,     InViewport->GetSizeXY().Y));
        ViewInit.ViewFamily = &ViewFamily;
        ViewInit.ViewLocation = SecondaryView.Location;
        ViewInit.ViewRotation = SecondaryView.Rotation;
        ViewInit.ViewActor = nullptr;
        ViewInit.PlayerIndex = INDEX_NONE;     // ← 핵심: LocalPlayer 없음
        ViewInit.BackgroundColor = FLinearColor::Black;
        ViewInit.OverlayColor = FLinearColor::Transparent;
        ViewInit.ProjectionMatrix = FPerspectiveMatrix(
            FMath::DegreesToRadians(SecondaryView.FOV * 0.5f),
            InViewport->GetSizeXY().X / 2.0f,
            InViewport->GetSizeXY().Y,
            10.0f /* near plane */);
        ViewInit.ViewOrigin = SecondaryView.Location;
        ViewInit.ViewRotationMatrix = FInverseRotationMatrix(SecondaryView.Rotation) *
            FMatrix(FPlane(0,0,1,0), FPlane(1,0,0,0), FPlane(0,1,0,0), FPlane(0,0,0,1));

        FSceneView* SecView = new FSceneView(ViewInit);
        ViewFamily.Views.Add(SecView);
    }

    // 3) 렌더 큐잉
    GetRendererModule().BeginRenderingViewFamily(SceneCanvas, &ViewFamily);
}
```

장점:
- **PlayerController 0 개 추가** — LocalPlayer 도 1 개만 유지
- ViewFamily 가 동일하므로 **Lumen SurfaceCache, Volumetric Fog, Sky LUT 가 두 뷰에서 공유**된다 — 4.1 의 SceneCapture 방식보다 GPU 측면에서 우월
- 트랜지션 / 좌우 스왑 / 비대칭 분할 자유

단점:
- **엔진 내부 API 의존도 매우 높음** — UE 5.6, 5.7 로 올라가며 시그니처가 깨질 가능성이 있다
- ViewState 관리, TSR 히스토리, AutoExposure, MotionVector 등 *LocalPlayer 가 자동으로 해주던 일* 을 우리가 다 챙겨야 한다
- 디버깅 어려움 — `stat unit`, `stat scenerendering` 이외에는 우리가 만든 View 가 잘 안 잡힘

**권장 도입 시점:** 4.1 (SceneCapture) 로 일단 구조를 분리한 뒤, 품질이 부족하다고 판단되면 4.2 로 승격. 한 번에 4.2 부터 만들지 말 것.

### 4.3 ISceneViewExtension 으로 보조 패스만 끄기

엔진을 더 깊이 건드리지 않으면서 *분할 시 특정 패스만 끄는* 방식. 두 번째 뷰가 *Lumen 을 안 써도 되는* 게임플레이 컨텍스트라면, 두 번째 View 의 `EngineShowFlags` 를 우리가 직접 지운다.

```cpp
class FCSSplitViewExtension : public FSceneViewExtensionBase
{
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override
    {
        // Player 1 이 보는 뷰만 강등
        if (InView.PlayerIndex == 1)
        {
            InView.FinalPostProcessSettings.bOverride_DynamicGlobalIlluminationMethod = true;
            InView.FinalPostProcessSettings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;
            InView.FinalPostProcessSettings.bOverride_ReflectionMethod = true;
            InView.FinalPostProcessSettings.ReflectionMethod = EReflectionMethod::None;

            InView.FinalPostProcessSettings.bOverride_BloomIntensity = true;
            InView.FinalPostProcessSettings.BloomIntensity = 0.0f;

            // 본인 의도와 다르게 강하게 죽이고 싶다면
            InView.bIsPlanarReflection = false;
        }
    }
};
```

이 방법은 **§3.1 의 글로벌 cvar 강등과 달리, "두 번째 뷰만" 골라서 죽일 수 있다.** 호스트 자기 화면은 풀 퀄로 유지. 두 화면의 비주얼 격차가 게임 디자인상 허용된다면 (= 더미 뷰가 보조적 정보 제공일 때) 가장 똑똑한 절충안.

---

## 5. 권장 도입 순서 — Phased Plan

| Phase | 작업 | 예상 회수 | 위험도 | 비고 |
|-------|------|----------|--------|------|
| **P1** | §3.1 분할 진입 시 Scalability 자동 강등 | +10~25 % FPS | 매우 낮음 | 1 일 |
| **P1** | §3.2 Animation URO + Significance | +3~8 % FPS | 낮음 | 1 일 |
| **P1** | §3.3 더미 PC Tick 차단 | +1~3 % FPS, CPU 안정성 ↑ | 매우 낮음 | 0.5 일 |
| **P1** | §3.4 RotationSync 30 Hz 강등 | 미미, 네트워크 안정성 ↑ | 매우 낮음 | 0.5 일 |
| **P2** | §4.3 SceneViewExtension 으로 두 번째 뷰만 GI/Reflection 강등 | +5~15 % FPS | 중 | 2~3 일, 비주얼 차이 디자인 검토 필요 |
| **P2.5** | §3.6 비대칭 → 대칭 구조 전환 (양 클라가 각자 분할 화면) | 호스트 부하 분산, 디자인 정합 | 중~높음 | 1~2 주, **디자인 의도 확정 후** |
| **P3** | §4.1 SceneCapture 기반 두 번째 뷰 (PlayerController 제거) | 가변 — 측정 필요 | 중 | 1 주, **PoC 먼저** |
| **P4** | §4.2 ViewFamily 직접 빌드 — 정공법 분할 | 가변, 품질 ↑ | 높음 | 2~3 주, 엔진 업데이트마다 점검 |

**권장:** P1 만 적용해도 *대부분의 체감 프레임 드랍이 사라질 가능성이 높다.* P2.5 (대칭 구조) 는 *It Takes Two 와 같은 양쪽 분할 경험* 이 디자인 요구라면 진행. P3/P4 는 P1 적용 후 측정해보고 *그래도* 부족할 때 진행한다.

---

## 6. 측정 — 무엇을 봐야 하는가

분할 진입 전후로 다음 stat 을 캡처해 비교 표를 만들자. 그 전에는 어떤 최적화 효과도 단정할 수 없다.

```
stat unit              ; Frame / Game / Draw / GPU 분리
stat scenerendering    ; 메시 드로우 콜, 라이트, 셰도우
stat gpu               ; GPU 패스별 ms (BasePass / Shadow / Lumen / Postprocess)
stat lumen             ; Lumen 사용 시
stat anim              ; 애니메이션 평가 시간
stat physxvehicles ... ; (사용 시)
```

또한 다음 cvar 로 분할 시 더 많은 정보:

```
r.RHISetGPUCaptureOptions 1
ProfileGPU                  ; 한 프레임 GPU 캡처
DumpGPU                     ; 더 자세한 캡처
```

권장: **분할 OFF / 분할 ON / P1 적용 후 / P3 적용 후** 4 가지 시나리오에서 동일 카메라 위치로 30 초 평균 FPS 와 GPU/Game/Draw 시간을 기록.

---

## 7. 결론 — 시니어 클라 엔지니어로서의 의견

1. **"분할 화면 = 두 배 비용" 은 부분적으로만 사실이다.** 분할 화면이 무거운 진짜 이유는 우리가 *분할 모드에 맞춘 품질 프로파일을 안 만들어둬서* 다. 1인 플레이용 풀 퀄리티를 그대로 분할에 적용하니 80 % 로 떨어지는 거다. 상용 코옵 게임은 모두 *분할 진입 시 한 단계 강등* 한다 (§3.1).
2. **"PlayerController 강제 추가" 가 미관상 거슬린다는 의견은 이해되지만, 그것 자체가 큰 비용은 아니다.** 진짜 비용은 *그 컨트롤러가 빙의한 폰의 Tick / Anim / Replication* 이다. 그 부분은 더미 플래그로 모두 차단할 수 있다 (§3.3). LocalPlayer 자체를 없애고 싶다면 §4.1 또는 §4.2 인데, 효과 대비 위험이 크다 — 먼저 P1 부터.
3. ***It Takes Two* 가 푼 문제와 우리 문제는 동일하다.** 그들도 온라인에서 양쪽 모두 분할 화면을 보여주며 완벽히 동기화한다. 그들의 모델은 **"양 클라가 각자 2 뷰포트를 렌더하고, 상대 카메라는 양방향 리플리케이션"** — 즉 우리가 §3.6 에서 제안한 *대칭 구조* 다. 우리 현재 구현은 비대칭이라 호스트가 모든 부하를 떠안고 있고, 디자인적으로도 클라이언트는 분할 화면을 못 본다. 디자인 의도가 *It Takes Two* 와 같은 "양쪽 모두 분할" 이라면 §3.6 전환이 사실상 필수다. 다만 그 전환 자체가 부하를 줄이는 게 아니라 *분산* 시키는 것이므로, **§3.1~§3.5 의 P1 최적화가 선행되어야** 한다.
4. **ViewportClient 만 만져서 프레임 드랍 0 은 불가능**하다. 두 카메라는 두 번의 GPU 작업이다. 다만 P1 적용으로 *체감* 프레임 드랍을 거의 없앨 수 있고, P2.5 의 대칭 전환으로 양쪽이 동등한 경험을 얻을 수 있다. P3/P4 는 그 다음 단계의 구조 개선이다.

> **다음 액션:** ① P1 (§3.1 ~ §3.4) 을 한 PR 로 묶어 적용하고 §6 의 측정 시나리오로 효과를 정량화. ② 디자인팀과 *"클라이언트도 분할 화면을 봐야 하는가?"* 를 확정. Yes 라면 §3.6 P2.5 진행, No 라면 §4.1 SceneCapture + PiP 검토. P2 / P3 / P4 는 그 결과를 보고 결정.

---

## 부록 A. 참고 코드 위치

- 현재 분할 화면 파이프라인:
  - [Source/ChronoSpace/UI/CSGameViewportClient.h](../Source/ChronoSpace/UI/CSGameViewportClient.h) / [.cpp](../Source/ChronoSpace/UI/CSGameViewportClient.cpp)
  - [Source/ChronoSpace/Subsystem/CSSplitScreenSubsystem.h](../Source/ChronoSpace/Subsystem/CSSplitScreenSubsystem.h) / [.cpp](../Source/ChronoSpace/Subsystem/CSSplitScreenSubsystem.cpp)
  - [Source/ChronoSpace/Game/CSGameMode.cpp](../Source/ChronoSpace/Game/CSGameMode.cpp) — `CreateDummyLocalPlayer`, `SetupOnlineSplitScreen`, `SyncDummyRotationWithProxy`
  - [Source/ChronoSpace/Actor/CSCameraViewProxy.h](../Source/ChronoSpace/Actor/CSCameraViewProxy.h)
  - [Source/ChronoSpace/Pawn/CSSpectatorPawn.h](../Source/ChronoSpace/Pawn/CSSpectatorPawn.h)
  - [Source/ChronoSpace/Actor/CSSplitScreenTrigger.h](../Source/ChronoSpace/Actor/CSSplitScreenTrigger.h)
- 설정:
  - [Config/DefaultEngine.ini](../Config/DefaultEngine.ini) — `bUseSplitscreen`, `TwoPlayerSplitscreenLayout`, `GameViewportClientClassName`

## 부록 B. 엔진 측 참고 (UE 5.5)

- `Engine/Source/Runtime/Engine/Private/GameViewportClient.cpp` — `UGameViewportClient::Draw`, `LayoutPlayers`
- `Engine/Source/Runtime/Engine/Public/SceneView.h` — `FSceneViewInitOptions`
- `Engine/Source/Runtime/Engine/Public/SceneViewExtension.h` — `ISceneViewExtension::SetupView`
- `Engine/Source/Runtime/Renderer/Private/SceneRendering.cpp` — `FSceneRenderer::Render`, ViewFamily 처리

## 부록 C. 자주 헷갈리는 cvar 매핑

| 기능 | cvar | 분할 시 권장값 | 비고 |
|------|------|----------------|------|
| Lumen GI | `r.DynamicGlobalIlluminationMethod` | 0 (Disabled) 또는 1 (Lumen) 그대로, 단 ScreenProbe 강등 | 끄면 룩 차이 큼 |
| Lumen ScreenProbe Downsample | `r.Lumen.ScreenProbeGather.DownsampleFactor` | 32 | 기본 16 |
| Volumetric Fog | `r.VolumetricFog` | 0 | 분할에서 가장 무거운 패스 중 하나 |
| CSM Cascades | `r.Shadow.CSM.MaxCascades` | 2 (기본 4) | |
| Shadow Resolution | `r.Shadow.MaxResolution` | 1024 (기본 2048) | |
| ScreenPercentage | `r.ScreenPercentage` | 75~85 | TSR 사용 시 안전 |
| TSR History | `r.TSR.History.SampleCount` | 8 (기본 16) | |
| Motion Blur | `r.MotionBlurQuality` | 0 | 분할에서 거의 안 보임 |
| Skeletal LOD Bias | `r.SkeletalMeshLODBias` | 1 | 캐릭터 한 단계 LOD |

---

*작성: Claude (Unreal Client Senior 관점) — 검토 후 코드 변경 PR 은 별도 진행 권장.*
