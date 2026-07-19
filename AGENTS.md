# Codex Project Notes

## 질문에 대한 답변 규칙
- 블루프린트, .uasset 파일 과 같은 파일들에 대한 read, 혹은 write 요청이 들어오는 경우 우선 unreal mcp를 사용하는 것을 반드시 확인한다.

## Project
- Unreal Engine project: `ChronoSpace.uproject`
- Workspace root: `C:\Users\ASUS\Documents\Unreal Projects\WindUp`
- Main game module: `Source/ChronoSpace`

## Top-Level Structure
- `.codex/`: local Codex configuration and project notes.
- `.github/`: GitHub workflow or repository metadata.
- `Binaries/`, `Intermediate/`, `DerivedDataCache/`, `Saved/`: generated Unreal build/editor output. Avoid editing these manually unless explicitly requested.
- `Build/`: Unreal build resources.
- `Config/`: project configuration files such as `DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`, and gameplay tags.
- `Content/`: Unreal assets, maps, Blueprints, UI, data assets, audio, meshes, materials, Niagara, and related content.
- `Plugins/`: project plugins, including `AdvancedSessions`, `AdvancedSteamSessions`, `EOSIntegrationKit-Version5`, and `unreal-mcp`.
- `Source/`: C++ source code and target/build files.

## Source Layout
- `Source/ChronoSpace/Actor`: gameplay actors.
- `Source/ChronoSpace/ActorComponent`: reusable actor components.
- `Source/ChronoSpace/Animation`: animation-related code.
- `Source/ChronoSpace/Attribute`: attribute/stat systems.
- `Source/ChronoSpace/BT`: behavior tree related code.
- `Source/ChronoSpace/Character`: character classes and logic.
- `Source/ChronoSpace/Common`: shared project code.
- `Source/ChronoSpace/DataAsset`: C++ data asset types.
- `Source/ChronoSpace/Debug`: debugging helpers.
- `Source/ChronoSpace/GA`: gameplay ability related code.
- `Source/ChronoSpace/Game`: game mode, game state, and core game classes.
- `Source/ChronoSpace/Interface`: interfaces.
- `Source/ChronoSpace/Pawn`: pawn classes.
- `Source/ChronoSpace/Physics`: physics-related code.
- `Source/ChronoSpace/Player`: player controller/state/input related code.
- `Source/ChronoSpace/Save`: save game code.
- `Source/ChronoSpace/Subsystem`: Unreal subsystem code.
- `Source/ChronoSpace/UI`: C++ UI classes.

## Content Layout
- `Content/01_Blueprint`: project Blueprints.
- `Content/02_Map` and `Content/Maps`: maps and level assets.
- `Content/03_Input`: input assets.
- `Content/04_DataAssets`: project data assets.
- `Content/05_ThirdPerson`: third-person template or related assets.
- `Content/07_LyraCharacter`: Lyra character assets.
- `Content/10_BehaviorTree`: AI behavior tree assets.
- `Content/11_Camera`: camera-related assets.
- `Content/12_Render`: rendering assets/settings.
- `Content/20_Data`: general data assets.
- `Content/30_Mesh`: meshes.
- `Content/31_Material`: materials.
- `Content/32_PhysicsMaterials`: physics materials.
- `Content/34_Niagara`: Niagara VFX.
- `Content/35_Font`: fonts.
- `Content/40_Audio`: audio assets.
- `Content/50_LevelInstance`: level instance assets.
- `Content/60_Character`: character assets.
- `Content/90_Movies`: movie/media assets.
- `Content/99_Asset`: miscellaneous assets.
- `Content/EasyGameUI`, `Content/USCS`, `Content/Realistic_Starter_VFX_Pack_Vol2`: third-party or imported asset packs.
- `Content/__ExternalActors__`, `Content/__ExternalObjects__`: Unreal external actor/object storage. Treat as generated/editor-managed data.

## Working Guidelines
- Prefer existing project conventions and folder ownership when adding or changing code.
- Do not manually edit generated Unreal output directories unless the task specifically requires it.
- For C++ changes, keep edits inside `Source/ChronoSpace` unless build targets or module configuration need updates.
- For project settings or input/tag changes, check `Config/` first.
