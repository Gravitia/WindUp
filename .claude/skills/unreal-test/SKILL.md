---
name: unreal-test
description: 언리얼 자동화 테스트를 실행하거나 결과를 확인할 때 사용한다. 테스트 목록 조회, 이름·태그로 테스트 검색, 테스트 실행과 진행 상태 모니터링, 결과 조회, 실행 중단 등. Session Frontend에서 하던 작업을 대신한다.
---

# 자동화 테스트 실행

주 툴셋: `AutomationTestToolset.AutomationTestToolset`

Session Frontend가 쓰는 것과 같은 `IAutomationControllerManager` API를 감싼 것이다. 공통 규칙은 `unreal-mcp` 스킬 참고.

## 정해진 워크플로

이 툴셋은 순서가 정해져 있다. 건너뛰면 동작하지 않는다.

1. `DiscoverTests()` — **세션당 한 번.** 워커를 초기화하고 테스트 목록을 로드한다. 이걸 안 하면 뒤가 전부 빈 결과다.
2. `ListTests()` — 이름이나 태그로 테스트를 찾는다.
3. `RunTests()` — 찾은 테스트 이름으로 실행한다.
4. `GetTestStatus()` / `GetTestResults()` — 진행 상황과 결과를 확인한다.
5. `StopTests()` — 중단이 필요할 때.

## 주의

- 테스트 이름은 `ListTests()` 결과에서 가져온다. 지어내지 않는다.
- `RunTests()`는 비동기다. 바로 결과를 조회하면 미완료 상태가 나온다. `GetTestStatus()`로 완료를 확인한 뒤 `GetTestResults()`를 부른다.
- 전체 테스트를 무작정 돌리면 오래 걸린다. 관련된 테스트로 좁혀 실행한다.
- 테스트가 실패하면 **실패했다고 그대로 보고한다.** 출력을 함께 보여준다. 통과한 것처럼 요약하지 않는다.
- 원인 추적은 `LogsToolset`으로 로그를 병행해서 본다 → `unreal-editor-debug`

## C++ 빌드가 필요한 경우

테스트 코드 자체를 추가/수정하는 것은 `Source/ChronoSpace` 편집 + 빌드 작업이다. MCP가 아니라 일반 파일 도구와 빌드 명령을 쓴다. 빌드 후 에디터를 재시작해야 새 테스트가 `DiscoverTests()`에 잡힌다.
