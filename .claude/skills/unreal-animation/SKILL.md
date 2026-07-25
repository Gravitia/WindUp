---
name: unreal-animation
description: 언리얼의 애니메이션과 시네마틱 작업에 사용한다. 레벨 시퀀스 생성·재생·트랙과 바인딩, 키프레임과 커브 편집, 컨트롤 리그 리깅과 애니메이션 레이어, 시퀀서 FBX 임포트/익스포트, 스켈레탈 메시 본·소켓·머티리얼, 스태틱 메시 검사 등.
---

# 애니메이션 · 시퀀서 작업

주 툴셋:
- `animation_toolset.toolsets.sequencer.SequencerTools` — 시퀀스 생성/재생/트랙/바인딩/섹션 (시퀀서의 기본 진입점)
- `animation_toolset.toolsets.keyframing.SequencerKeyframingTools` — 키프레임, 채널, 커브 에디터
- `animation_toolset.toolsets.controlrig.ControlRigTools` — 컨트롤 리그 에셋 자체 편집
- `animation_toolset.toolsets.controlrig_sequencer.SequencerControlRigTools` — 시퀀서 안에서 컨트롤 리그 애니메이션
- `animation_toolset.toolsets.outliner.SequencerOutlinerTools` — 시퀀서 아웃라이너 트리 조회/선택/mute·solo·lock
- `animation_toolset.toolsets.import_export.SequencerImportExportTools` — FBX / AnimSequence 입출력
- `animation_toolset.toolsets.conditions.SequencerConditionTools` — 트랙 런타임 조건
- `animation_toolset.toolsets.custom_bindings.SequencerCustomBindingTools` — possessable/spawnable/replaceable 변환
- `editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools` — 스켈레탈 메시, 본 계층, 소켓
- `editor_toolset.toolsets.static_mesh.StaticMeshTools` — 스태틱 메시

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 툴셋 고르기

시퀀서 툴셋이 여러 개로 쪼개져 있다. 헷갈리면 이 순서로 판단한다.

| 하려는 일 | 툴셋 |
|---|---|
| 시퀀스를 만들고 액터를 붙이고 트랙을 추가 | `SequencerTools` |
| 값에 키를 찍고 보간을 조정 | `SequencerKeyframingTools` |
| 시퀀서에서 리그 컨트롤을 움직이고 키 | `SequencerControlRigTools` |
| 컨트롤 리그 에셋의 계층/그래프 자체를 편집 | `ControlRigTools` |
| 트랙이 보이는지, mute/lock 상태 확인 | `SequencerOutlinerTools` |
| FBX·AnimSequence 주고받기 | `SequencerImportExportTools` |
| 본 이름, 소켓 추가, 메시 머티리얼 슬롯 | `SkeletalMeshTools` |

## 표준 작업 순서

1. `SequencerTools`로 시퀀스를 열거나 만든다. **대부분의 다른 시퀀서 툴은 시퀀스가 열려 있어야 동작한다.**
2. 액터를 바인딩한다.
3. 트랙/섹션을 추가한다.
4. 키를 찍는다.
5. `AssetTools.save_assets`로 저장하고, 열어둔 시퀀스는 닫는다.

## 주의

- 바인딩 없이 트랙을 추가하려 하면 실패한다. 순서를 지킨다.
- 본과 소켓 이름은 스켈레톤마다 다르다. `SkeletalMeshTools`로 조회하고 쓴다. 추측하지 않는다.
- FBX 익스포트는 디스크에 파일을 쓴다. 경로를 사용자에게 알리고 진행한다.

## 이 프로젝트에서

- 캐릭터 에셋은 `Content/60_Character`, `Content/07_LyraCharacter`에 있다.
- 애니메이션 관련 C++는 `Source/ChronoSpace/Animation`, 카메라는 `Content/11_Camera`에 있다.
- 무비/미디어는 `Content/90_Movies`에 있다.
