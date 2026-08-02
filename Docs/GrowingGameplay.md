# Botanicus growing gameplay

## Playable loop

The first complete crop loop uses the existing order catalogue, deliveries,
private quickbar, placement and authoritative interaction systems.

1. Order and collect a `Pot de culture`.
2. Select the pot in the quickbar and press `A` to place it.
3. Order and collect `Dose de terreau`, `Graines de basilic` and `Arrosoir`.
4. Select the soil, look at the pot and hold the left mouse button for
   1.5 seconds.
5. Select the basil seeds and left-click once on the filled pot.
6. Look at the delivered watering can and press `E` to keep it in hand.
7. Hold the left mouse button on the planted pot. Releasing the button
   immediately stops watering.
8. Keep the water percentage inside the healthy range while the plant grows.
9. Order and collect the `Petite pelle de jardinage`.
10. When the plant reaches 100%, select the trowel and look directly at the pot.
11. Hold left click for one second to collect one complete basil plant.

Soil and seeds are consumed one unit at a time. The watering can is a physical
tool that never enters the hotbar. Its camera-facing label displays its
remaining water percentage. Pressing `E` while carrying it places it in front
of the player.

## Controls

- Hold `E` for 0.75 seconds while looking at an existing placeable item to
  start moving it. Releasing early or looking away cancels the pickup.
- Left click confirms the new position of a moved or newly placed item.
- Right click cancels placement or movement.
- Left click performs the selected pot action.
- Soil requires a continuous 1.5-second hold and is consumed only on completion.
- A seed is planted and consumed with one click.
- Water is added continuously while left click is held; release stops it.
- Watering consumes the held watering can's finite water level.
- The purchasable `Reserve d'eau` costs 250 credits. Place it from the hotbar
  with `A`, hold the watering can, look at the reserve and left-click to refill
  it to 100%. The prototype reserve is a permanent source.
- A mature plant can only be harvested with its configured harvest tool.
- Harvest requires one uninterrupted second of left-click hold.
- Looking away, moving out of range, changing tools or releasing early cancels
  the harvest without modifying the plant.
- A full hotbar also refuses the harvest without destroying the mature plant.

Movement and pot actions use separate server-validated paths. Moving an
existing pot preserves its soil, seed, water, growth and watering count.

When the correct tool is selected and the player is close enough while looking
at a mature pot, a green world-space action appears:
`UTILISER PETITE PELLE POUR RECOLTER BASILIC`.

Every placed pot permanently displays a camera-facing debug label:

- soil present (`TERREAU : OUI/NON`);
- seed present (`GRAINE : OUI/NON`);
- number of watering-can uses;
- current water and growth percentages.
- current held action and soil-filling progress.

## Basil prototype parameters

- growth duration under healthy conditions: 120 seconds;
- healthy water range: 25% to 90%;
- water per watering-can use: 35%;
- water consumption: 0.3% per second;
- growth pauses when the pot is too dry or overwatered.

All values live in
`/Game/Botanicus/Data/DA_BotanicusPlantCatalog`. Adding another plant requires
a new plant definition and a seed item whose key matches `SeedItemKey`; the pot
interaction does not contain species-specific code.

## Multiplayer and persistence

Every mutation runs on the server. Soil, planted species, water, watering count
and growth are replicated to all clients. The visual stem and foliage scale
from replicated growth progress.

Autosave version 8 stores the complete growing state of every placed pot and
the remaining water in every physical watering can. After
restart, the exact soil, species, water, watering count and growth values are
restored. Growth continues only while the authoritative world is running.

## Harvest prototype

- required tool: `GardenTrowel`;
- produced item: `Harvest_Basil`;
- quantity: 1 complete plant;
- uninterrupted hold duration: 1 second;
- the trowel is reusable;
- the complete harvested basil plant is inventory-only and cannot be ordered
  from the catalogue;
- soil remains in the pot so another seed can be planted after harvest.

## Sales display prototype

1. Order and collect a `Presentoir de vente`.
2. Select it in the hotbar and press `A` to start placement.
3. Confirm its position with left click.
4. In the Preparation tab, order an `Etabli de preparation` for 350 credits.
5. Carry and place the delivered workbench, then order a `Pot de vente` for
   10 credits.
6. Place the sale pot near the workbench: its preview snaps onto the free
   preparation slot.
7. Select compatible soil and hold left click on the sale pot for 1.5 seconds.
8. Select one complete harvested plant and left-click the filled sale pot.
9. Hold `E` while looking at the prepared pot to move it.
10. Position its green preview on an empty sales display and confirm with left
   click.

A whole plant can no longer be placed directly on a sales display. The server
validates the workbench slot, sale pot, soil compatibility, complete plant,
empty display and seller. A server-side manager maintains the visitor
population configured by the main shop level. They use a dedicated visitor
circuit instead of wandering freely:

1. In the top-down view, place one `PARKING PNJ` zone outside the building.
2. Place one `ESPACE VENTE PNJ` zone over the room that visitors may browse.
3. Place one `CAISSE PNJ` zone around the checkout.
4. Select `ROUTE PNJ`, then draw a continuous circuit whose first point is
   inside the parking zone, whose intermediate points enter the sales zone
   through the building openings, pass through the checkout zone, then return
   to the parking.
5. The final point must also be inside the parking zone. Add several route
   points inside the sales area to define the exact visit path, then validate
   the circuit.

A visitor follows this route from the parking and only searches for sale
displays while inside the sales zone. Depending on the number of plants
available, each visitor compares between two and four displays. Every
inspection lasts between three and five-and-a-half seconds and displays a
comic-style reaction bubble about the plant or its colour. Rejected plants are
released for another visitor.

After making a choice, the visitor physically carries a sale pot and plant
from the display to the checkout. The visitor pauses at the checkout for 1.8
seconds, pays, then continues to the parking while still visibly carrying the
purchase. The display is cleared and the shared wallet is credited only after
this checkout pause.

Each visitor now has a preferred colour, preferred plant family, maximum
budget and personal minimum quality score. Displayed plants expose matching
catalogue attributes. Reactions mention the real mismatch (colour, family or
price), visitors remember the best acceptable plant while comparing, and may
leave without buying when no available plant satisfies their profile. The
prototype basil is tagged `Green`, `Aromatic`, with visitor appeal 62.

## Plant care quality

Quality rewards precise watering without adding a survival penalty. Growth
still pauses outside the plant's healthy water range. While it grows, the pot
also measures how close its water level remains to the centre of that range:

- `Standard`: care rating below 50%;
- `Beautiful`: care rating from 50% to 79%;
- `Exceptional`: care rating from 80%.

The estimated quality is visible on the growing pot and can improve throughout
growth. Harvesting produces a quality-specific whole-plant inventory item, so
the result survives inventory persistence, sale-pot preparation and display
placement. Beautiful plants sell for 135% of their base price and gain 10
visitor-appeal points. Exceptional plants sell for 175% and gain 22 appeal
points. Visitors with enough budget explicitly react to these better plants.

The route cannot be validated through a wall or blocking obstacle. Visitors
ignore Pawn collision, so players and other visitors can pass through them,
while walls, floors and furniture remain solid. A visitor that is physically
stuck for five seconds is safely removed and replaced by the manager.

## Main shop level and visitor population

The blue `BOUTIQUE PRINCIPALE` row appears first in the Buildings tab and now
only contains the shop level, visitor capacities, upgrade price and upgrade
button. Its level is shared by every player and controls the number of
customers admitted inside:

- level 1: 4 customers inside, 16 visitor NPCs in the level;
- level 2: 10 customers inside, 28 visitor NPCs in the level;
- level 3: 20 customers inside, 45 visitor NPCs in the level;
- later levels add 10 indoor places and 15 total visitors per level.

Only customers following the indoor visit circuit consume shop capacity. The
next six visitors form an ordered queue at the route entrance. Additional
visitors wait throughout the parking zone and progressively join the queue as
places become available. All visitor NPCs continue to ignore Pawn collision.

The command panel contains a shared green/red shop-state button. Closing the
shop prevents the manager from spawning or admitting anyone. Visitors already
browsing follow the authored circuit toward the exit, while customers still
approaching, queuing or waiting in the parking area reverse their arrival
route naturally. Reserved but unpaid plants are released without recording a
negative review. Reopening the shop restarts the randomized visitor flow. The
server owns and replicates this state, and autosave version 17 restores it
after a restart.

An upgrade requires both the displayed amount of shared money and all listed
tasks. The prototype tasks track cumulative plants sold and catalogue orders
placed. They are displayed in a collapsible orange list on the right side of
the gameplay HUD, below the shared wallet, with a status for every step. Their
progress, the shop level and the paid upgrade are authoritative, replicated
and persisted by autosave version 13.

The command panel contains a development button directly below its money
total. Each click adds 100 credits to the shared wallet through the server and
triggers the normal autosave path.

Prepared sale-pot soil and plant contents are replicated and restored by
autosave. Visitor routes and the three visitor zones are restored by autosave
version 11. Pending occupied displays attract a new visitor after a restart
once a complete valid circuit exists.

Each harvest is one complete plant, occupies its own hotbar slot and remains
unavailable from the order catalogue. The first four varieties are:

- basil: 120 seconds, healthy water from 25% to 90%, sale price 60 credits,
  green aromatic profile;
- pink orchid: 240 seconds, healthy water from 45% to 75%, sale price
  110 credits, premium pink flowering profile;
- monstera: 300 seconds, healthy water from 30% to 82%, sale price 95 credits,
  slow decorative green foliage profile;
- purple lavender: 180 seconds, healthy water from 18% to 62%, sale price
  75 credits, low-consumption purple aromatic profile.

Their color and silhouette are retained in growing pots, prepared sale pots,
sales displays and while carried by visitors. Visitor preference scoring uses
the color, type, appeal and price stored in the item catalogue.

## Satisfaction and shop reputation

The nursery shares a persistent reputation from one to five stars, starting
at three. A successful purchase produces a satisfaction score based on how
well the plant matches the visitor and receives an additional bonus for
Beautiful or Exceptional quality. A visitor who inspected plants but found
nothing leaves a mildly negative review. An empty shop does not receive
negative reviews.

Each review moves reputation gradually, between minus five and plus seven
points on the 100-to-500 scale. Higher reputation gives future visitors a
larger shopping budget and reduces their randomized arrival interval. The
top-right HUD displays filled stars, the exact decimal rating and the last
visitor's satisfaction. Reputation is also an orange shop-upgrade objective:
3.20 stars for level 2, 3.80 for level 3, then progressively up to 4.80.

## Customer trends

Three shared demands rotate every ten minutes: one available colour, one plant
family and one quality level. The top-right HUD displays all three plus the
server-synchronized countdown. Every matching attribute adds 10% to the final
sale price, 10 points to the visitor's selection score and 5 points to final
satisfaction. A plant can therefore reach a 30% trend premium.

Sales displays show how many of the three current demands they satisfy and
their adjusted price. Visitors explicitly comment when a plant matches at
least two trends. The active tags and remaining rotation time persist through
autosave so restarting the host cannot reroll demand early.

## Physical delivery cartons

Every non-building item ordered from the command panel now arrives sealed in a
carton. Hotbar supplies, handheld tools and one- or two-player equipment all
use the same delivery step; physical contents only spawn after unpacking.
Carton dimensions are derived from the item's world scale, weight class and
delivery quantity. The delivery pad uses wider spacing for larger boxes.

Every new player receives one persistent starter `BoxCutter`; existing saves
are checked on every inventory restore and receive one in the first free
quickbar slot when missing. Replacement cutters are available in Gardening
Tools for 35 credits.

A sealed carton can be moved before opening. The player looks at it and holds
`E` for the normal pickup delay, moves its placement preview, rotates it with
the mouse wheel and confirms with left mouse. Right mouse cancels the move.
The server validates floor slope, distance and collisions, prevents two
players from reserving the same carton and autosaves the confirmed position.

The top seam contains 16 bright-red debug tape segments. With the cutter
selected, the player looks at the tape, holds left mouse and moves the aim
along its length. Server traces project every sample into carton-local space.
Only samples inside the tape width count, large jumps are not interpolated,
and opening requires both end segments plus at least 14 of 16 total segments.
Cut sections disappear immediately. The opened carton exposes two lid flaps
and `E` releases its contents; a full hotbar leaves the contents safely inside.

Cut coverage and opened state are authoritative, replicated and persisted.
Cartons from older saves migrate as already open so an existing delivery is
never made inaccessible retroactively.

## Generated assets

- `Tools/CreateItemCatalog.py` adds the nursery order definitions.
- `Tools/CreatePlantVarietyItemData.py` creates the new seed and harvest Item
  Data assets.
- `Tools/CreatePlantCatalog.py` creates the plant catalogue.
- `Tools/ValidateGrowingAssets.py` checks all four varieties, prices, visitor
  attributes, class mappings and water bounds.
