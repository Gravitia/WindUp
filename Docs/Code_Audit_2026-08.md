# ChronoSpace 소스 전수 감사 보고서 (2026-08-23)

대상: `Source/ChronoSpace` 전체 — 300개 파일, 약 28,200줄 (`.h`/`.cpp`/`.cs`).
방법: 6개 영역으로 나눠 모든 파일을 정독하고, 리슨 서버 멀티플레이 기준으로 권한·리플리케이션·수명·성능·엔진 API·로직·코드 품질을 점검했다. 엔진 동작이 판단에 걸린 항목은 UE 5.8 소스로 확인했다. 아래 Critical/High 항목은 보고 후 코드에서 다시 확인했다. **파일은 수정하지 않았다.**

---

## 1. 총평

| 구분 | 판단 |
|---|---|
| 전체 건강도 | **보통~나쁨.** 컴파일되고 호스트 단독 플레이에서는 대체로 동작하지만, 원격 클라이언트·리스폰·늦은 접속·스플릿스크린 조건에서 깨지는 코드가 광범위하다. |
| 가장 큰 구조적 문제 | "호스트에서만 된다"를 만드는 패턴이 파일 수십 개에 반복된다 — 권한 체크 없는 오버랩/Tick, 서버에서만 부르는 연출, `GetPlayerController(0)`/`GetFirstPlayerController`, static 전역으로 클라→서버 데이터 전달. |
| 확정 크래시 경로 | 6건 (`FindAndRemoveChecked`, `GetPlayerCharacter(0)` null, `NetMulticastSetGravity` null 인자, `CSSwitchBase::Data` null, `CSCharacterPatrol` 컨트롤러 null, `CSLabyrinthKeyActivator` 무한 루프). |
| 접속 끊김 경로 | 2건 (매 틱 Reliable Multicast: `CSMoveObjectSwitch`, `CSPartyPopper`). |
| 잘 된 곳 | `CSAbilityReactorBase`, `CSTransformMover`, `CSAnimatedTrap`, `CSDecoyCharacter`, `CSStagePortal`, `CSGameUIWidget`, `CSGravityAnchorItem`, `CSPlayerState`. 서버 시각 기반 동기화·OnRep 수동 호출·`TWeakObjectPtr` 집합 관리 등 올바른 패턴이 이미 프로젝트 안에 있다. 나머지를 이쪽 패턴으로 맞추면 된다. |

### 기계적 지표

| 지표 | 값 |
|---|---|
| Tick 상시 활성 클래스 | 24개 (대부분 유휴 시에도 틱) |
| `GetFirstPlayerController` / `GetPlayerController(0)` / `GetPlayerCharacter(0)` | 10곳 |
| Server RPC | 11개 중 `WithValidation` 2개, 나머지 검증 없음 |
| `CommitAbility` 호출하는 어빌리티 | 15개 중 2개 |
| 소스 파일 인코딩 | UTF-8 214 / **CP949 76** / UTF-8 BOM 7 — 혼재 |
| TODO/FIXME | 0 (대신 "삭제 예정", "will fix" 주석과 주석 처리 블록으로 남아 있음) |
| `GetAllActorsOfClass` | 0 (좋음) |
| UPROPERTY 없는 UObject 포인터 멤버 | 4건 |

---

## 2. 최우선 수정 목록

심각도 순. 번호는 이 문서 안에서의 참조용.

### Critical — 크래시 / 접속 끊김

| # | 위치 | 문제 | 실패 시나리오 |
|---|---|---|---|
| C1 | `GA/TA/CSTA_ChronoControl.cpp:107` | `ActorsInBoxTrigger.FindAndRemoveChecked`가 맵에 없는 키로 호출 (BeginOverlap이 `CustomTimeDilation==0`이면 Emplace 없이 return) | 두 플레이어의 시간정지 박스가 겹친 NPC 위에서 한쪽을 먼저 벗어나면 `check` 크래시. `CSGA_ChronoControl`이 LocalPredicted라 클라에 박스가 2개 생기므로(H1) 클라 단독으로도 발생 |
| C2 | `GA/TA/CSTA_BlackHoleSphere.cpp:158-167` | `GetPlayerCharacter(GetWorld(), 0)`으로 **항상 호스트의** 화이트홀 사용 + `Player`·`OverlapedCharacter` null 무검사 | 클라가 만든 블랙홀이 호스트 화이트홀로 텔레포트; 플레이어 0 리스폰 중이거나 `ACharacter`가 아닌 Pawn이 닿으면 크래시 |
| C3 | `Actor/CSMoveObjectSwitch.cpp:286` | `Tick → UpdateMovement → NetMulticastUpdateMovement(Reliable, TArray 2개)` 매 서버 프레임 | 이동 중 클라 hitch 시 Reliable 버퍼 오버플로 → **클라 연결 해제** |
| C4 | `Actor/CSPartyPopper.cpp:126-133, 231` | `bOneShot=false` + `LinkedButtonActor` 설정 시 거리 조건이 매 틱 참 → `Multicast_Explode`(Reliable, ~3KB) 매 프레임. `Explode()`에 진행 중 가드 없음 | 초당 60개 Reliable RPC → 클라 연결 해제. 서버도 타이머를 매 프레임 리셋해 폭죽이 영원히 안 터짐 |
| C5 | `Actor/CSGravitySwitch.cpp:49-52` | `NetMulticastSetGravity_Implementation(ACharacter* Char)`에서 `Char` null 무검사. 액터 인자는 수신 클라에 리플리케이트 안 된 액터면 null로 역직렬화 | 리스폰 직후/컬링된 캐릭터가 있을 때 스위치를 누르면 다른 클라 크래시 |
| C6 | `Actor/CSLabyrinthKeyActivator.cpp:41-63` | `length == MaxKeyCount`이고 충돌 켜진 키가 하나라도 있으면 `while` 종료 불가. `length < MaxKeyCount`면 조기 return으로 아무것도 활성화 안 됨. `RandRange(0, length)` 상한 포함 | 서버 프리즈 / 퍼즐 불가 |
| C7 | `Actor/CSSwitchBase.cpp:55` | `Data->InteractionPromptWidgetClass` — `Data` null 무검사 (같은 파일 `:95`는 검사함) | `Data` 미지정 스위치에 접근 시 전 머신 크래시 |
| C8 | `Character/CSCharacterPatrol.cpp:126,149` | `Cast<ACSAIController>(GetController())->ActiveMove()` null 무검사 | 플레이어가 트리거 안에 있을 때 Patrol이 Destroy/SetDead되면 EndOverlap에서 서버 크래시 |
| C9 | `Interface/CSProgressActivatable.h:10` | `UINTERFACE(MinimalAPI)`에 `Blueprintable` 누락. C++ 구현체 없음 | BP 액터가 인터페이스를 구현할 수 없어 `CSProgressTrigger`의 `ImplementsInterface`가 항상 false → **진행 트리거 체인 전체가 무동작** |

### High — 조용히 상태가 망가짐

**GAS / 캐릭터 수명**

| # | 위치 | 문제 |
|---|---|---|
| H1 | `Character/CSCharacterPlayer.cpp:119-129` + `ActorComponent/CSGASManagerComponent.cpp:28-56` | ASC는 PlayerState에 살지만 `PossessedBy`마다 `SetGASAbilities()` → **리스폰할 때마다 어빌리티 스펙이 한 벌씩 누적**. `ClearAbility` 호출은 프로젝트 전체에 없음. `CSMimicCharacter::CopyAbilitiesFromSource`가 중복분까지 복사 |
| H2 | `Character/CSCharacterPlayer.cpp:147-154` + `CSGASManagerComponent.cpp:58-113` | 클라에서 `OnRep_PlayerState`와 `SetupPlayerInputComponent`가 둘 다 `SetupGASInputComponent` 호출, 멱등성 없음 → **입력 이중 바인딩**, Server RPC 2발, `Ability.Movement` 루즈 태그 카운트 2. 또 `GetPlayerState<ACSPlayerState>()` null 무검사(언포제스 시 null로 OnRep 호출됨) |
| H3 | `CSGASManagerComponent.cpp:87-91` | `ScaleLargeAction` Triggered/Completed 바인딩이 코드에 그대로 두 번 (복붙) |
| H4 | `Game/CSGameMode.cpp:199-296` + `Game/CSGameState.cpp:117-133` | `RespawnSinglePlayer`가 구 Pawn 제거/새 Pawn 등록을 안 해 `FindPlayerDeathState(NewPawn)` null → **첫 사망 이후 사망/부활 추적 영구 붕괴** (`OnPlayerRevived` 미발송, `GetDeadPlayerCount` 고정). `CheckAllPlayersDeath/TriggerAllPlayersRespawn`은 선언만 있고 정의 없음 |
| H5 | `Character/CSCharacterPlayer.cpp:156-173` | `Landed()` 본문 전체(Super 포함)가 주석 처리 → BP `OnLanded`/`LandedDelegate`가 영원히 발생 안 함 |
| H6 | `Actor/System/CSKillZone.cpp:78-98` | 오버랩에 `HasAuthority()` 없음 → 클라 로컬에서도 `SetDead/SetRevive` 실행. 클라 `SetRevive` 후 서버 텔레포트가 RTT만큼 늦으면 재오버랩 → 이중 사망. `bIsActive` 비복제. 부활 전 중복 오버랩 시 타이머 2개 |
| H7 | `GA/CSGA_ChronoControl.cpp:13-16` | `NetExecutionPolicy` 미지정(=LocalPredicted). 형제 박스 어빌리티는 전부 ServerOnly. 원격 클라: 예측 스폰 + 서버 스폰 복제 → **클라 월드에 박스 2개** (C1의 원인) |
| H8 | `GA/CSGA_TimeRewind.cpp:13-23, 59-125` | 정책 미지정(LocalPredicted) → 서버·클라 양쪽에서 `SetActorLocation`으로 캐릭터를 끌고 감. `DisableInput`/`GravityScale=0` 복구가 태스크 완료 델리게이트에만 있어 취소·사망 시 **입력 비활성+중력 0 영구 고착** |
| H9 | `GA/CSGA_ProjectileBlackHole.cpp:180-196` | `EndAbility`가 카메라 Lerp 중이면 `Super::EndAbility` 없이 return → retrigger 경로에서 `ActiveCount` 누수, `Spec->IsActive()` 영구 true, `ClearAbility` 불가 |
| H10 | `GA/TA/CSTA_WeakenGravityBox.h:41-42` | `GravityCoef`가 `Replicated`가 아님. 서버는 0.5, 클라 복제본은 기본 0.1로 `GravityScale` 적용 → 5배 차이로 매 프레임 러버밴딩 |
| H11 | `GA/CSGA_AbilityPreviewBox.cpp:48-60` ↔ `GA/TA/CSTA_BoxTrigger.cpp:15-24` | 클라가 **DataAsset 메모리 값을 수정 + `MarkPackageDirty`** 해서 박스 크기 전달. 서버는 자기 프로세스의 DataAsset을 읽음 → 원격 클라 선택 무시. PIE에서 프로젝트 에셋이 더티됨 |
| H12 | `GA/CSGA_BlackHole.h:38-43` | `static FVector PendingTargetLocation`으로 LocalOnly GA(클라)→ServerOnly GA(서버) 전달 → 호스트만 동작 |
| H13 | `GA/AT/CSAT_AbilityPreviewBox.cpp:67,73,80,197,202` | `GetFirstPlayerController()->GetPawn()`에 부착(스플릿 2P는 1P 폰에 붙음), `PlayerController` weak ptr `->` 무검사 |
| H14 | `Attribute/CSAttributeSet.cpp:43-75` | 메타 어트리뷰트(Damage/Healing/ClockUnwind) 소비 후 0으로 안 되돌림. GE가 `Add`면 누적 (GE 에셋은 바이너리라 미확인 — `Override`면 현재는 잠복) |

**액터 / 컴포넌트**

| # | 위치 | 문제 |
|---|---|---|
| H15 | `Actor/CSBlackHole.cpp:195-254` | 오버랩 콜백에 `HasAuthority()` 없음 → 클라에서도 중력 off/카메라 채널 변경. `GravityInfluenceRange`·스피어 반경 비복제 → 클라는 기본 500으로 판정 |
| H16 | `Actor/CSBlackHole.cpp:207-212, 235-240` | **모든** `UStaticMeshComponent`(바닥·벽 포함)에 `ECC_Camera` Ignore, 나갈 때 원래 값 무관하게 `Block` 고정 → 카메라가 벽 뚫음 / 장식물에 걸림 |
| H17 | `Actor/CSBlackHole.cpp:292-320` | 원격 클라 캐릭터 CMC에 서버만 `AddImpulse`/`StopMovementImmediately` → 자율 프록시 예측 불일치, 매 틱 보정 떨림 |
| H18 | `Actor/CSLabyrinthKey.cpp:127-130` | `GetLifetimeReplicatedProps`에 `Super::` 없음 → AActor 기본 복제 + **BP 자식의 Replicated 변수 전부 복제 안 됨** |
| H19 | `Actor/CSCameraZoomVolume`, `CSFixedCameraVolume`, `CSSplitScreenTrigger` (3개 동일 패턴) | `Cast<APlayerController>(Character->GetController())`가 클라에선 원격 플레이어에 대해 null, 호스트에선 `GetLocalPlayer()` null → 원격 플레이어 진입 시 **호스트 자신이 풀스크린**. 게다가 `UCSSplitScreenSubsystem::TransitionToFullScreen(int32)`는 인자를 무시 → `bFullScreenForEnteringPlayer` 등 옵션 전부 죽은 옵션. `CSFixedCameraVolume`은 EndOverlap에서 컨트롤러 null이면 `SetIgnoreLookInput(true)`가 영구 잔류 |
| H20 | `CSSwitchBase`, `CSTransformMover`, `CSGridBoard`, `CSGridLever`, `CSMoveObjectSwitch`, `CSBellows::bLinkedMoved`, `CSPartyPopper::bHasFired` | 상태를 Multicast RPC로만 전파 → **늦은 접속/재접속 클라는 기본 상태** (문 닫힘, 스위치 OFF, 타일 기본). `CSTransformMover`는 `bAlwaysRelevant`를 켰지만 복제 프로퍼티가 없어 무의미 |
| H21 | `Actor/CSMoveObjectSwitch.cpp:60-93, 167-173` | 이동 중 재상호작용 시 `bIsInteracted` 토글은 되고 `StartMovement`는 return → 표시와 실제 위치가 영구히 어긋남. `SetInteracted()`가 같은 로직을 비가상으로 재구현 |
| H22 | `Actor/CSRotatingActor.cpp:27-36` | `bReplicates`만 켜고 각 머신이 로컬 DeltaTime으로 회전 → 서버/클라 각도 발산, 회전 플랫폼 위 클라 캐릭터 밀려 떨어짐 |
| H23 | `Actor/CSReverseGravityField.cpp:139-141` | MID `Tiling`을 "현재값 × Scale"로 0.25초마다 덮어씀 → 지수 폭주, 수 초 후 텍스처 뭉개짐 |
| H24 | `Actor/CSClawMachine.h:79` vs `.cpp:101,110` | `Destination`이 `MakeEditWidget`(액터 로컬 좌표)인데 코드는 월드 좌표로 사용 → 위젯으로 찍으면 엉뚱한 곳으로 |
| H25 | `Actor/CSProjectileGuideActor.cpp:79-145` | 호출마다 세그먼트 컴포넌트 N개 Destroy + NewObject + Register (조준 중 매 프레임) → GC 압박·스파이크. 세그먼트 간격/길이 수식도 불일치 |
| H26 | `ActorComponent/CSKillZoneResetComponent.cpp:68-87` | 속도/각속도 0 리셋이 `IsA(ACSKillZone)` 블록 **밖** → 블랙홀 스피어·플레이어 트리거 등 아무 오버랩에서나 물리 프롭 정지 |
| H27 | `ActorComponent/CSCustomGravityDirComponent.cpp:12,33` + `Actor/CSGravitySwitch.cpp:31,36` | static `OrgGravityDirection`을 `BeginPlay`마다 (0,0,-1)로 리셋 → 중력 스위치 후 누가 리스폰/미믹 스폰하면 전역 중력이 되돌아감. 서버에서만 갱신되어 클라 값은 항상 (0,0,-1) |
| H28 | `CSCustomGravityDirComponent.cpp:56-66` | 클라 보간 `VInterpTo(...).GetSafeNormal()`: 정반대 방향(천장→바닥)이면 정규화 후 원래 값 → **한 틱도 진행 안 함**; DT×Speed=0.5면 영벡터 → `SetGravityDirection(0)` ensure. 서버는 즉시, 클라는 보간 → 자율 프록시 예측 불일치 |
| H29 | `ActorComponent/CSButtonIndicatorComponent.cpp:34-38` + `CSGravityCoreSphere.cpp:134,157` | 버튼 점등 연출이 서버에서만 브로드캐스트되는 로컬 델리게이트를 구독 → 클라 화면에서 버튼 안 켜짐 |
| H30 | `ActorComponent/CSTransformRecordComponent.h:37` vs `GA/CSGA_TimeRewind.cpp:51,81` | `MaxHistorySize`(EditAnywhere)와 별개로 GA가 `99` 하드코딩 → 50으로 낮추면 되감기 영원히 발동 안 함. 되감기 중에도 기록 계속 → 연속 사용 시 앞으로 튐 |

**서브시스템 / UI / 렌더**

| # | 위치 | 문제 |
|---|---|---|
| H31 | `UI/CSViewFamilyViewportClient.cpp:197-348` | 분할 화면 중 `Super::Draw`를 우회하면서 엔진이 매 프레임 하던 **오디오 리스너 갱신, 텍스처 스트리밍 뷰 등록, SceneViewExtension, `LastRenderTime`** 등이 전부 빠짐 → 분할 중 3D 사운드 고정, 텍스처 밉 안 올라옴. `CalcSceneView` null 미검사 + Views 0개로 `BeginRenderingViewFamily` 가능. HUD 캔버스에 SceneView 미설정 → `Project` 전부 (0,0). `ULocalPlayer::Origin/Size`를 Draw 중에만 덮어써 `ProjectWorldLocationToScreen`/`DeprojectMouse` 불일치 |
| H32 | `Subsystem/CSEIKSubsystem.cpp:13-19, 164, 249-273` | `Initialize` 첫 줄이 `return;`(Super 도달 불가 — 사실상 비활성 서브시스템). `OnJoinSessionComplete`가 `Result` 확인 전에 `ClientTravel`. `IOnlineSubsystem::Get("EIK")` null 무검사 4곳. 존재하지 않는 맵 `L_StageSize` 하드코딩 ServerTravel |
| H33 | `Subsystem/CSStageGameInstanceSubsystem.cpp:37-46` | `BlueprintCallable ChangeStage`가 `UGameplayStatics::OpenLevel` → 리슨 서버에서 호출 시 클라 전원 접속 해제. 키 `L_Stage1/2`·`L_Main`은 실제 맵 이름과 불일치(유물 코드) |

---

## 3. 전체를 관통하는 패턴

수정은 아래 패턴 단위로 묶어서 해야 효율이 난다. 개별 버그 30개를 따로 고치면 같은 문제가 다른 파일에서 다시 나온다.

### P1. "호스트만 된다" 비대칭 (가장 많음)
- **원격 플레이어의 `APlayerController`는 클라에 존재하지 않는다.** `Cast<APlayerController>(Pawn->GetController())`, `IsPlayerControlled()`, `GetLocalPlayer()`를 권한 체크 없이 쓰는 코드(`CSCameraZoomVolume`, `CSFixedCameraVolume`, `CSSplitScreenTrigger`, `CSTextTrigger`, `CSKillZone`)는 전부 호스트/클라가 다르게 동작한다.
- **`GetPlayerController(0)` / `GetFirstPlayerController` / `GetPlayerCharacter(0)`** 10곳 (`CSTA_BlackHoleSphere`, `CSAT_AbilityPreviewBox`, `CSCameraViewProxy`, `CSStagePortal`, `CSEIKSubsystem`). 서버에서 0번은 항상 호스트다.
- **연출을 ServerOnly 경로에서 실행**: `CSGA_GravityCore`(사운드), `CSGA_WindUp`(파티클·사운드·몽타주), `CSDecoyCharacter`(BIE 훅), `CSButtonIndicator`(서버 델리게이트 구독). 호스트만 보고 듣는다.
- **static/전역/DataAsset 메모리로 클라→서버 데이터 전달**: `CSGA_BlackHole::PendingTargetLocation`, `CSGA_AbilityPreviewBox`의 DataAsset 수정, `CSCustomGravityDirComponent::OrgGravityDirection`. 프로세스 경계를 넘지 못한다. GAS 타겟 데이터 또는 복제 프로퍼티로 보내야 한다.
- 규칙: 서버 로직은 `HasAuthority()` 뒤에서, 로컬 연출은 `IsLocallyControlled()` 뒤에서만. 연출 트리거는 `ReplicatedUsing` 또는 Multicast로 내려보낸다.

### P2. 권한 체크 없는 오버랩/Tick에서 상태 변경
`CSKillZone`, `CSBlackHole`, `CSTA_ReverseGravityBox/WeakenGravityBox/ChronoControl/BlackHoleSphere`, `CSPushingCharacterComponent`, `CSSwitchBase::Interact`. 서버·클라 복제본이 각자 `GravityScale`·`CustomTimeDilation`·임펄스·`SetDead`를 적용하는 "우연한 이중 시뮬레이션"이다. 복제되지 않는 파라미터(H10)나 복제 지연이 끼면 즉시 어긋난다. `04-multiplayer.md` 규칙의 직접 위반.

### P3. Pawn 수명과 PlayerState 수명 혼동
ASC·어빌리티는 PlayerState에, 사망 추적 키는 Pawn에. 리스폰 한 번이면 어빌리티 누적(H1)과 사망 추적 붕괴(H4)가 동시에 일어난다. `SetASC`/`SetGASAbilities`/`SetupGASInputComponent` 모두 "이미 했는지" 검사가 없다.

### P4. 상태를 Multicast RPC로만 전파 → 늦은 접속 미대응
H20 목록. 리슨 호스트가 OnRep을 안 받는 걸 피하려 RPC를 택한 것으로 보이지만, 올바른 해법은 `ReplicatedUsing` + 서버에서 값 변경 직후 OnRep 수동 호출이다 — `CSLabyrinthKey::SetActive`, `CSAbilityReactorBase`가 정확히 이렇게 하고 있다. 지금은 로비→ServerTravel 흐름이라 숨어 있지만 재접속 기능을 넣는 순간 전부 터진다.

### P5. 움직이는 오브젝트 동기화 방식이 5가지
| 방식 | 파일 | 평가 |
|---|---|---|
| 서버 시각 기반 결정론 재생 | `CSAnimatedTrap`, `CSTransformMover` | **이게 정답** |
| 매 틱 Reliable Multicast | `CSMoveObjectSwitch` | 접속 끊김 (C3) |
| 매 틱 float 프로퍼티 복제 + 클라 보간 | `CSConveyorManager`, `CSSplineRider` | 스터터, 초기값 문제 |
| 로컬 DeltaTime 각자 회전 | `CSRotatingActor` | 발산 (H22) |
| `SetReplicatingMovement` + 서버 Tick `SetActorLocation` | `CSGridBox` | 계단식 이동 |

"A→B 이동" 오브젝트가 `CSMoveObjectSwitch`와 `CSTransformMover` 둘, "경로 따라 이동"이 `CSConveyorManager+Platform`, `CSTrackManager+MovingPlatform`, `CSSplineTrack+Rider` 셋이다. 전자는 `CSTransformMover`로, 후자는 하나로 통일.

### P6. GAS 기본 위생
- `CommitAbility` 없음 13/15 — BP에서 Cost/Cooldown GE를 걸어도 조용히 무시된다.
- `NetExecutionPolicy` 미지정 2건이 가장 큰 멀티 버그(H7, H8)를 만든다. 생성자에서 정책 명시를 규칙으로.
- "실패하면 return"에 `EndAbility` 없음 (`CSGA_BlackHole:60-64`, `CSGA_GravityCore:48-49`, DurationTask 자식 4개의 null 분기) → InstancedPerActor 고착.
- 입력을 GAS 밖에서 직접 읽음: `IsInputKeyDown(LeftMouseButton/RightMouseButton)` 폴링 3곳, 레거시 `BindAction("WheelUp")`. Enhanced Input·게임패드·리매핑 전부 우회.
- AT/TA 트리오 14파일 1,196줄 중 **65~75%가 복붙**. `UCSAT_DurationTask`·`ACSTA_BoxTrigger` 베이스가 있는데 공통 로직이 베이스에 없다. 공통화하면 "한 형제에만 있는 버그"(C1, H10)가 자연히 사라진다.

### P7. 엔진 가상 함수를 대체하면서 Super의 부수 효과를 재현하지 않음
`CSViewFamilyViewportClient::Draw`(H31), `CSLabyrinthKey::GetLifetimeReplicatedProps`(H18), `CSCharacterPlayer::Landed`(H5), `CSGravityCoreSphere::Tick`(BP Event Tick 안 돎), `CSEIKSubsystem::Initialize`(도달 불가), `CSUserSettings::SetToDefaults`(미오버라이드).

### P8. null/캐스트 무검사 + `check()`로 콘텐츠 누락 처리
`Cast<>()->` 즉시 역참조 12곳 이상. `check(AbilityClass)`, `check(DamageEffect)`, `check(GravityCoreClass)`, `check(BlackHoleDummyClass)`(쓰지도 않음), `check(GiveDamageAbility)`, `check(BBAsset && BTAsset)`, `check(OptionWidgetClass)` — BP 하나만 비어도 PIE가 통째로 죽는다. `ensureMsgf` + 조기 반환으로.

### P9. 트리거/프롬프트/리액터 템플릿 다중 복제
- 트리거 액터 5종(`CameraZoomVolume`, `FixedCameraVolume`, `SplitScreenTrigger`, `TextTrigger`, `LevelTransferTrigger`)이 Box + Begin/End + 카운트 + `IsPlayer()`를 각자 구현. H19를 세 곳에서 고쳐야 한다.
- 상호작용 프롬프트 위젯 코드 4중 복제(`SwitchBase`, `LabyrinthKey`, `LabyrinthKeyAltar`, `GridLever`). `UCSPlayerInteractionComponent`가 로컬 여부를 안 가려 **원격 플레이어가 접근해도 내 화면에 프롬프트가 뜬다**.
- `IsAbilitySourceOfInterest`(Reactor/Rider), `NotifyTargets`+경고 루프(Reactor/Group/Track), `EvaluateTrigger`/`EvaluateGroup`(Reactor/Group) 복제.
- 오버랩 카운트 방식 3가지: refcount(`ReactorBase`, 정답), `TSet`(블랙홀의 동심 스피어 2개 앞에서 깨짐 — `CSSplineRider`), 없음(`MeasuringTape`, `ClawMachine`, 트리거 3종 드리프트).

### P10. 죽은 코드가 `BlueprintCallable`로 노출되어 "언제든 터질 수 있는 코드"
`CSGA_ProjectileGuide`(스스로 "삭제 예정"), `CSGA_CharacterScale` 후반 90줄, `CSStageGameInstanceSubsystem`(OpenLevel), `CSEIKSubsystem`(없는 맵), `CSMapSelectionWidget`(`/Game/Maps` 불일치, 가드 없음), `CSAbilityHUD`(항상 빈 목록), GAS 위젯 체인 3개(본문 주석), `SCSServerTravelWidget`, `CSReactorGroup`(사용 중단 예정), `CSGA_CameraZoom`(종료 경로 없음), `AlwaysClockUnwind` 체인, 빈 Zoom RPC 2개, `FirstPersonCamera`, `CheckAllPlayersDeath/TriggerAllPlayersRespawn`(선언만), 빈 `Interface/*.cpp` 5개, `CSCommon.h`, `CSOnTriggerEffectData`.

### P11. 파일 인코딩 혼재 (CP949 76개)
한글 주석이 mojibake로 깨져 있고 `CSAbilityHUD.cpp:76`처럼 `TEXT("...")` 리터럴 안의 한글은 **런타임에 깨진 채 출력**된다. MSVC C4819 경고의 원인. 일부 UTF-8 파일(`CSPlayerState.h`, `CSGASManagerComponent.cpp`)은 이미 깨진 문자열이 저장되어 복구 불가. `.editorconfig`로 UTF-8(BOM) 고정 후 일괄 변환, 깨진 주석은 다시 쓴다. 목록은 부록 B.

### P12. 기타 반복
- `SetIsReplicated(true)`/`SetIsReplicatedByDefault(true)` 남발: 복제 프로퍼티도 RPC도 없는 컴포넌트 9개 중 4개, 정적 서브오브젝트 다수 → 대역폭만 소비.
- `bAlwaysRelevant` 불일치: 상태 복제로 멀리 있는 타겟을 움직이는 `CSAbilityReactorBase`·`CSSplineTrack`·`CSSplineRider`·`CSBellows`에는 없고, 폐기 예정 `CSReactorGroup`에만 있음 → 150m 밖 기믹이 클라에서 반응 안 함.
- 고정 dt: `0.016f` 타이머 + `Elapsed += 0.016f` 5곳 → 프레임레이트 따라 지속시간이 바뀜.
- 생성자에서 런타임 작업: `ConstructorHelpers` 경로 하드코딩 7곳, `SetActorLabel`/`SetActive`/`StaticLoadObject`/`AddDynamic`/`SetReplicates(true)`.
- 로그: `LogCS`/`LogTemp` 혼용, 정상 흐름을 `Warning`으로, `BeginPlay`마다 디버그 로그, 매 프레임 Log.
- 매직 넘버가 DataAsset/EditAnywhere와 코드에 이중 존재(되감기 99, 줌 1200/600, 캡슐 34/88, 스플릿 75%/25%).
- `CSCollision.h`가 ini와 불일치: `CCHANNEL_CSSPECTATOR`=`ECC_Camera`(ini는 GameTraceChannel3), `CPROFILE_GRAVITY_CORE`=`"OverlapAll"`(ini의 `CSGravityCore` 미사용). 콜리전 디버깅 시 제일 먼저 사람을 속이는 파일.
- `Build.cs`: `GameplayTasks` 중복, `AIModule` 미선언(전이 의존으로 우연히 컴파일), 템플릿 주석 잔존.

---

## 4. 영역별 상세

### 4.1 Character / Game / Player / BT / Animation

| 파일 | 상태 | 주요 문제 |
|---|---|---|
| `CSCharacterPlayer` | **문제 집중** | H1, H2, H5. `ServerSpawnAndSetBlackHole` 등 Server RPC 4개가 클라 파라미터를 검증 없이 스폰/적용하고 이전 `BlackHole`을 파괴 없이 덮어씀. `BeginPlay`에서 매핑 컨텍스트 추가(클라에서 `Controller` 미매핑 시 누락 → "가끔 조작 안 됨"; AI 빙의 시 `CastChecked` assert). 코요테 점프 `bCanCoyoteJump` 비복제로 서버/클라 창 불일치. `Multicast_ApplyClockUnwind`가 모든 머신에서 GE 적용(현재 주석). SpringArm/카메라 2개가 시뮬 프록시·미믹·디코이에도 틱(스윕). `FirstPersonCamera` 미사용. `SetDead/SetRevive`는 호출 머신에서만 바뀌고 복제 계약 없음. god-class (카메라·입력·GAS·화이트홀·스케일·UI·RPC·사운드·킬존 플래그). |
| `CSCharacterBase` | 단순 | `SetDead/SetRevive` 권한·복제 없음(`BlueprintCallable`). 40줄 주석 블록 `AttachWindUpKeyToSocket`. |
| `CSCharacterPatrol` | 불량 | C8. 오버랩 카운트 로직 역전(`--Count; if (Count>0) {Count=0; 충돌 껐다 켬}`) — Begin 없이 End만 오면 음수, 남은 플레이어 있어도 순찰 재개. `check(GiveDamageAbility)`. 생성자 `AddDynamic`. `int8` 인덱스. `Player`·`AttributeSet` 미사용. |
| `CSMimicCharacter` | 대체로 건전 | 루즈 태그 `Ability.Movement` 비복제(그리고 `BlockAbilitiesWithTag`와 무관해 효과 없음). `SourceCharacter` weak ptr 무검사. `RemoveAt(0)` 매 틱. H1 중복분 복사. |
| `CSDecoyCharacter` | 가장 깨끗 | BIE 훅 3개가 서버에서만 호출 → 원격 클라는 태엽 연출 못 봄. `WindUpRequiredTime` 0이면 inf. |
| `CSGameMode` | 보통 | H4. 스폰 실패 시 구 Pawn 이미 파괴·재시도 없음. 리스폰 회전 설정 3중 적용. `ConnectedPlayers` UPROPERTY 없음. PIE 폴백 키 `remote:0`이 원격 2명이면 같은 슬롯. `HandleSeamlessTravelPlayer`는 seamless가 꺼져 있어 미실행 경로. `Logout` 시 Pawn null이면 사망 추적 제거 건너뜀. |
| `CSGameState` | 보통 | H4. `CheckAllPlayersDeath/TriggerAllPlayersRespawn` 선언만, `AllDeadRespawnTimer` SetTimer 없음 — "전원 사망 리스폰" 미완. 생성자 `SetReplicates(true)`. |
| `CSPlayerController` | 보통 | `ServerRequestStagePortalTravel` 검증 없음(어느 클라든 임의 스테이지 ServerTravel 강제 가능). `OnPossess`에서만 UI 갱신 → 클라 리스폰 시 HUD 갱신 안 됨(`RequestUIRefresh` BP 우회가 증거). UI 생성을 `0.2f` 타이머로 지연(레이스 회피 하드코딩). 레거시 `BindAction("ToggleOption")`. `AddOnScreenDebugMessage` 3곳. 빈 Zoom RPC 2개. |
| `CSPlayerState` | 표준적 | `SetPlayerSlot`이 권한 체크 없고 호스트에서 OnRep 수동 호출 안 함(잠복). |
| `CSAIController`, `BT/*` | 단순 | `check(BBAsset && BTAsset)`. `Cast<ACSAIController>(GetAIOwner())->` 무검사. `Result` 미사용. |
| `CSAnimInstance` | 문제 | `Velocity.Z` Z-up 고정 가정 — 커스텀 중력 컴포넌트를 캐싱까지 하면서 속도 분해엔 안 씀 → 중력 반전 시 점프 애님 안 나옴. `bIsFalling & (...)` 비트 AND. |

### 4.2 GA / AT / TA

| 파일 | 상태 | 주요 문제 |
|---|---|---|
| `CSGA_AbilityPreviewBox` | 불량 | H11. `check(AbilityClass)`. ChronoControl을 `TryActivateAbility`로 켜면 H7 이중 스폰. `DurationTime` 죽은 설정. |
| `CSGA_BlackHole` | 불량 | H12. `TargetActorClass` null이면 EndAbility 없이 return → 고착. |
| `CSGA_CameraZoom` | 죽은 코드 | 종료 경로 없음(Active 영구). 호출자 전부 주석. |
| `CSGA_CharacterScale` | 반쯤 죽음 | 후반 90줄(`ChangeCharacterScale`/`UpdateScaleTransition`/타이머) 미호출. 0.016 고정 dt. 캡슐 34/88 매직(`CSCharacterPlayer::BaseCapsule*`과 중복). `CurrentScaleType` 미초기화. |
| `CSGA_ChronoControl` | 불량 | H7. `UCSGA_ChronoControl(float)` 죽은 생성자. |
| `CSGA_GiveDamage` | 양호 | `check(DamageEffect)`. |
| `CSGA_GravityCore` | 보통 | `OwnerCharacter` null → EndAbility 없이 return. `NetMulticastMakeGravityCoreSphere`가 `if (GravityCore)` 밖 → null 역참조. 사운드 서버 전용. |
| `CSGA_ProjectileBlackHole` | 불량 | H9. `check(BlackHoleDummyClass)`인데 스폰 코드 없음. 토글이 호스트(`InputPressed`)/클라(retrigger) 두 경로로 갈려 부수효과 다름. 스플릿 75%/25% 하드코딩(풀스크린 클라는 가로 75% 지점 조준). `IsInputKeyDown(RMB)` 0.02초 폴링 + 라인 트레이스 50회/초. `DurationTimer`/`CheckMouseMovement`/`CameraZoomAbilityClass` 죽은 코드. `MouseYSensitivity` 헤더 1.5 vs 생성자 3.0. |
| `CSGA_ProjectileGuide` | 삭제 대상 | 파일 머리에 "레거시, 삭제 예정". static 전달·`IsInputKeyDown(LMB)`·매 틱 `DrawDebugSphere`. BP에 부여되면 그대로 동작. |
| `CSGA_ReverseGravity` / `CSGA_WeakenGravity` | 구조 양호 | 서로 diff 5줄. Commit 없음. |
| `CSGA_Sprint` | 가장 양호 | `DashSpeed/WalkSpeed` UPROPERTY가 있지만 캐릭터 값 사용(효과 없음). `EndAbility`가 `MaxWalkSpeed` 무조건 복원. |
| `CSGA_TimeRewind` | 불량 | H8. `Cast<ACharacter>` null 무검사 3곳. `BlockAbilitiesWithTag("Ability.Movement")`가 의도(이동 차단)와 무관. |
| `CSGA_WhiteHall` | 양호 | `Cast<ACSCharacterPlayer>` 무검사. |
| `CSGA_WindUp` | 로직 양호 | 파티클·사운드·`Montage_Play` 서버 전용(복제 안 됨). `CanActivateAbility`에서 `const_cast`로 오버랩 쿼리. Cascade `UParticleSystem`(deprecated). |
| `AT/CSAT_DurationTask` | 껍데기 | 빈 가상 함수만. 자식 4개가 스폰→타이머→Confirm→Destroy를 각자 복사. `SetDurtionTime` 오타. `EndTimer` 미정리. |
| `AT/CSAT_AbilityPreviewBox` | 불량 | H13. `AllowedSizes[INDEX_NONE]` OOB. PC InputComponent에 `BindAction` 매 활성화마다 추가·미제거. 매 틱 `TArray` 힙 할당 + 미사용 `YOffset`. `StaticLoadObject` 4종 런타임 동기 로드. `SetSteticMeshMaterial` 오타·복붙. |
| `AT/CSAT_BlackHoleSphere/ChronoControl/ReverseGravityBox/WeakenGravityBox` | 복붙 | `SpawnedTargetActor` null이면 `SetWaitingOnAvatar` 영구 대기 → 어빌리티 종료 불가. `SpawnedTargetActors.Add` 후 미제거. `DisplayName="ReverseGravity"` 5개 복붙. ChronoControl만 `SetOwner` 누락. |
| `AT/CSAT_TimeRewind` | 불량 | `AActor* TargetActor` **UPROPERTY 없음**(GC 댕글링). 되감기 수학: 5ms 타이머는 프레임당 1회라 0.5초가 아니라 1.65초, `VInterpTo`로 일부만 이동 후 인덱스 감소 → 궤적 지연. `Super::Activate` 미호출. |
| `TA/CSTA_BoxTrigger` | 보통 | 생성자에서 `StaticLoadObject`(CDO 시점 실행). 생성자에서 `UMaterialInstanceDynamic::Create`(CDO에 MID). 공통 로직 미흡수. |
| `TA/CSTA_BlackHoleSphere` | 불량 | C2. Tick `AddImpulse` 권한 검사 없음(클라 복제본도 임펄스). `Power(10000)`·`PullStrength 50` 매직. |
| `TA/CSTA_ChronoControl` | 불량 | C1. `EndPlay`가 `CreateIterator` 순회 중 `FindAndRemoveChecked`로 같은 맵 수정. `CustomTimeDilation` float 동치 비교. |
| `TA/CSTA_ReverseGravityBox` | 보통 | 겹친 박스: 하나 벗어나면 나머지 안에 있어도 중력 복귀. 클라 복제본도 `GravityScale` 뒤집음. `AddImpulse(0,0,0.1)` 매직. |
| `TA/CSTA_WeakenGravityBox` | 불량 | H10. `DynMaterial` null 무검사. `BeginPlay`에서 Reliable 멀티캐스트(늦은 접속 미대응). |
| `TA/CSTA_MultiTrace` | 양호 | `CastChecked<ACharacter>`. `bShowDebug=true` 기본 → 공격마다 5초 디버그 캡슐. |

### 4.3 ActorComponent / Attribute / Interface / Common / Physics / DataAsset / Build

| 파일 | 상태 | 주요 문제 |
|---|---|---|
| `CSGASManagerComponent` | 핵심 결함 | H1, H2, H3. `if (UE_BUILD_SHIPPING)` 런타임 if. 쉬핑 밴 목록을 `IsLocallyControlled()`로 고름(이제 `ECSPlayerSlot`이 있으니 슬롯 기준으로). `ReplicationMode` 직접 대입. 입력이 전부 Server RPC → 클라 예측 없음·RTT 지연; `ETriggerEvent::Triggered` 바인딩이라 IA에 Pressed 트리거 없으면 키 누르는 동안 매 프레임 Reliable RPC(`if (Spec->InputPressed) return;` 가드의 존재 자체가 흔적). |
| `CSCustomGravityDirComponent` | 가장 문제 | H27, H28. `OwnerCharacter` null 무검사(BP로 비-Character에 붙이면 첫 틱 크래시). `SetComponentTickEnabled` 토글 주석 처리 → 모든 캐릭터 항상 틱. `Core->Owner` 직접 접근. `IsGravityCustomzied` 오타가 AnimInstance까지 전파. |
| `CSKillZoneResetComponent` | 불량 | H26. 매 BeginPlay `NewObject<UBoxComponent>` 고정 이름. 박스 50 고정. `bIgnoreKillZone`이 `CSCharacterPlayer`와 중복. |
| `CSButtonIndicatorComponent` | 불량 | H29. `FButtonMeshCache::OriginalMaterial`이 비-UPROPERTY 배열 안 → 런타임 MID면 GC 후 댕글링. |
| `CSCharacterScaleComponent` | 보통 | Multicast + RepNotify 이중 적용 → 클라는 스냅, 서버만 부드러움. `ScaleTransitionSpeed` 0이면 inf → 타이머 영구. `RequestScaleChange` if/else 양 분기 동일. 빈 `TickComponent`. `AddOnScreenDebugMessage` 상시. `GA/CSGA_CharacterScale.h` include(enum 때문). |
| `CSCameraZoomComponent` | 보통 | 조기 종료 조건 부호 반대(`Org + Target` vs 적용은 `Org - Target`) → 음수 줌 시 고착. no-op Server RPC 호출. `1200/600` 매직 + `// will fix`. 매 프레임 Log. |
| `CSPushingCharacterComponent` | 죽은 기능 | 권한 없이 매 틱 `AddForce`. Begin 필터가 `ACSCharacterPlayer`·`ACSCharacterPatrol`을 **제외**하므로 프로젝트 캐릭터는 전부 걸러짐. `Strength=1000000` 매직. |
| `CSPlayerInteractionComponent` | 보통 | 인터페이스 캐스트 결과 무검사 `Interact()`. `CreateIterator` 순회 중 `Remove(Key)`. 오버랩 추적이 머신별 로컬이라 클라 프롬프트 대상과 서버 실행 대상이 다를 수 있음. 로컬 여부 안 가려 원격 플레이어 접근에도 프롬프트. |
| `CSTransformRecordComponent` | 보통 | H30. `bCanEverTick=true`인데 `TickComponent` 없음(빈 틱). 모든 머신·시뮬 프록시에서 33Hz 기록. `RemoveAt(0)` 매 기록. |
| `CSMeshAffectedByGravityCore` | 보통 | `BeginPlay`에서 `Owner->SetReplicates(true)` — 클라에선 경고만 찍고 무효. `bEnable` 비복제인데 `BlueprintCallable`. `CSMeshPulledByBlackhole`과 역할 같은데 네트워크 전략 다름. |
| `CSActorAttachmentComponent` | 양호 | `SetIsReplicatedByDefault(true)`인데 슬롯 상태 비복제 → 클라 `IsSlotOccupied` 항상 false. 클라 복제본은 부착된 채 물리 계속. |
| `CSVFXComponent` | 보통 | `check(Contains) + operator[]` → 쉬핑 UB. `Attacked`/`Attached` 오타. `NONE`만 있는 enum. |
| `CSCharacterPulledByBlackhole`, `CSCharacterPushedComponent` | 빈 마커 | 하나의 컴포넌트 + 비트마스크(`ECSAbilityType`이 이미 비트 설계)로 합칠 수 있음. |
| `CSAttributeSet` | 리플리케이션 표준 | H14. `PreAttributeChange` Super 미호출. Health 클램프 없음(`SetHealth` 직접 호출 시 Max 초과). MaxHealth 감소 시 재클램프 없음. `PostGameplayEffectExecute`마다 `Warning` 로그. |
| `Interface/CSProgressActivatable.h` | C9 | |
| `Physics/CSCollision.h` | ini 불일치 | P12 참조. `CSTrigger` 프로파일 `ObjectTypeName=""` → WorldStatic. `CSCapsule`의 `CSAction`/`GravityCore` 채널 `Response=` 누락(기본 Block). |
| `DataAsset/*` | 양호 | `CSOnTriggerEffectData` 참조 0. `CSDA_BoxProperties::BoxSize` 기본값 없음. Patrol/Player 데이터 필드 9개 중복. |
| `ChronoSpace.Build.cs` | 보통 | `GameplayTasks` 중복, `AIModule` 미선언, `GameplayAbilities/Tags`가 Private인데 Public 헤더가 include(모듈 분리 시 깨짐), 템플릿 주석. |

### 4.4 Actor (리액터·능력 오브젝트 계열)

| 파일 | 상태 | 주요 문제 |
|---|---|---|
| `CSAbilityReactorBase` | **가장 건강** | `bAlwaysRelevant` 없음(150m 밖 클라 미반응). 루트 스피어 `SetIsReplicated`. `TargetActors` 없으면 `IsTriggerActive()` 항상 false. 중복 헬퍼. |
| `CSReactorGroup` | 폐기 예정 | 대체 클래스와 동일 알고리즘 복제본. `bAlwaysRelevant`는 이쪽에만. |
| `CSSplineTrack` | 양호 | `ResolveRiderMove` 재귀에 종료 보장 없음 — 같은 거리·같은 PushPriority 라이더 2개면 **스택 오버플로**. `1 << 31` UB. 같은 엔트리 2개 도킹 시 먼저 떠나면 비트 꺼짐. |
| `CSSplineRider` | 양호 | 클라 `RepDistance` 초기값 0 → 레일 시작점에 놓였다가 미끄러짐. `ActiveSources` `TSet`이라 블랙홀 동심 스피어 하나만 빠져도 당김 중단. 클라 Tick 유휴에도 보간. |
| `CSTrackManager` | 단순 | `NetUpdateFrequency` 직접 대입(5.5 deprecated). CP949. |
| `CSBlackHole` | **가장 문제** | H15, H16, H17. `SetStopRange`가 `EventHorizonSphereTrigger` 반경 미갱신(두 판정 불일치). `bCheckMeshHaveComponent`여도 오버랩 시 모든 메시 중력 off → 대상 아닌 프롭 공중 정지. 두 블랙홀 겹침 시 한쪽 이탈이 중력 켬. `Destroyed()` 뒤 EndOverlap이 다시 와 `NotifyInteractionEnded` 2회. `MeshRadius` 0이면 inf. |
| `CSGravityCoreSphere` | 보통 | `Tick`에 `Super::Tick` 없음 → BP Event Tick 안 돎. 진입 시 `MaxAngularVelocity(180)`/`AngularDamping(2)` 걸고 이탈 시 미복원 → 한 번 닿은 프롭 영구 느려짐. 블랙홀과 복원 충돌. `OnCoreBeginOverlap`은 NoCollision 메시에 바인딩(죽은 코드). BeginPlay `LoadSynchronous`. |
| `CSGravitySwitch` | 불량 | C5, H27. 스위치 루프가 자신 포함 → `SetMaterial` 2회. `CharGravity.Z = ±Abs(Z)` Z축 정렬 가정. |
| `CSReverseGravityField` | 구조 타당 | H23. 0.25초마다 값 변화 없어도 `SetBoxExtent`(바디 재생성) 등 호출. `RequiredItemCount<=0`이면 `Items[0]` OOB. 프로퍼티 3개가 같은 OnRep → 중간 상태 적용. |
| `CSPartyPopper` | 큰 클래스 | C4. 입자 200개를 `UStaticMeshComponent`로 + 매 프레임 600회 트랜스폼 → ISM/Niagara로. `UpdateStringMesh`가 매 틱 스플라인 메시 재빌드. 반동을 서버 복제 + 클라 로컬 양쪽 → 떨림. `bHasFired` 비복제. |
| `CSBellows` | 오버랩 관리 좋음 | `LinkedActor`를 서버와 클라 OnRep 양쪽에서 로컬 이동 → 이동 복제 액터면 싸움; `bLinkedMoved` 비복제로 늦은 접속 시 문 닫힘. |
| `CSMeasuringTape` | 보통 | 오버랩 카운트 없음 → 한 명 나가면 남은 사람 앞에서 접힘. `bFaceReacting` 복제 플래그를 클라 로컬이 덮어씀. 유휴 틱. |
| `CSClawMachine` | 깔끔한 상태머신 | H24. `Home` 복귀 시 트리거 안 남은 플레이어 재검사 없음 → 영원히 대기. `GrabbedPlayer` `IsValid` 없음. |
| `CSGravityAnchorItem`, `CSWhiteHall`, `CSGravityCore`, `CSBlackHoleDummy` | 문제 없음 | 미사용 include, 주석 블록, 컴포넌트 `EditAnywhere` 정도. |

### 4.5 Actor (트리거·스위치·이동·퍼즐 계열)

| 파일 | 상태 | 주요 문제 |
|---|---|---|
| `CSMoveObjectSwitch` | **최악** | C3, H20, H21. `SetReplicateMovement(true)`(안 움직이는 액터). 이동 안 할 때도 Tick. BeginPlay마다 디버그 로그 6줄. `CSTransformMover`로 대체 권장. |
| `CSTransformMover` | 설계 가장 좋음 | 클라 시작 시각을 RPC 수신 시각으로 잡아 레이턴시만큼 뒤처지다 종료 시 스냅(서버 `ServerStartTime`을 인자로 보내면 해결). 늦은 접속 미대응. |
| `CSAnimatedTrap` | 모범 | 비활성 시 Tick 비활성화만 없음. |
| `CSAutoTrap` | 양호 | `Delay<=0` 스텝이면 타이머 미설정 → 패턴 조용히 멈춤. `ClampMin` 없음. |
| `CSRotatingActor` | 불량 | H22. 빈 `BeginPlay`. |
| `CSConveyorManager` / `CSConveyorPlatform` | 보통 | 매 틱 float 복제 + 클라 stale 타깃 보간 스터터. 플랫폼 `AddTickPrerequisiteActor` 없음 → 떨림. `BuildVisual` 호출 주석(죽은 코드). 주석은 NoCollision인데 코드는 BlockAll. `CSConveyorPlatform` `bReplicates` 무의미. |
| `CSTrackManager` / `CSMovingPlatform` | 보통 | `CSConveyor*`와 중복. 틱 의존성 없음. 빈 `BeginPlay`. |
| `CSCameraZoomVolume`, `CSFixedCameraVolume`, `CSSplitScreenTrigger` | 불량 | H19. `PlayersInTrigger` 드리프트(Begin에 PC 있고 End에 없으면 증가만). `CSFixedCameraVolume`: 원시 `APlayerController*`를 TMap 키로(GC 후 `IsValid` 역참조 가능). |
| `CSCameraViewProxy` | 양호 | 60Hz `FindComponentByClass<USpringArmComponent>`. `_Validate` 항상 true(NaN 미검사). 생성자 `SetActorLabel`. `GetPlayerController(World, 0)`. |
| `CSLabyrinthKey` / `Activator` / `Altar` | 불량 | H18, C6. 생성자에서 `SetActive(false)`(CDO에 런타임 작업). `TriggerRange`를 생성자에서만 읽음. `"BaseMap?listen"` 하드코딩. 로그 메시지 복붙. |
| `CSLevelTransferTrigger` | 보통 | 권한 처리 맞음. 지역 `FTimerHandle`로 타이머(핸들 유실, `bTriggerOnce=false`면 중복 타이머). 정상 흐름 `Warning`. |
| `CSTextTrigger` | 보통 | `IsPlayerControlled()` 비대칭 → 호스트 화면에 원격 플레이어 진입 텍스트 표시. 생성자 "Welcome to the game!" 하드코딩. 위젯 영구 보유. |
| `CSSwitchBase` | 불량 | C7, H20. `:55-63` 조건 뒤집힘(클래스 **미로드일 때만** 위젯 세팅 → 다른 스위치가 먼저 로드하면 적용 안 됨). `Interact()` 권한 가드 없음(서브클래스엔 있음). `ConstructorHelpers`와 DataAsset 이중 소스. |
| `CSStagePortal` | 양호 | 권한·검증·UI 분리 잘 됨. `DisableInput`이 Seamless Travel 켜면 새 레벨까지 지속(조건부). `GetFirstPlayerController` 3곳. |
| `CSMimicSourceZone` / `TargetZone` | 보통 | 점유자 무효화 후 재점유 시 `Release()` 없이 `Occupy()` → `SpawnedMimics` 누적, 이전 입력 바인딩 잔류. 오버랩 바인딩이 `Super::BeginPlay` 뒤라 초기 오버랩 누락. `OnConstruction`마다 MID 생성. |
| `CSProjectileGuideActor` | 불량 | H25. |
| `GridPuzzle/*` | 양호 | `CSGridBoard` 타일 상태 RPC 전용(H20). `CSGridLever::bUsed` 비복제. `CSGridBox` 계단식 이동. 프롬프트 코드 4번째 복제본. |
| `System/CSKillZone` | 불량 | H6. `KillZoneType`·`bAffectsPlayersOnly` 미사용. |
| `System/CSProgressTrigger` | 보통 | C9의 피해자. `bTriggered=true`가 권한 return 뒤 → 클라에서 `bTriggerOnce`여도 매번 UI 재생성(누수). |
| `System/CSCheckPoint`, `CSRespawnPoint` | 양호 | BeginPlay마다 `DebugNetworkInfo()` 로그. 미사용 include 7개. |

### 4.6 Subsystem / UI / Save / Setting / Settings / Debug

| 파일 | 상태 | 주요 문제 |
|---|---|---|
| `UI/CSViewFamilyViewportClient` | **가장 위험** | H31. `DrawOnscreenDebugMessages`에 `SceneCanvas`(엔진은 `DebugCanvas`). `ViewMode=VMI_Lit` 고정. 전환 중 매 프레임 ViewRect 폭 변화 → TSR/AutoExposure 히스토리 무효화(고스팅). 생성자에서 `PostLoadMapWithWorld.AddUObject`(CDO 포함). |
| `Subsystem/CSSplitScreenSubsystem` | 보통 | 프록시/캐릭터 캐시 미스 시 매 프레임 `TActorIterator` 2회(싱글·상대 접속 전 상시). `ResolveRemoteCharacter`가 "비로컬 첫 `ACSCharacterPlayer`" → 시체/미믹에 앵커. `GEngine->GameViewport`(전역)와 `GI->GetGameViewportClient()` 혼용(PIE 다중 창에서 다른 창 설정). `TransitionToFullScreen` 인자 무시(H19). `EditAnywhere` 튜닝값 12개가 편집 불가 위치. `FindComponentByClass<USpringArmComponent>` 매 프레임. |
| `Subsystem/CSEIKSubsystem` | 나쁨 | H32. `Deinitialize` 실패 경로에서 Super 생략, 로그인 안 했어도 `Logout(0)`. `LoginWithDeviceId` 호출마다 델리게이트 누적. 검색 결과 첫 항목 무조건 Join, 0건이면 로그 없음. `JoinSessionForBlueprint` 빈 함수. C++ 호출처 없음(BP 전용 가정). |
| `Subsystem/CSStageGameInstanceSubsystem` | 유물 | H33. 생성자 `ConstructorHelpers::FClassFinder`로 위젯 BP 엔진 기동 시 로드, 그 `AbilityHUDClass` 미사용. `GetAvailableAbilities()` 항상 빈 배열. 삭제 후보. |
| `Subsystem/CSGameProgressSubsystem` | 보통 | 진행도가 호스트 머신에만 존재, 클라에 전달 경로 없음 → 클라였던 사람이 호스트 맡으면 처음부터. `LoadGame` 실패 시 `CurrentSaveGame` null로 남아 세션 내내 저장 무력화(`SaveVersion` 기록만 하고 미비교). `IsClient()`가 `GetWorld()` 의존(`Deinitialize` 시점 불안정). `MarkStageCleared`+`SetLastPlayedStage` 연속 동기 저장 2회(트래블 직전 히치). `GetNextStage` `Chapter±1` 가정. |
| `Subsystem/CSPlayerSlotSubsystem` | 보통 | PS 유무로 키가 `net:..`/`remote:0`으로 갈림 → `GetDefaultPawnClassForController` 폴백으로 `remote:0` 생성 후 Logout은 `net:` 키로 삭제 시도 실패 → 영구 잔류 → 이후 전원 Player1. |
| `Subsystem/CSManagedActorSubsystem` | 보통 | `ActorsPulledByBlackHole` `TArray<TObjectPtr>` **UPROPERTY 없음**. `GetActorsPulledByBlackHole()` 호출마다 배열 복사. |
| `Subsystem/CSMimicWorldSubsystem`, `CSLabyrinthKeyWorldSubsystem` | 양호 | |
| `UI/CSTextWidget` | 보통 | `TypewriterTimer`를 `NativeDestruct`에서 미정리 → 제거된 위젯에서 BP 이벤트 계속 발화. `GetWorld()` null 무검사 4곳. `HideText`의 `IsVisible()` 기준이 `IsTextVisible()`과 다름. 정상 흐름 `Warning`. |
| `UI/CSGameUIWidget`, `CSHPDisplayWidget`, `CSStageSelectWidget`, `CSProgressUIWidget` | 양호 | HP 위젯 바인딩 시점 이름만 표시(클라 늦은 복제 시 "Player" 고정). StageSelect는 BP 자식이 디자이너 루트를 만들면 `BuildFrame` 스킵 → 빈 패널. |
| `UI/CSGASUserWidget`, `CSGASEnergyBarUserWidget`, `CSGASWidgetComponent` | 죽은 코드 | 본문 주석 처리, 호출처도 주석. `CurrentEnergy/CurrentMaxEnergy` 0이면 NaN. |
| `UI/CSAbilityHUD`, `CSHUD`, `SCSServerTravelWidget` | 유물 | 항상 빈 목록 / `return;` 뒤 도달 불가 코드 / 동일 블록 5회 복제 + `L_TestLevel1~5` 하드코딩. `NewObject<UTextBlock>(this)`는 `WidgetTree->ConstructWidget`이어야. |
| `Setting/CSUserSettings` | 보통 | `SetToDefaults` 미오버라이드 → "기본값 복원"에 볼륨 7개 안 돌아감. 슬라이더마다 `ApplyAudioVolumeSettings`+`SaveConfig`(드래그 중 프레임마다 ini 쓰기). `OnAudioDeviceCreated` 미연결, BP가 `ClearSoundMixModifiers` 한 번 부르면 캐시 때문에 재푸시 안 함. `BlueprintPure` 게터 비-const. |
| `Settings/CSAudioRoutingSettings`, `CSStageDataSettings` | 양호 | USTRUCT 멤버 `Config` 무의미. 중복 Chapter/Stage 검증 없음. |
| `Save/*` | 양호 | `SaveVersion` 미사용. 슬롯 이름 매직 문자열 2곳. `CSProgressTestPreset`이 `StagesPerChapter` 수동 입력(Settings에서 유도 가능), 항목 누락 챕터 건너뛰어 모순 세이브 생성 가능. |
| `Debug/CSMapSelectionWidget` | 보통 | `#if` 가드 없이 출시 빌드 포함. `/Game/Maps/%s`(맵은 `02_Map`), `Map_A/B/C` 부재. `AddDynamic` 미제거. |

---

## 5. 권장 수정 순서

각 단계는 독립적으로 머지 가능하다. 1→2→3 순서가 리스크 대비 효과가 가장 크다.

### 1단계 — 크래시·접속 끊김 제거 (작은 수정, 즉시)
- C1 `FindAndRemoveChecked`→`Remove`, `EndPlay` 순회 수정.
- C2 `GetPlayerCharacter(0)`→`SourceActor`, null 검사.
- C3 `CSMoveObjectSwitch` 매 틱 Multicast 제거(`CSTransformMover`로 교체하거나 시작/종료만 RPC).
- C4 `Explode()`에 `if (bIsExploding || bLaunchPending) return;`, 거리 트리거를 엣지 감지로.
- C5 `if (!IsValid(Char)) return;`.
- C6 Activator 루프 재작성(클램프, `RandRange(0, length-1)`, 활성 여부를 별도 플래그로).
- C7 `Data` null 검사 + `:55` 조건 반전 수정.
- C8 컨트롤러 null 검사.
- C9 `UINTERFACE(MinimalAPI, Blueprintable)`.
- H18 `Super::GetLifetimeReplicatedProps`. H5 `Super::Landed(Hit)` 한 줄 복원. H3 중복 바인딩 2줄 삭제.

### 2단계 — 리스폰·클라이언트 핵심 흐름 (GAS/캐릭터, 중간 규모)
- H1 어빌리티 1회 부여(플래그 또는 PlayerState에서 부여). H2 `SetupGASInputComponent` 멱등화 + `OnRep_PlayerState` null 검사.
- H4 `RespawnSinglePlayer`에서 사망 추적 키 갱신(또는 키를 `APlayerState`로).
- H6 `CSKillZone` 오버랩에 `HasAuthority()`, `SetDead/SetRevive`를 `ReplicatedUsing`으로.
- H7/H8 `NetExecutionPolicy` 명시, `CSGA_TimeRewind`에 `EndAbility` 오버라이드로 입력·중력 복구.
- H9 `Super::EndAbility` 즉시 호출, 카메라 복원은 캐릭터 컴포넌트 타이머로 분리.
- H10 `GravityCoef` `Replicated`. H11/H12 박스 크기·목표 위치를 GAS 타겟 데이터/어빌리티 파라미터로.
- H27/H28 static 제거 → GameState 복제 프로퍼티, 클라 보간을 `Slerp` 또는 서버와 같은 즉시 적용으로.
- 15개 GA에 `CommitAbility` 추가.

### 3단계 — 구조 통합 (리팩터, 큰 규모)
- AT/TA 트리오 공통화(`UCSAT_DurationTask`/`ACSTA_BoxTrigger`에 공통 로직 흡수, 자식은 `OnEnter/OnExit`만).
- 트리거 액터 5종 → `ACSPlayerTriggerBase`(권한 분리 + 카운트 + 로컬 PC 판정 한 곳에서). H19를 여기서 해결.
- 프롬프트 위젯 → `UCSInteractionPromptComponent`(로컬 플레이어만 표시).
- 이동 오브젝트 → `CSTransformMover`/`CSAnimatedTrap` 방식으로 통일(`CSMoveObjectSwitch`, `CSRotatingActor`, `CSConveyor*`, `CSGridBox`).
- 능력 영향 메시: 소스(블랙홀/중력코어)가 메시 상태를 직접 복원하지 않고, 메시 쪽 컴포넌트가 "원래 값 + 소스 카운트"를 보유(H15/H16/H17, GravityCoreSphere 복원 누락, 겹침 문제 일괄 해결).
- H20 상태를 `ReplicatedUsing` + 수동 OnRep으로 전환(늦은 접속 대응).
- `bAlwaysRelevant`를 리액터/트랙/라이더/풀무로 이동.

### 4단계 — 분할 화면 렌더 (`CSViewFamilyViewportClient`, 별도 트랙)
- H31: `Super::Draw`가 하는 작업 중 오디오 리스너·스트리밍 뷰 정보·ViewExtension·`LastRenderTime`을 커스텀 경로에 복제. `CalcSceneView` null·Views 0 가드. HUD 캔버스 `SceneView` 설정. `ULocalPlayer::Origin/Size`를 분할 중 상시 유지.

### 5단계 — 정리 (위험 낮음, 시간 날 때)
- 죽은 코드 삭제(P10 목록). `BlueprintCallable` 유물은 최소 `#if !UE_BUILD_SHIPPING`.
- 인코딩 UTF-8(BOM) 통일 + `.editorconfig`. 깨진 주석 재작성.
- `check()`→`ensureMsgf`+조기 반환. `Cast<>()->` 무검사 제거.
- `CSCollision.h`↔ini 정합. `Build.cs` 정리(`AIModule` 추가, 중복 제거).
- 유휴 Tick 비활성화(24개), `FindComponentByClass` 캐싱, 고정 dt 제거, 로그 레벨·카테고리 통일.
- Server RPC `_Validate`/권한/소유 검증(`ServerRequestStagePortalTravel`, `ServerSpawnAndSetBlackHole` 등).

---

## 부록 A. `GetFirstPlayerController` / `GetPlayerController(0)` / `GetPlayerCharacter(0)` 사용처

```
Actor/CSCameraViewProxy.cpp:74, 110
Actor/CSStagePortal.cpp:134, 161, 198
GA/AT/CSAT_AbilityPreviewBox.cpp:67, 73, 80
GA/TA/CSTA_BlackHoleSphere.cpp:158
Subsystem/CSEIKSubsystem.cpp:258
```

## 부록 B. CP949(비-UTF-8) 소스 파일 76개

```
Actor/CSAutoTrap.{h,cpp}  Actor/CSBellows.cpp  Actor/CSConveyorManager.{h,cpp}
Actor/CSLevelTransferTrigger.{h,cpp}  Actor/CSMeasuringTape.{h,cpp}  Actor/CSMoveObjectSwitch.{h,cpp}
Actor/CSProjectileGuideActor.{h,cpp}  Actor/CSRotatingActor.{h,cpp}  Actor/CSTextTrigger.{h,cpp}
Actor/CSTrackManager.cpp  Actor/System/CSProgressTrigger.{h,cpp}
ActorComponent/CSCameraZoomComponent.cpp  ActorComponent/CSCharacterScaleComponent.{h,cpp}
ActorComponent/CSKillZoneResetComponent.cpp  ActorComponent/CSProgressActivatableComponent.{h,cpp}
ActorComponent/CSVFXComponent.cpp
Animation/CSAnimInstance.cpp  Attribute/CSAttributeSet.cpp
Character/CSCharacterBase.cpp  Character/CSF_CharacterFrameData.h
Debug/CSMapSelectionWidget.{h,cpp}
GA/AT/CSAT_AbilityPreviewBox.{h,cpp}  GA/AT/CSAT_BlackHoleSphere.{h,cpp}  GA/AT/CSAT_ChronoControl.cpp
GA/AT/CSAT_ReverseGravityBox.cpp  GA/AT/CSAT_TimeRewind.{h,cpp}  GA/AT/CSAT_WeakenGravityBox.cpp
GA/CSGA_AbilityPreviewBox.cpp  GA/CSGA_BlackHole.{h,cpp}  GA/CSGA_CameraZoom.h  GA/CSGA_CharacterScale.{h,cpp}
GA/CSGA_GravityCore.cpp  GA/CSGA_ProjectileGuide.{h,cpp}  GA/CSGA_Sprint.cpp  GA/CSGA_TimeRewind.{h,cpp}
GA/CSGA_WindUp.h  GA/TA/CSTA_BlackHoleSphere.cpp  GA/TA/CSTA_ChronoControl.cpp  GA/TA/CSTA_ReverseGravityBox.cpp
Game/CSGameState.{h,cpp}  Interface/CSProgressActivatable.h
Subsystem/CSEIKSubsystem.h  Subsystem/CSStageGameInstanceSubsystem.{h,cpp}
UI/CSAbilityHUD.{h,cpp}  UI/CSGameUIWidget.{h,cpp}  UI/CSHPDisplayWidget.{h,cpp}  UI/CSHUD.cpp
UI/CSProgressUIWidget.{h,cpp}  UI/CSTextWidget.{h,cpp}  UI/SCSServerTravelWidget.cpp
```

## 부록 C. Tick 상시 활성 클래스 24개

`CSAnimatedTrap` `CSBellows` `CSBlackHole` `CSCameraViewProxy` `CSClawMachine` `CSConveyorManager` `CSConveyorPlatform` `CSFixedCameraVolume` `CSGravityCoreSphere` `CSMeasuringTape` `CSMoveObjectSwitch` `CSMovingPlatform` `CSPartyPopper` `CSRotatingActor` `CSSplineRider` `CSTrackManager` `CSTransformMover` `CSGridBox` `CSCameraZoomComponent` `CSCustomGravityDirComponent` `CSPushingCharacterComponent` `CSTransformRecordComponent`(빈 틱) `CSDecoyCharacter` `CSTA_BlackHoleSphere`
