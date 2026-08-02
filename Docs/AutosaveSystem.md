# Botanicus autosave

Botanicus uses a native server-authoritative autosave, independently of the
Easy Building System demonstration save buttons.

## Current persisted data

- transforms of every EBS building actor in the current map;
- every confirmed spline path and all of its control points;
- each connected player's ten quickbar slots;
- quantities, distinct item instance IDs and selected slot.
- each player's world transform;
- the nursery's shared credit balance;
- the shared main-shop level, plants sold and catalogue-order counters;
- whether the main shop is currently open or closed;
- the current day number and its in-progress sales, revenue and reviews;
- the shared time of day used by automatic shop hours;
- visitor routes and the parking, sales-area and checkout zones;
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

In multiplayer, building state and the shared wallet are stored on the
host/server. Player inventories, progression and pending orders use the
platform Unique Net ID when available, with a local
topology slot for PIE editor testing (`PIELocal_0`, `PIEClient_0`, etc.). PIE
deliberately ignores both its synthetic Unique Net ID and `PlayerId`, because
Unreal regenerates the former and keeps incrementing the latter across Play
sessions.

Save format version 21 adds the persistent player-only object-refund surface.
Eligible objects deposited entirely on it return 80% of their catalogue price
to the shared wallet. Tools, the manual register, preparation workbench and
command computer are protected. Save format version 20 adds the persistent
preparation-workbench level and its
one-to-five simultaneous pot capacity. Save format version 19 adds the
persistent shared clock and automatic opening
hours. Version 18 adds the persistent day cycle and its current daily
statistics. The development time multiplier is deliberately excluded and
always resets to x1 when the world starts. Version 17 adds the persistent
shared open/closed shop state.
Version 16 adds each delivery carton's cut-tape coverage and open
state, plus the one-time starter-cutter migration. Version 15 adds the three
shared customer trends and their
remaining rotation time. Version 14 adds shared shop reputation, the last visitor
satisfaction and the number of reviews. Version 13 adds the persistent
care-quality score of every growing plant. Version 12 adds the shared
main-shop level and its persistent task
progress. Version 11 adds visitor-route types and the three visitor zones.
Version 10 adds the shared nursery wallet. The first legacy player
wallet is migrated once without adding every old balance together.
Version 9 adds the compatible soil and whole plant stored in each
prepared sale pot. Version 8 adds player transforms and the remaining water level of
physical watering cans. Inventory and economy are also written to disk
immediately during logout, before the pawn is destroyed. Non-hotbar tools from
older saves are rejected during quickbar restoration.
Version 7 adds the complete plant stored on each sales display.
Version 6 adds placed-pot soil, species, water and growth state.
Version 5 adds the owner-only building development level. Version 4
saves retain their balance and pending catalogue orders and migrate at
development level 1. If a version-4 world already contains a confirmed compact
greenhouse, that achievement migrates to level 2 with the normal 500-credit
level reward. Version 3 saves remain compatible: existing world and
inventory data loads normally, while players without an economy entry receive
the configured starting balance.
