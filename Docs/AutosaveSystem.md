# Botanicus autosave

Botanicus uses a native server-authoritative autosave, independently of the
Easy Building System demonstration save buttons.

## Current persisted data

- transforms of every EBS building actor in the current map;
- every confirmed spline path and all of its control points;
- each connected player's ten quickbar slots;
- quantities, distinct item instance IDs and selected slot.
- each player's credit balance;
- each player's building development level;
- pending catalogue orders, charged price, quantity and remaining delay;
- uncollected parcels, heavy equipment and placed small items;
- runtime-purchased buildings and communication doors.

The server writes the save automatically when the game world ends, including
when PIE is stopped. It also captures a player's inventory and economy before
that player disconnects. The save is loaded automatically when the same map
starts again. Pending delivery timers resume from their saved remaining
duration; server downtime does not consume delivery time.

Each map uses its own slot:

`Botanicus_Autosave_<MapName>`

In multiplayer, building state is stored on the host/server. Player inventories
and economies use the platform Unique Net ID when available, with a local
topology slot for PIE editor testing (`PIELocal_0`, `PIEClient_0`, etc.). PIE
deliberately ignores both its synthetic Unique Net ID and `PlayerId`, because
Unreal regenerates the former and keeps incrementing the latter across Play
sessions.

Save format version 6 adds placed-pot soil, species, water and growth state.
Version 5 adds the owner-only building development level. Version 4
saves retain their balance and pending catalogue orders and migrate at
development level 1. If a version-4 world already contains a confirmed compact
greenhouse, that achievement migrates to level 2 with the normal 500-credit
level reward. Version 3 saves remain compatible: existing world and
inventory data loads normally, while players without an economy entry receive
the configured starting balance.
