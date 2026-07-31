"""Fail fast when the growing gameplay assets are incomplete."""

import unreal


item_catalog = unreal.load_asset(
    "/Game/Botanicus/Data/DA_BotanicusItemCatalog"
)
plant_catalog = unreal.load_asset(
    "/Game/Botanicus/Data/DA_BotanicusPlantCatalog"
)
if item_catalog is None or plant_catalog is None:
    raise RuntimeError("A growing catalogue asset is missing")

items = {
    str(item.get_editor_property("item_key")): item
    for item in item_catalog.get_editor_property("items")
}
for required_key in (
    "PlantPot",
    "PottingSoil",
    "SeedPacket_Basil",
    "WateringCan",
):
    if required_key not in items:
        raise RuntimeError(f"Missing item definition {required_key}")
    item_data = unreal.load_asset(
        f"/Game/Botanicus/Items/DA_{required_key}"
    )
    if item_data is None:
        raise RuntimeError(f"Missing ItemData asset for {required_key}")

pot_class = items["PlantPot"].get_editor_property("world_actor_class")
if pot_class is None or "BotanicusPlantPotActor" not in str(pot_class):
    raise RuntimeError("PlantPot does not resolve to BotanicusPlantPotActor")

plants = plant_catalog.get_editor_property("plants")
if len(plants) != 1:
    raise RuntimeError("Expected exactly one prototype plant")
basil = plants[0]
if str(basil.get_editor_property("seed_item_key")) != "SeedPacket_Basil":
    raise RuntimeError("Basil seed mapping is invalid")
minimum_water = basil.get_editor_property("minimum_healthy_water")
maximum_water = basil.get_editor_property("maximum_healthy_water")
if not 0.0 <= minimum_water < maximum_water <= 1.0:
    raise RuntimeError("Basil healthy-water range is invalid")

unreal.log(
    "[Botanicus Growing Validation] "
    f"4 items, pot class, basil, water "
    f"{minimum_water:.2f}-{maximum_water:.2f}: OK"
)
unreal.SystemLibrary.quit_editor()
