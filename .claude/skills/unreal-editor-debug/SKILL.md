---
name: unreal-editor-debug
description: 언리얼 에디터 상태를 조회·제어하거나 디버깅할 때 사용한다. 출력 로그 읽기와 로그 카테고리 verbosity 조정, 콘솔 변수(CVar) 설정, PIE 플레이 시작·정지, 뷰포트 카메라 이동과 스크린샷, 액터/에셋 선택, 콘텐츠 브라우저 이동, 에디터 UI 자동화 등.
---

# 에디터 제어 · 디버깅

주 툴셋:
- `EditorToolset.LogsToolset` — 출력 로그 읽기, 로그 카테고리 verbosity 제어
- `EditorToolset.EditorAppToolset` — CVar, 에셋 이미징, 액터/에셋 선택, 뷰포트 카메라, 콘텐츠 브라우저, PIE 세션 제어
- `SlateInspectorToolset.SlateInspectorToolset` — 에디터 UI(Slate) 자동화

공통 규칙은 `unreal-mcp` 스킬 참고. 정확한 인자 스키마는 `describe_toolset`으로 확인한다.

## 디버깅 순서

"안 된다"는 보고를 받으면 이 순서로 접근한다.

1. **로그부터 읽는다.** `LogsToolset`으로 출력 로그를 확인한다. 대부분 여기서 원인이 나온다.
2. 관련 로그가 안 보이면 해당 카테고리의 verbosity를 올리고 재현한다.
3. 재현이 필요하면 `EditorAppToolset`으로 PIE를 시작한다.
4. 런타임 상태는 `ActorTools`/`ObjectTools`, GAS라면 `AbilitySystemInspectorToolset`으로 확인한다 → `unreal-gas`
5. 확인이 끝나면 PIE를 정지한다.

추측으로 코드를 고치기 전에 로그를 먼저 본다.

## PIE

- 런타임 상태를 조회하는 툴들(특히 GAS 인스펙터)은 **PIE가 켜져 있어야 의미 있는 값**을 준다.
- PIE를 켜두면 에디터 상태가 바뀐다. 조사 후 반드시 정지한다.
- 이 프로젝트는 데디케이티드 서버 구성이라 PIE 모드에 따라 동작이 달라진다. 서버/클라이언트 중 어디의 로그를 보는지 확인한다.

## 시각적 확인

화면으로 확인해야 하는 문제(UI 레이아웃, 이펙트가 안 보임)는 `EditorAppToolset`의 뷰포트 카메라 이동 + 스크린샷을 쓴다. 텍스트로만 추론하지 않는다.

## SlateInspectorToolset

에디터 UI 자체를 조작해야 할 때만 쓴다. Playwright 스타일이다.

- 얕은 루트 옵저버(depth 0)가 최상위 윈도우만 추적한다. **특정 창이나 패널을 다루기 전에 그 대상에 `Observe()`를 호출**해야 깊은 위젯까지 잡힌다. 끝나면 `Unobserve()`한다.
- 옵저버는 ~100ms마다 서브트리를 순회하므로 열어둔 채 방치하면 에디터가 느려진다.
- 대부분의 작업은 전용 툴셋으로 되므로, UI 자동화는 다른 방법이 없을 때의 마지막 수단이다.

## 콘솔 변수

CVar 변경은 에디터 세션에만 적용되고 프로젝트에 저장되지 않는다. 영구 설정이 필요하면 `Config/`의 ini를 고쳐야 한다 → `unreal-project-config`
