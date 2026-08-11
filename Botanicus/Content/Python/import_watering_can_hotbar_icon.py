import os
import unreal


SOURCE_FILE = os.path.join(
    unreal.Paths.project_saved_dir(),
    "WateringCanUiImport",
    "arosoir.png",
)
DESTINATION_PATH = "/Game/Botanicus/UI/Tools"
TEXTURE_NAME = "T_WateringCan_Hotbar"
TEXTURE_PATH = DESTINATION_PATH + "/" + TEXTURE_NAME
ITEM_DATA_PATH = "/Game/Botanicus/Items/DA_WateringCan"
CATALOG_PATH = "/Game/Botanicus/Data/DA_BotanicusItemCatalog"


def import_texture():
    existing = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
    if existing:
        return existing

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", SOURCE_FILE)
    task.set_editor_property("destination_path", DESTINATION_PATH)
    task.set_editor_property("destination_name", TEXTURE_NAME)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    return unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)


def main():
    if not os.path.isfile(SOURCE_FILE):
        raise RuntimeError("Missing watering can icon: " + SOURCE_FILE)

    texture = import_texture()
    if not texture:
        raise RuntimeError("The watering can texture could not be imported")

    try:
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("srgb", True)
    except Exception as error:
        unreal.log_warning("Could not apply every UI texture setting: {}".format(error))
    texture.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)

    item_data = unreal.EditorAssetLibrary.load_asset(ITEM_DATA_PATH)
    if not item_data:
        raise RuntimeError("Missing item data asset: " + ITEM_DATA_PATH)
    item_data.set_editor_property("item_icon", texture)
    item_data.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(item_data, False)

    catalog = unreal.EditorAssetLibrary.load_asset(CATALOG_PATH)
    if not catalog:
        raise RuntimeError("Missing item catalog: " + CATALOG_PATH)
    items = list(catalog.get_editor_property("items"))
    watering_can_found = False
    for definition in items:
        if str(definition.get_editor_property("item_key")) != "WateringCan":
            continue
        definition.set_editor_property(
            "weight_class",
            unreal.BotanicusItemWeightClass.HOTBAR,
        )
        definition.set_editor_property("maximum_stack", 1)
        definition.set_editor_property("allowed_placement_surfaces", 1)
        definition.set_editor_property("icon", texture)
        watering_can_found = True
        break
    if not watering_can_found:
        raise RuntimeError("WateringCan is absent from the item catalog")
    catalog.set_editor_property("items", items)
    catalog.modify()
    unreal.EditorAssetLibrary.save_loaded_asset(catalog, False)

    unreal.log("Watering can hotbar data and icon updated successfully.")


main()
