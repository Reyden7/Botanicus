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

Botanicus uses a native AZERTY movement layer: `Z/S` moves forward/backward and
`Q/D` moves left/right in both first-person and top-down modes. These keys are
consumed before the marketplace QWERTY mappings, so `Q` and `Z` cannot reopen
the disabled EBS construction/debug shortcuts.

The gameplay camera uses a true-first-person foundation. It is attached to the
animated `head` socket of the full-body mesh; mouse yaw rotates the complete
character and mouse pitch rotates the view without tilting the collision
capsule. The owner sees the full body, while only that owner's head bone and
the separate template first-person mesh are hidden to prevent skull clipping.
Remote players continue to see the complete character.

While a local player is in top-down mode, EBS roof components are hidden only
for that player's view. Their collision is ignored by the planning cursor so
floors, furniture, walls, doors and communication-door candidates remain
selectable. Roofs become visible again on return to first person; no replicated
or saved actor state is modified.

## Test building purchase

The native top-down toolbar currently exposes an `ACHETER BATIMENT TEST`
button. Until the final catalogue exists, it uses the complete building nearest
to the player as a template, duplicates the whole structural group on the
server, and immediately gives the copy to the purchaser for placement.

- The test purchase is free.
- Confirming keeps the new complete building.
- Cancelling destroys the provisional copy.
- The spawned actors replicate to every player.
- Runtime-purchased buildings store their class and transform in autosave
  version 3 and are respawned when the map is loaded.

The temporary template lookup will be replaced by explicit catalogue entries,
prices and unlock conditions.

## Connection between buildings

When a purchased building is placed against an existing building, the current
prototype:

1. magnetically aligns a compatible wall of the moving building with a wall of
   the existing building when it enters the 350 cm capture range;
2. permits only that intended common-wall overlap during placement validation;
3. detects compatible overlapping modular wall pairs;
4. presents every valid wall module as a door position;
5. snaps a native doorway preview to the candidate nearest the mouse;
6. confirms with left click or cancels with right click / `Escape`;
7. removes the two superposed EBS wall modules on the server;
8. creates an open replicated communication doorway;
9. hides removed level walls on every connected or late-joining client;
10. saves removed walls and doorway transforms.

Positions are intentionally discrete because EBS walls are modular meshes.
Freeform cutting inside a single wall mesh is outside this prototype. The
native white frame is a functional placeholder for the final doorway model.

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

## Data-driven item catalogue

The active catalogue is
`/Game/Botanicus/Data/DA_BotanicusItemCatalog`. It is selected in
`Project Settings > Game > Botanicus Item Catalog` and contains one editable
entry per inventory, delivery, equipment or decoration item. Adding a normal
content item no longer requires a C++ change.

Each entry defines:

- a stable `Item Key`, display name, category and UI icon;
- its world static mesh, scale and optional custom world actor class;
- maximum hotbar stack size;
- weight class: `Hotbar`, `One Player Carry` or `Two Player Carry`;
- purchase price and quantity delivered per parcel;
- allowed placement surfaces: floor, table and/or wall;
- normal and precision rotation steps;
- whether alignment guides are enabled and their edge/angle tolerances;
- an optional collision half-extent override for meshes whose automatic bounds
  do not represent their usable footprint.

`SeedPacket_Test` and `LargeEquipment_Test` are already present and native
fallback definitions keep tests operational even if the catalogue asset is
temporarily unavailable. The hotbar now fills existing compatible stacks up to
the catalogue maximum before using another slot, and refuses a delivery
atomically if the complete quantity cannot fit. Runtime actors load their mesh,
scale and display name from the same definition. Catalogue weight and placement
rules are validated again by the server.

## Equipment delivery prototype

The temporary top-down toolbar exposes `COMMANDER COLIS TEST`. The authoritative
server creates or reuses the nearest replicated delivery pad and places a small
replicated parcel on it. Back in first person, a player must be close, face the
parcel, look at it within a 22-degree camera cone and have an unobstructed line
of sight before pressing `E`. The server validates the same conditions before
the parcel is destroyed and its item is added to that player's owner-only
hotbar. A full hotbar leaves the parcel in place.

Delivery parcels show a yellow `[ E ]` world indicator only while the local
player is within range and looking toward them.

The test parcel currently contains ten units of `SeedPacket_Test` so it can
reuse the validated Item Data entry and demonstrate quantity handling. With
the slot selected, `A` opens a local ground-placement preview. The mouse wheel
rotates it in 5-degree steps (`Shift` gives 1-degree steps), green guide lines
show nearby item alignment, left click asks the server to place it, and right
click or `Escape` cancels without consuming anything. The server verifies the
exact private slot instance and removes one unit only after the replicated
world actor is created successfully. Placed small items are included in world
autosave. This placeholder flow establishes the boundary between shared world
deliveries and private player inventories. Catalogue prices, delivery delays,
final art and two-player carrying remain later iterations.

The temporary toolbar exposes `OBJET LOURD SOLO` and
`OBJET LOURD A DEUX`. They create separate replicated placeholders on the same
delivery pad so both weight classes can be tested. Neither enters the hotbar.
Holding `E` fills a small circular progress indicator before
the object can be lifted. An item marked `One Player Carry` activates after one
player completes the hold. For a `Two Player Carry` item, a first player
completes the hold to request help and a second nearby player must complete the
same hold on that object to activate placement. Both players must then keep `E`
held and remain within seven metres of each other. Releasing `E`, moving too far
away or cancelling restores the object to its original transform. The first
player controls and confirms the preview.
The equipment is never displayed in front of the character: its ground preview
appears directly.
Active carriers cannot jump and receive a replicated animation role: `Solo`,
`Primary` or `Helper`. The character Blueprint event
`On Equipment Carry State Changed` is the integration point for final carrying
poses and animations. The role returns to `None` after placement or
cancellation.
The mouse wheel rotates it in 5-degree steps, or 1-degree steps while holding
`Shift`. Left click confirms a valid position; right click or `Escape` restores
the object to the position from which it was taken. Green and red overlay
materials show placement validity. Green ground lines appear only when nearby
objects have the same orientation within 2 degrees and a pair of their actual
parallel edges is aligned within 5 cm. The line follows the matching edge
instead of comparing object centres and never magnetically changes the preview
position. Floor slope, range and overlaps with world
geometry, other equipment and players are validated locally and again by the
authoritative server. Only one pair can reserve a given object, and one player
can participate in only one heavy-object move at a time. Items configured as
`One Player Carry` retain the same placement system without requiring a helper.
Final hand sockets, carrying animations and movement penalties remain later
refinements.

Autosave version 3 persists every uncollected parcel and large equipment actor
with its exact class, world position, rotation, scale, item identifier and
quantity. Loading happens only on the authoritative server, which removes any
temporary level copies before spawning the saved actors so clients receive one
replicated copy without duplicates. An object that was being carried when the
session ended is restored as an uncarried world object at its last world
transform.

## Multiplayer authority

Building purchase, placement, rotation, door creation and ownership must be
server-authoritative. A building always moves with all of its contents. No API
should expose individual structural-piece destruction or movement to players.

## Rebuilding the bridge

`Tools/IntegrateEBS.py` recreates or repairs the Botanicus integration Blueprints
without modifying the marketplace source assets.
