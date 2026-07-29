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
- `T`: toggle between first-person gameplay and the top-down building view;
- `1`, `2`, `3`: change tool/building mode;
- mouse wheel: rotate the preview;
- `E`: confirm/place a valid green preview;
- left mouse button: damage trace;
- right mouse button: mallet interaction.

The original EBS `V` binding is disabled. It cycled through first person,
top-down and third person, while Botanicus intentionally supports only first
person and top-down construction. The original `T` radial/square-menu shortcut
is also replaced by the Botanicus camera toggle.

During normal gameplay, the native Botanicus first-person camera is forced and
all inherited third-person cameras are disabled.

In Botanicus top-down mode:

- the character cannot move or rotate;
- `W`, `A`, `S`, `D` pan the camera freely on the world X/Y axes;
- the mouse remains available for cursor-based construction;
- left-clicking a structural piece selects its complete connected building;
- clicking furniture selects the containing building and never the furniture
  alone;
- the selected group includes connected foundations, walls, ceilings, roofs
  and all EBS contents located inside the resulting building volume;
- moving the mouse changes only the building's world X/Y position;
- its Z position is sampled directly from the Landscape so the complete group
  follows the terrain relief instead of colliding with roofs, trees or foliage;
- the original foundation-to-ground offset is preserved while moving;
- the mouse wheel rotates the complete group in 15-degree steps;
- left click or `E` confirms the new transform;
- right click or `Escape` cancels and restores every actor to its original
  transform;
- pressing `T` returns directly to first person;
- third person is not exposed as a playable camera state.

Whole-building transformations are requested by the owning client, applied by
the server and replicated actor by actor. EBS ownership is checked when the
source building enables its ownership interface. Demonstration buildings that
do not use ownership remain editable for prototype testing.

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
