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

basil = unreal.BotanicusPlantDefinition()
basil.set_editor_property("plant_key", "Basil")
basil.set_editor_property("seed_item_key", "SeedPacket_Basil")
basil.set_editor_property("display_name", "Basilic")
basil.set_editor_property("growth_duration_seconds", 120.0)
basil.set_editor_property("minimum_healthy_water", 0.25)
basil.set_editor_property("maximum_healthy_water", 0.9)
basil.set_editor_property("water_added_per_use", 0.35)
basil.set_editor_property("water_consumption_per_second", 0.003)
basil.set_editor_property(
    "mature_color",
    unreal.LinearColor(0.12, 0.55, 0.08, 1.0),
)

asset.set_editor_property("plants", [basil])
unreal.EditorAssetLibrary.save_loaded_asset(
    asset,
    only_if_is_dirty=False,
)
unreal.log(f"Botanicus plant catalog created: {ASSET_PATH}")
unreal.SystemLibrary.quit_editor()
