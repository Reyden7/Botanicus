# Botanicus autosave

Botanicus uses a native server-authoritative autosave, independently of the
Easy Building System demonstration save buttons.

## Current persisted data

- transforms of every EBS building actor in the current map;
- every confirmed spline path and all of its control points;
- each connected player's ten quickbar slots;
- quantities, distinct item instance IDs and selected slot.

The server writes the save automatically when the game world ends, including
when PIE is stopped. It also captures a player's inventory before that player
disconnects. The save is loaded automatically when the same map starts again.

Each map uses its own slot:

`Botanicus_Autosave_<MapName>`

In multiplayer, building state is stored on the host/server. Player inventories
use the platform Unique Net ID when available, with a local player ID fallback
for editor testing.

Runtime-spawned purchased buildings will require a persistent building ID and
class asset reference before they can be recreated after loading. The current
prototype restores map-placed building actors.
