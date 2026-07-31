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
6. Select the watering can and hold the left mouse button on the planted pot.
   Releasing the button immediately stops watering.
7. Keep the water percentage inside the healthy range while the plant grows.

Soil and seeds are consumed one unit at a time. The watering can is a reusable
tool.

## Controls

- Hold `E` for 0.75 seconds while looking at an existing placeable item to
  start moving it. Releasing early or looking away cancels the pickup.
- Left click confirms the new position of a moved or newly placed item.
- Right click cancels placement or movement.
- Left click performs the selected pot action.
- Soil requires a continuous 1.5-second hold and is consumed only on completion.
- A seed is planted and consumed with one click.
- Water is added continuously while left click is held; release stops it.

Movement and pot actions use separate server-validated paths. Moving an
existing pot preserves its soil, seed, water, growth and watering count.

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

Autosave version 6 stores the complete growing state of every placed pot. After
restart, the exact soil, species, water, watering count and growth values are
restored. Growth continues only while the authoritative world is running.

## Generated assets

- `Tools/CreateItemCatalog.py` adds the four order definitions.
- `Scripts/CreatePrototypeItemData.py` creates their Item Data assets.
- `Tools/CreatePlantCatalog.py` creates the plant catalogue.
- `Tools/ValidateGrowingAssets.py` checks keys, class mappings and water bounds.
