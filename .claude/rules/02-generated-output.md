# 생성 디렉터리 취급

`Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`, `Content/__ExternalActors__`, `Content/__ExternalObjects__`는 언리얼이 생성·관리한다. 명시적 요청이 없으면 수동으로 편집하지 않는다.

빌드 산출물을 고쳐 증상을 없애려 하지 말고 원인이 되는 소스나 설정을 고친다.

## Binaries는 통째로 무시되는 게 아니다

이 프로젝트는 `.uproject` 더블클릭만으로 에디터가 뜨도록 **에디터 DLL 일부를 의도적으로 커밋**한다. `.gitignore`가 `Binaries/`를 막은 뒤 다음 항목만 되살린다.

- `Binaries/Win64/UnrealEditor-ChronoSpace.dll`
- `Binaries/Win64/UnrealEditor.modules`
- `Binaries/Win64/ChronoSpaceEditor.target`
- `Plugins/*/Binaries/Win64/UnrealEditor-*.dll`, `UnrealEditor.modules`
- `Plugins/*/Binaries/Win64/EOSSDK-Win64-Shipping.dll`

따라서 C++ 빌드 후 `git status`에 이 DLL들이 뜨는 것은 **정상이며 커밋 대상**이다. "빌드 산출물이니 빼자"고 판단해 `.gitignore`를 고치거나 파일을 되돌리지 않는다.

`.gitignore`의 해당 규칙은 "마지막 매치가 이긴다"는 순서에 의존한다. 이 파일의 규칙 순서를 재배치하지 않는다.

## `Content/__ExternalActors__`

World Partition이 관리한다. 레벨을 저장하면 이 아래 파일이 함께 변한다. 정상이므로 되돌리지 않는다.
