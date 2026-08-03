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
    "SeedPacket_Orchid",
    "SeedPacket_Monstera",
    "SeedPacket_Lavender",
    "SeedPacket_Violet",
    "WateringCan",
    "WaterReserve",
    "GardenTrowel",
    "BoxCutter",
    "Harvest_Basil",
    "Harvest_Orchid",
    "Harvest_Monstera",
    "Harvest_Lavender",
    "Harvest_Violet",
    "SalesDisplay",
    "SalePot",
    "PreparationWorkbench",
    "WorkSurfaceSmall",
    "WorkSurfaceMedium",
    "WorkSurfaceLarge",
    "StorageShelfFloorSmall",
    "StorageShelfFloorLarge",
    "StorageShelfWallSmall",
    "StorageShelfWallLarge",
    "CommandComputer",
    "CashRegister",
    "SelfCheckout",
):
    if required_key not in items:
        raise RuntimeError(f"Missing item definition {required_key}")
    if required_key in (
        "PreparationWorkbench",
        "WorkSurfaceSmall",
        "WorkSurfaceMedium",
        "WorkSurfaceLarge",
        "StorageShelfFloorSmall",
        "StorageShelfFloorLarge",
        "StorageShelfWallSmall",
        "StorageShelfWallLarge",
        "CommandComputer",
        "CashRegister",
        "SelfCheckout",
    ):
        continue
    item_data = unreal.load_asset(
        f"/Game/Botanicus/Items/DA_{required_key}"
    )
    if item_data is None:
        raise RuntimeError(f"Missing ItemData asset for {required_key}")

for starter_fixture_key in (
    "PreparationWorkbench",
    "CommandComputer",
    "CashRegister",
):
    if items[starter_fixture_key].get_editor_property("purchasable"):
        raise RuntimeError(
            f"{starter_fixture_key} must be a free starter fixture"
        )

for storage_shelf_key in (
    "StorageShelfFloorSmall",
    "StorageShelfFloorLarge",
    "StorageShelfWallSmall",
    "StorageShelfWallLarge",
):
    shelf = items[storage_shelf_key]
    if (
        shelf.get_editor_property("weight_class")
        != unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY
    ):
        raise RuntimeError(
            f"{storage_shelf_key} must be movable by one player"
        )
    expected_surface = (
        4 if "Wall" in storage_shelf_key else 1
    )
    if (
        shelf.get_editor_property("allowed_placement_surfaces")
        & expected_surface
        == 0
    ):
        raise RuntimeError(
            f"{storage_shelf_key} has an invalid placement surface"
        )

work_surface_expectations = {
    "WorkSurfaceSmall": ((1.2, 0.6, 0.9), 180),
    "WorkSurfaceMedium": ((2.0, 0.7, 0.9), 280),
    "WorkSurfaceLarge": ((3.0, 0.8, 0.9), 420),
}
for surface_key, (expected_scale, expected_price) in (
    work_surface_expectations.items()
):
    surface = items[surface_key]
    if (
        surface.get_editor_property("weight_class")
        != unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY
    ):
        raise RuntimeError(f"{surface_key} must be movable furniture")
    if int(surface.get_editor_property("allowed_placement_surfaces")) != 1:
        raise RuntimeError(f"{surface_key} must be floor-placeable")
    if surface.get_editor_property("price") != expected_price:
        raise RuntimeError(f"{surface_key} price is invalid")
    scale = surface.get_editor_property("world_scale")
    if any(
        abs(actual - expected) > 0.001
        for actual, expected in zip(
            (scale.x, scale.y, scale.z),
            expected_scale,
        )
    ):
        raise RuntimeError(f"{surface_key} dimensions are invalid")
    surface_class = surface.get_editor_property("world_actor_class")
    if (
        surface_class is None
        or "BotanicusWorkSurfaceActor" not in str(surface_class)
    ):
        raise RuntimeError(
            f"{surface_key} does not resolve to BotanicusWorkSurfaceActor"
        )

for base_harvest_key in (
    "Harvest_Basil",
    "Harvest_Orchid",
    "Harvest_Monstera",
    "Harvest_Lavender",
    "Harvest_Violet",
):
    for quality_suffix in ("Beautiful", "Exceptional"):
        quality_key = f"{base_harvest_key}_{quality_suffix}"
        if quality_key not in items:
            raise RuntimeError(
                f"Missing quality harvest definition {quality_key}"
            )
        if unreal.load_asset(
            f"/Game/Botanicus/Items/DA_{quality_key}"
        ) is None:
            raise RuntimeError(
                f"Missing quality ItemData asset {quality_key}"
            )

pot_class = items["PlantPot"].get_editor_property("world_actor_class")
if pot_class is None or "BotanicusPlantPotActor" not in str(pot_class):
    raise RuntimeError("PlantPot does not resolve to BotanicusPlantPotActor")

watering_class = items["WateringCan"].get_editor_property(
    "world_actor_class"
)
if (
    watering_class is None
    or "BotanicusWateringCanActor" not in str(watering_class)
):
    raise RuntimeError(
        "WateringCan does not resolve to BotanicusWateringCanActor"
    )
if (
    items["WateringCan"].get_editor_property("weight_class")
    != unreal.BotanicusItemWeightClass.HANDHELD
):
    raise RuntimeError("WateringCan must be a physical handheld tool")
if not items["PottingSoil"].get_editor_property("sale_soil"):
    raise RuntimeError("PottingSoil must be compatible with sale pots")
if int(
    items["PottingSoil"].get_editor_property(
        "allowed_placement_surfaces"
    )
) != 1:
    raise RuntimeError("PottingSoil must only be placeable on the floor")

tab_expectations = {
    "SeedPacket_Basil": 1,
    "SeedPacket_Orchid": 1,
    "SeedPacket_Monstera": 1,
    "SeedPacket_Lavender": 1,
    "SeedPacket_Violet": 1,
    "WateringCan": 2,
    "GardenTrowel": 2,
    "BoxCutter": 2,
    "PlantPot": 4,
    "SalePot": 8,
    "SalesDisplay": 8,
    "PreparationWorkbench": 4,
    "WorkSurfaceSmall": 4,
    "WorkSurfaceMedium": 4,
    "WorkSurfaceLarge": 4,
    "StorageShelfFloorSmall": 4,
    "StorageShelfFloorLarge": 4,
    "StorageShelfWallSmall": 4,
    "StorageShelfWallLarge": 4,
    "SelfCheckout": 8,
}
for item_key, required_tab in tab_expectations.items():
    tabs = items[item_key].get_editor_property("catalog_tabs")
    if tabs & required_tab == 0:
        raise RuntimeError(
            f"{item_key} is missing command-panel tab {required_tab}"
        )
soil_tabs = items["PottingSoil"].get_editor_property("catalog_tabs")
if soil_tabs & 4 == 0 or soil_tabs & 8 == 0:
    raise RuntimeError(
        "PottingSoil must appear in preparation and sales tabs"
    )

reserve_class = items["WaterReserve"].get_editor_property(
    "world_actor_class"
)
if (
    reserve_class is None
    or "BotanicusWaterReserveActor" not in str(reserve_class)
):
    raise RuntimeError(
        "WaterReserve does not resolve to BotanicusWaterReserveActor"
    )

sale_pot_class = items["SalePot"].get_editor_property(
    "world_actor_class"
)
if (
    sale_pot_class is None
    or "BotanicusSalePotActor" not in str(sale_pot_class)
):
    raise RuntimeError("SalePot does not resolve to BotanicusSalePotActor")
if items["SalePot"].get_editor_property("price") != 10:
    raise RuntimeError("SalePot price must be 10 credits")

self_checkout = items["SelfCheckout"]
self_checkout_class = self_checkout.get_editor_property(
    "world_actor_class"
)
if (
    self_checkout_class is None
    or "BotanicusSelfCheckoutActor" not in str(self_checkout_class)
):
    raise RuntimeError(
        "SelfCheckout does not resolve to BotanicusSelfCheckoutActor"
    )
if self_checkout.get_editor_property("price") != 1500:
    raise RuntimeError("SelfCheckout price must be 1500 credits")
self_checkout_scale = self_checkout.get_editor_property("world_scale")
if (
    abs(self_checkout_scale.y - 0.4) > 0.001
    or abs(self_checkout_scale.z - 2.0) > 0.001
):
    raise RuntimeError(
        "SelfCheckout must be 40 cm wide and 2 metres high"
    )

workbench_class = items["PreparationWorkbench"].get_editor_property(
    "world_actor_class"
)
if (
    workbench_class is None
    or "BotanicusPreparationWorkbenchActor" not in str(workbench_class)
):
    raise RuntimeError(
        "PreparationWorkbench does not resolve to "
        "BotanicusPreparationWorkbenchActor"
    )

plants = plant_catalog.get_editor_property("plants")
if len(plants) != 5:
    raise RuntimeError("Expected five playable plant varieties")
plants_by_key = {
    str(plant.get_editor_property("plant_key")): plant
    for plant in plants
}
expected_plants = {
    "Basil": {
        "seed": "SeedPacket_Basil",
        "harvest": "Harvest_Basil",
        "growth": 120.0,
        "water": (0.25, 0.90),
        "sale": 60,
        "colour": "Green",
        "type": "Aromatic",
        "appeal": 62,
    },
    "Orchid": {
        "seed": "SeedPacket_Orchid",
        "harvest": "Harvest_Orchid",
        "growth": 240.0,
        "water": (0.45, 0.75),
        "sale": 110,
        "colour": "Pink",
        "type": "Flowering",
        "appeal": 82,
    },
    "Monstera": {
        "seed": "SeedPacket_Monstera",
        "harvest": "Harvest_Monstera",
        "growth": 300.0,
        "water": (0.30, 0.82),
        "sale": 95,
        "colour": "Green",
        "type": "Foliage",
        "appeal": 72,
    },
    "Lavender": {
        "seed": "SeedPacket_Lavender",
        "harvest": "Harvest_Lavender",
        "growth": 180.0,
        "water": (0.18, 0.62),
        "sale": 75,
        "colour": "Purple",
        "type": "Aromatic",
        "appeal": 74,
    },
    "Violet": {
        "seed": "SeedPacket_Violet",
        "harvest": "Harvest_Violet",
        "growth": 210.0,
        "water": (0.35, 0.70),
        "sale": 85,
        "colour": "Purple",
        "type": "Flowering",
        "appeal": 78,
    },
}
for plant_key, expected in expected_plants.items():
    if plant_key not in plants_by_key:
        raise RuntimeError(f"Missing plant definition {plant_key}")
    plant = plants_by_key[plant_key]
    if str(plant.get_editor_property("seed_item_key")) != expected["seed"]:
        raise RuntimeError(f"{plant_key} seed mapping is invalid")
    if str(plant.get_editor_property("harvest_item_key")) != expected["harvest"]:
        raise RuntimeError(f"{plant_key} harvest mapping is invalid")
    if str(plant.get_editor_property("harvest_tool_item_key")) != "GardenTrowel":
        raise RuntimeError(f"{plant_key} must use the garden trowel")
    if str(plant.get_editor_property("compatible_sale_soil_item_key")) != "PottingSoil":
        raise RuntimeError(f"{plant_key} sale soil mapping is invalid")
    if plant.get_editor_property("harvest_duration_seconds") != 1.0:
        raise RuntimeError(f"{plant_key} harvest duration must be one second")
    if plant.get_editor_property("harvest_quantity") != 1:
        raise RuntimeError(f"{plant_key} must yield one whole plant")
    if plant.get_editor_property("growth_duration_seconds") != expected["growth"]:
        raise RuntimeError(f"{plant_key} growth duration is invalid")
    minimum_water = plant.get_editor_property("minimum_healthy_water")
    maximum_water = plant.get_editor_property("maximum_healthy_water")
    expected_minimum_water, expected_maximum_water = expected["water"]
    if (
        abs(minimum_water - expected_minimum_water) > 0.0001
        or abs(maximum_water - expected_maximum_water) > 0.0001
    ):
        raise RuntimeError(f"{plant_key} healthy-water range is invalid")

    harvest = items[expected["harvest"]]
    if harvest.get_editor_property("purchasable"):
        raise RuntimeError(f"{expected['harvest']} must not be purchasable")
    if not harvest.get_editor_property("whole_plant"):
        raise RuntimeError(f"{expected['harvest']} must be a whole plant")
    if harvest.get_editor_property("maximum_stack") != 1:
        raise RuntimeError(f"{expected['harvest']} must use one hotbar slot")
    if harvest.get_editor_property("sale_price") != expected["sale"]:
        raise RuntimeError(f"{expected['harvest']} sale price is invalid")
    if str(harvest.get_editor_property("plant_color_tag")) != expected["colour"]:
        raise RuntimeError(f"{expected['harvest']} colour tag is invalid")
    if str(harvest.get_editor_property("plant_type_tag")) != expected["type"]:
        raise RuntimeError(f"{expected['harvest']} type tag is invalid")
    if harvest.get_editor_property("visitor_appeal") != expected["appeal"]:
        raise RuntimeError(f"{expected['harvest']} visitor appeal is invalid")
    if str(harvest.get_editor_property("plant_quality_tag")) != "Standard":
        raise RuntimeError(f"{expected['harvest']} quality must be Standard")
    for quality_suffix, multiplier, appeal_bonus in (
        ("Beautiful", 1.35, 10),
        ("Exceptional", 1.75, 22),
    ):
        quality_key = f"{expected['harvest']}_{quality_suffix}"
        quality_harvest = items[quality_key]
        expected_price = int(expected["sale"] * multiplier + 0.5)
        if quality_harvest.get_editor_property("sale_price") != expected_price:
            raise RuntimeError(f"{quality_key} sale price is invalid")
        if (
            str(quality_harvest.get_editor_property("plant_quality_tag"))
            != quality_suffix
        ):
            raise RuntimeError(f"{quality_key} quality tag is invalid")
        expected_appeal = min(100, expected["appeal"] + appeal_bonus)
        if (
            quality_harvest.get_editor_property("visitor_appeal")
            != expected_appeal
        ):
            raise RuntimeError(f"{quality_key} visitor appeal is invalid")
display_class = items["SalesDisplay"].get_editor_property(
    "world_actor_class"
)
if (
    display_class is None
    or "BotanicusSalesDisplayActor" not in str(display_class)
):
    raise RuntimeError(
        "SalesDisplay does not resolve to BotanicusSalesDisplayActor"
    )
for runtime_class_name in (
    "BotanicusGameState",
    "BotanicusSharedFundsWidget",
    "BotanicusVisitorCharacter",
    "BotanicusVisitorManager",
    "BotanicusVisitorZoneActor",
    "BotanicusRefundZoneActor",
    "BotanicusShopObjectivesWidget",
    "BotanicusDaySummaryWidget",
    "BotanicusClockWidget",
    "BotanicusVisitorSpeechBubbleWidget",
    "BotanicusPreparationWorkbenchActor",
    "BotanicusWorkSurfaceActor",
    "BotanicusStorageShelfActor",
    "BotanicusComputerActor",
    "BotanicusCashRegisterActor",
    "BotanicusSelfCheckoutActor",
    "BotanicusDevelopmentPanelWidget",
):
    runtime_class = unreal.load_class(
        None,
        f"/Script/Botanicus.{runtime_class_name}",
    )
    if runtime_class is None:
        raise RuntimeError(
            f"Missing runtime class {runtime_class_name}"
        )
unreal.log(
    "[Botanicus Growing Validation] "
    "quality, trends, cutter and physical-delivery runtime classes: OK"
)
unreal.SystemLibrary.quit_editor()
