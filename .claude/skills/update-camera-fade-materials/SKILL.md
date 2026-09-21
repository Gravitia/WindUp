---
name: update-camera-fade-materials
description: 카메라와 캐릭터 사이를 가리는 오브젝트를 반투명으로 페이드하는 기능의 머티리얼 쪽 규칙과 작업 절차. 새 머티리얼·새 대상 오브젝트가 들어왔을 때, PIE 로그에 "CameraOcclusionFade" 경고가 떴을 때, 페이드가 안 된다는 보고를 받았을 때 쓴다.
---

# 카메라 가림 페이드 · 머티리얼 설정

## 구조 (읽고 시작한다)

- 판정과 값 세팅: `Source/ChronoSpace/Subsystem/CSCameraOcclusionFadeSubsystem.*`
  매 틱 카메라→캐릭터 광선으로 "막힌 비율" 을 구해 메시 컴포넌트의 **Custom Primitive Data 0번** 에 넣는다 (0 = 불투명, 1 = 최대 페이드).
- 그리는 쪽: `Content/31_Material/CameraFade/MF_CameraFade`
  CPD 0번을 읽어 `DitherTemporalAA` 마스크를 만든다. 대상 오브젝트의 **부모 Material** 이 Masked 이고 OpacityMask 에 이 함수가 꽂혀 있어야 한다.
- 대상: `UCSMeshPulledByBlackhole` 또는 `UCSMeshAffectedByGravityCore` 가 붙은 액터. 컴포넌트가 `BeginPlay` 에서 서브시스템에 등록한다.
- 설정: 프로젝트 설정 > Game > ChronoSpace Camera Occlusion Fade (`CSCameraOcclusionFadeSettings`). `PrimitiveDataIndex` 는 함수 안 파라미터의 인덱스와 같아야 한다. 어긋나면 첫 등록 때 Error 로그가 뜬다.

머티리얼 인스턴스는 부모가 준비돼 있으면 자동으로 따라온다. 손대는 단위는 언제나 **Material(그래프)** 이다.

## 새 머티리얼 / 새 대상이 들어왔을 때

에디터 콘솔에서:

```
CS.CameraFade.SetupMaterials save
```

- 프로젝트의 블루프린트 전부와 맵 전부(서브레벨, 월드 파티션 외부 액터 포함)를 훑어 대상 액터의 부모 Material 을 찾고, 함수 삽입 → Masked 전환 → 숨은 플래그 정리 → 저장까지 한다.
- 결과는 로그 `CameraFade: 설정 완료 - ...` 한 줄과 머티리얼별 `삽입 / 합성 / 플래그 정리 / 건너뜀` 줄로 나온다.
- 열린 맵만 빨리 보려면 `CS.CameraFade.SetupMaterials current save`.
- `save` 를 빼면 더티만 남기니 Ctrl+S 로 저장한다.

처리 규칙:

| 원본 상태 | 처리 |
|---|---|
| Opaque | 함수 호출 노드 추가, OpacityMask 연결, Masked 전환 |
| Masked, 기존 마스크 있음 | `Multiply(기존 마스크, 함수)` 를 OpacityMask 에 연결. 원래 구멍 유지 |
| 이미 함수 있음, Masked, OpacityMask 연결됨 | 건너뜀 (숨은 플래그만 검사) |
| 이미 함수 있음, 그러나 연결이나 Masked 전환이 빠짐 | 남은 단계만 채운다. 노드는 다시 안 꽂는다 |
| Translucent 등 | 건너뛰고 Warning 으로 보고. 유리처럼 이미 비치면 그대로 둔다 |
| Surface 도메인 아님, MaterialAttributes 사용 | 건너뛰고 보고 |
| 엔진(`/Engine`)·플러그인·서드파티 팩 머티리얼 | 건너뛰고 보고. 대상 액터가 쓰고 있으면 프로젝트 폴더로 복제한 머티리얼로 바꿔 준다 |

## 페이드가 안 된다는 보고를 받았을 때

1. PIE 시작 직후 출력 로그에서 `CameraOcclusionFade` 를 검색한다. 대상 액터가 등록될 때 머티리얼이 Masked 가 아니거나 함수가 없으면 머티리얼 경로와 액터 이름이 Warning 으로 찍힌다. 이 목록이 곧 작업 대상이다 → 위 명령을 돌린다.
2. 경고가 없는데 안 되면 콘솔 `cs.CameraFade.Debug 1`. 화면에 후보 수, 페이드 중인 액터와 목표/현재 값이 뜬다. 후보에 없으면 컴포넌트 등록 문제, 값은 오르는데 안 비치면 머티리얼 문제다.
3. 머티리얼 문제면 `CS.CameraFade.FixMaterials` 를 돌린다. 함수는 있는데 숨은 플래그가 남았거나, OpacityMask 연결·Masked 전환을 빠뜨린 경우를 잡는다.

## 숨은 플래그 두 개 (코드로 머티리얼을 건드릴 때 반드시 안다)

머티리얼 에디터에서 손으로 하면 UI 가 정리하지만, MCP 나 코드로 연결하면 남아서 **컴파일은 통과하고 마스크만 조용히 죽는다**. 둘 다 `ObjectTools` 로 읽거나 쓸 수 없다.

- `UMaterial::bCanMaskedBeAssumedOpaque` — Opaque 시절 캐시. 켜져 있으면 Masked 여도 Opaque 로 컴파일된다. 5.8 엔진에 재계산 코드가 없다.
- OpacityMask 입력의 `UseConstant` — FBX 임포터가 켜 둔다. 켜져 있으면 꽂은 노드 대신 상수 1을 쓴다.

MCP `MaterialTools` 로 함수를 꽂았다면 뒤이어 반드시 `CS.CameraFade.FixMaterials` 를 돌린다. `SetupMaterials` 는 이 단계를 포함한다.

## MCP 로 수동 처리할 때 (명령을 못 쓰는 경우)

1. `MaterialTools.add_expression` 으로 `MaterialExpressionMaterialFunctionCall` 추가 → `ObjectTools.set_properties` 로 `MaterialFunction` 에 `MF_CameraFade` 지정.
2. `MaterialTools.connect_to_output` (output_name `OpacityMask`, property `MP_OpacityMask`).
3. `ObjectTools.set_properties` 로 `BlendMode` = `BLEND_Masked`.
4. `MaterialTools.recompile`, `AssetTools.save_assets`.
5. **콘솔에서 `CS.CameraFade.FixMaterials`** 를 돌리고 다시 저장한다. 이 단계를 빼면 안 보이는 채로 실패한다.

## 하지 말 것

- 서드파티 팩(`05_ThirdPerson`, `07_LyraCharacter`, `EasyGameUI`, `DemonicUI`, `AdvancedMenu`, `USCS`, `Realistic_Starter_VFX_Pack_Vol2`, `Jet_engine_effects`) 머티리얼에는 넣지 않는다. 명령도 제외한다.
- `MF_CameraFade` 안의 CPD 인덱스와 설정의 `PrimitiveDataIndex` 를 한쪽만 바꾸지 않는다.
- 원본 머티리얼을 복제해 페이드 전용 버전을 만들지 않는다. 그 방식은 노드 종류마다 지원이 필요해 폐기했다.
