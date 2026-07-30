# EBS compatibility layer in Botanicus

## Product rule

Botanicus is not a survival or modular construction game. Players never place
individual foundations, walls, roofs or ceilings, and never damage or destroy a
building piece.

Players start inside a complete building. Progression comes from:

- ordering equipment and decorations;
- collecting delivered objects from the delivery area;
- carrying small objects in the ten-slot inventory;
- carrying large objects with two hands or cooperatively, depending on weight;
- purchasing complete buildings with predefined gameplay functions;
- creating outdoor paths to connect buildings.

## What remains from Easy Building System

The marketplace pack is retained only as a temporary compatibility layer for:

- identifying the structural actors belonging to one complete building;
- grouping the building and all contents located inside it;
- moving and rotating that complete group in top-down mode;
- authoritative multiplayer transforms, placement collision and edit locks;
- existing prototype building meshes and save interfaces.

Vendor content remains untouched in `/Game/EasyBuildingSystem`. Botanicus-owned
integration assets remain in `/Game/Botanicus/Building`.

## Features deliberately disabled

Player input no longer exposes:

- the `Q` modular building menu;
- snapping and grid controls (`C`, `G`);
- floor/debug/demo save controls (`Page Up`, `Page Down`, `Z`, numpad `1/3`);
- EBS hatchet, pickaxe and mallet inventory slots;
- left-click damage/destruction traces;
- right-click mallet interaction;
- the original `V` three-camera cycle.

The complete EBS demonstration HUD is collapsed at runtime, including its
resource counters, shortcut list, construction menus and tool strip. Keyboard
numbers belong to the Botanicus inventory.

## Complete-building placement

The current prototype top-down system already:

- locks character movement;
- pans the camera with `W`, `A`, `S`, `D`;
- zooms the camera with the mouse wheel when no building is selected;
- zooms with `Shift + mouse wheel` while moving a selected building;
- selects a complete connected building, never an isolated piece of furniture;
- includes equipment and decoration inside the building volume;
- follows Landscape height while moving on X/Y;
- rotates in 15-degree steps with the mouse wheel while a building is selected;
- shows valid/invalid placement;
- validates collision and Landscape placement again on the server;
- confirms with left click or `E`;
- cancels with right click or `Escape`;
- locks the building against concurrent editing by another player.

Manual `T` access remains available for prototype testing. In the production
flow, buying a building will automatically enter this mode for the purchaser and
provide the newly purchased complete building as the selected group.

## Planned connection between buildings

If a purchased building is placed against an existing building:

1. detect the common wall and compatible connection zone;
2. preview an automatic communication door;
3. let the purchaser slide the door along the valid part of the common wall;
4. validate the building transform and door position together on the server;
5. create the opening/door without giving players direct wall-editing tools.

This door workflow is not implemented yet.

## Player-created paths

Paths are the only player-created exterior construction network. They are not
made from EBS foundations or modular building pieces.

The first native path prototype now:

- exposes a `CHEMIN` button in the top-down planning toolbar;
- adds Landscape-projected spline points with left click;
- previews the next segment under the cursor;
- magnetically snaps path endpoints to nearby building doors;
- snaps endpoints to the nearest point on an existing path and creates a
  replicated junction piece;
- exposes a `SUPPRIMER ROUTE` mode that removes the clicked segment;
- splits a multi-segment route into its valid remaining pieces after deletion;
- removes the last point with right click;
- confirms or cancels from the toolbar;
- replicates confirmed paths from the authoritative server;
- saves and reloads the complete path network.

Slope/obstacle validation, final art meshes and path costs remain planned
refinements.

When a complete building is moved, connected path endpoints must be detected.
The placement preview will show whether they can reconnect to a door at the new
position; existing paths must never be silently stretched through obstacles.

Path meshes, materials, width variants, construction cost and more advanced
editing tools still require final game-design decisions.

## Multiplayer authority

Building purchase, placement, rotation, door creation and ownership must be
server-authoritative. A building always moves with all of its contents. No API
should expose individual structural-piece destruction or movement to players.

## Rebuilding the bridge

`Tools/IntegrateEBS.py` recreates or repairs the Botanicus integration Blueprints
without modifying the marketplace source assets.
