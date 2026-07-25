# 코드 · 콘텐츠 배치

새 파일을 만들 때는 기존 폴더 소유권을 따른다. 애매하면 비슷한 기존 파일이 어디 있는지 먼저 찾아보고 그 옆에 둔다.

## Source (`Source/ChronoSpace`)

게임 모듈 하나다. 빌드 설정은 `ChronoSpace.Build.cs`, 타깃은 `Source/*.Target.cs`.

`Actor`, `ActorComponent`, `Animation`, `Attribute`, `BT`, `Character`, `Common`, `DataAsset`, `Debug`, `GA`, `Game`, `Interface`, `Physics`, `Player`, `Save`, `Setting`, `Settings`, `Subsystem`, `UI`

C++ 변경은 모듈 설정을 손댈 이유가 없는 한 `Source/ChronoSpace` 안에서 끝낸다.

`Setting`과 `Settings`는 이름이 비슷하지만 역할이 다르다. 오타가 아니므로 통합하지 않는다.

- `Setting` — 사용자 설정 (`CSUserSettings`). 플레이어가 게임 안에서 바꾸는 값.
- `Settings` — 개발자 설정 (`CSAudioRoutingSettings`, `CSStageDataSettings`). 프로젝트 설정에 노출되는 값.

## Content

번호 접두 폴더가 실질적인 분류다.

`01_Blueprint`(BP) · `02_Map`, `Maps`(레벨) · `03_Input` · `04_DataAssets` · `10_BehaviorTree` · `11_Camera` · `12_Render` · `20_Data` · `21_Animation` · `30_Mesh` · `31_Material` · `32_PhysicsMaterials` · `34_Niagara` · `35_Font` · `40_Audio` · `50_LevelInstance` · `60_Character` · `90_Movies` · `99_Asset`

에셋 검색 시 전체를 뒤지지 말고 이 폴더로 범위를 좁힌다.

## 서드파티 에셋은 복제해서 쓴다

`05_ThirdPerson`, `07_LyraCharacter`, `EasyGameUI`, `DemonicUI`, `AdvancedMenu`, `USCS`, `Realistic_Starter_VFX_Pack_Vol2`, `Jet_engine_effects`는 외부에서 가져온 팩이다.

원본을 직접 수정하지 않는다. 프로젝트 폴더로 복제하거나 파생 에셋을 만들어 쓴다. 원본을 고치면 팩 업데이트 시 변경분이 날아가고, 무엇을 바꿨는지 추적하기 어려워진다.

## 프로젝트 설정

입력, 게임플레이 태그, 엔진 설정 변경은 `Config/`를 먼저 확인한다. 코드로 우회하지 않는다.
