# Easy Building System V10 in Botanicus

## Status

Easy Building System V10 is integrated and enabled as the gameplay foundation.

- Fab source project: Unreal Engine 5.3
- Botanicus target: Unreal Engine 5.8
- UE 5.8 Blueprint verification: 0 errors, 0 warnings
- Runtime smoke test: successful
- Steam Online Subsystem remained available during the runtime test

The vendor folder contains 1,075 files (about 531 MB):

`/Game/EasyBuildingSystem`

Do not edit this folder for Botanicus-specific work. Keeping the marketplace
content untouched makes updates and troubleshooting much easier.

## Botanicus integration layer

Botanicus-specific copies live here:

`/Game/Botanicus/Building`

The active classes are:

- `BP_BotanicusBuilderCharacter`
  - derives from native `ABotanicusCharacter`;
  - retains the EBS character interaction component and animation events;
  - also owns the native Botanicus interaction and quick-bar components.
- `BP_BotanicusPlayerController`
  - derives from native `ABotanicusPlayerController`;
  - retains EBS building, resources, save/load, HUD and player-interface logic;
  - retains Botanicus Steam session commands and camera setup.
- `BP_BotanicusBuildingGameMode`
  - derives from native `ABotanicusGameMode`;
  - uses the two integration classes above.

`DefaultEngine.ini` selects `BP_BotanicusBuildingGameMode` as the global game
mode. Steam-hosted travel to `Lvl_FirstPerson` therefore uses the same
integrated classes.

## Input

Botanicus continues to support keyboard and mouse only.

Legacy mappings were added only so the EBS demonstration graphs remain
compilable:

- movement: `W`, `A`, `S`, `D`;
- camera: mouse;
- jump: `Space`;
- alternate camera rate: arrow keys.

Controls confirmed during the first in-editor test:

- `Q`: open/close the building menu;
- `T`: switch between radial and square building menus;
- `1`, `2`, `3`: change tool/building mode;
- mouse wheel: rotate the preview;
- the yellow building-menu button: toggle the Botanicus top-down building view;
- `E`: confirm/place a valid green preview;
- left mouse button: damage trace;
- right mouse button: mallet interaction.

The original EBS `V` binding is disabled. It cycled through first person,
top-down and third person, while Botanicus intentionally supports only first
person and top-down construction.

In Botanicus top-down mode:

- the character cannot move or rotate;
- `W`, `A`, `S`, `D` pan the camera freely on the world X/Y axes;
- the mouse remains available for cursor-based construction;
- pressing the same yellow button returns directly to first person;
- third person is not exposed as a playable camera state.

The yellow button is a functional placeholder injected into both EBS HUD
variants. Its final size, icon, typography and placement can be redesigned in
the Botanicus UI without changing the camera code.

The EBS player controller also contains direct key events for `Tab`, `C`, `G`,
`Z`, `Left Shift`, right mouse button, `Page Up`, `Page Down`, numpad `1` and
numpad `3`. Their final assignments will be normalized later.

These direct bindings are inherited from the pack. They should be normalized
into Botanicus Enhanced Input actions after the first playable building test
and after the final UI/control scheme is chosen.

## Building content

The main data tables are:

- `/Game/EasyBuildingSystem/Blueprints/DataTables/DT_EBS_BuildingLists`
- `/Game/EasyBuildingSystem/Blueprints/DataTables/DT_EBS_BuildingObjects`
- `/Game/EasyBuildingSystem/Blueprints/DataTables/DT_EBS_Requirements`

They define the available construction pieces and their requirements. Replace
the dummy, polygonal or stylized meshes through Botanicus-owned child
Blueprints or data-table rows; do not overwrite the vendor assets.

Initial foundations and free-standing objects expect an Unreal `Landscape`.
The geometric floor in `Lvl_FirstPerson` is a `StaticMeshActor`, so EBS treats
it as an invalid initial placement surface and keeps the preview red. Use
`/Game/Botanicus/Maps/Lvl_BuildingTest` for integration tests; it contains a
compatible Landscape and uses the Botanicus building GameMode.

## Multiplayer caution

The EBS components and building actors contain replicated/server-side logic,
and the combined GameMode starts correctly with the Steam subsystem. This is
not yet a full multiplayer acceptance test. Before building production content,
validate with two Steam users that:

1. a client can join the host;
2. a client placement is authorized and spawned by the server;
3. both players see placement, upgrades, repairs and destruction;
4. ownership and resource deductions are authoritative;
5. saved buildings reload correctly on the host/server.

## Rebuilding the bridge

`Tools/IntegrateEBS.py` recreates or repairs the three Botanicus integration
Blueprints without changing the source marketplace assets.
