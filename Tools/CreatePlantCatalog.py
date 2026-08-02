"""Create the first data-driven Botanicus plant definition."""

import unreal


ASSET_PATH = "/Game/Botanicus/Data/DA_BotanicusPlantCatalog"

asset = unreal.load_asset(ASSET_PATH)
if asset is None:
    factory = unreal.DataAssetFactory()
    factory.set_editor_property(
        "data_asset_class",
        unreal.BotanicusPlantCatalog,
    )
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_BotanicusPlantCatalog",
        "/Game/Botanicus/Data",
        unreal.BotanicusPlantCatalog,
        factory,
    )
if asset is None:
    raise RuntimeError("Could not create DA_BotanicusPlantCatalog")

def make_plant(
    key,
    seed_key,
    display_name,
    harvest_key,
    growth_duration,
    minimum_water,
    maximum_water,
    water_per_use,
    water_consumption,
    mature_color,
):
    plant = unreal.BotanicusPlantDefinition()
    plant.set_editor_property("plant_key", key)
    plant.set_editor_property("seed_item_key", seed_key)
    plant.set_editor_property("display_name", display_name)
    plant.set_editor_property("harvest_tool_item_key", "GardenTrowel")
    plant.set_editor_property("harvest_item_key", harvest_key)
    plant.set_editor_property(
        "compatible_sale_soil_item_key",
        "PottingSoil",
    )
    plant.set_editor_property("harvest_quantity", 1)
    plant.set_editor_property("harvest_duration_seconds", 1.0)
    plant.set_editor_property(
        "growth_duration_seconds",
        growth_duration,
    )
    plant.set_editor_property(
        "minimum_healthy_water",
        minimum_water,
    )
    plant.set_editor_property(
        "maximum_healthy_water",
        maximum_water,
    )
    plant.set_editor_property("water_added_per_use", water_per_use)
    plant.set_editor_property(
        "water_consumption_per_second",
        water_consumption,
    )
    plant.set_editor_property("mature_color", mature_color)
    return plant


basil = make_plant(
    "Basil",
    "SeedPacket_Basil",
    "Basilic",
    "Harvest_Basil",
    120.0,
    0.25,
    0.90,
    0.35,
    0.0030,
    unreal.LinearColor(0.12, 0.55, 0.08, 1.0),
)
orchid = make_plant(
    "Orchid",
    "SeedPacket_Orchid",
    "Orchidee rose",
    "Harvest_Orchid",
    240.0,
    0.45,
    0.75,
    0.28,
    0.0040,
    unreal.LinearColor(0.95, 0.22, 0.58, 1.0),
)
monstera = make_plant(
    "Monstera",
    "SeedPacket_Monstera",
    "Monstera",
    "Harvest_Monstera",
    300.0,
    0.30,
    0.82,
    0.32,
    0.0020,
    unreal.LinearColor(0.05, 0.42, 0.16, 1.0),
)
lavender = make_plant(
    "Lavender",
    "SeedPacket_Lavender",
    "Lavande violette",
    "Harvest_Lavender",
    180.0,
    0.18,
    0.62,
    0.25,
    0.0015,
    unreal.LinearColor(0.48, 0.20, 0.78, 1.0),
)

asset.set_editor_property(
    "plants",
    [basil, orchid, monstera, lavender],
)
unreal.EditorAssetLibrary.save_loaded_asset(
    asset,
    only_if_is_dirty=False,
)
unreal.log(f"Botanicus plant catalog created: {ASSET_PATH}")
unreal.SystemLibrary.quit_editor()
