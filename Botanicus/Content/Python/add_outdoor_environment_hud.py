"""Import climate icons and add the editable outdoor environment HUD element."""

import os
import unreal


SOURCE_DIRECTORY = r"C:\Users\quent\Documents\Botanicus\image du jeu\UI"
TEXTURE_DIRECTORY = "/Game/Botanicus/UI/HUD/Textures"
ELEMENT_DIRECTORY = "/Game/Botanicus/UI/HUD/Elements"
ELEMENT_PATH = ELEMENT_DIRECTORY + "/WBP_HUD_OutdoorEnvironment"
MASTER_PATH = "/Game/Botanicus/UI/HUD/WBP_BotanicusHUD"

TEXTURES = {
    "T_HUD_Temperature": "temperature.png",
    "T_HUD_AirHumidity": "humidit&é.png",
    "T_HUD_Luminosity": "luminosité.png",
}


def import_texture(asset_name, source_name):
    source_path = os.path.join(SOURCE_DIRECTORY, source_name)
    if not os.path.isfile(source_path):
        raise RuntimeError("Missing outdoor environment icon: " + source_path)
    task = unreal.AssetImportTask()
    task.filename = source_path
    task.destination_path = TEXTURE_DIRECTORY
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture_path = TEXTURE_DIRECTORY + "/" + asset_name
    texture = unreal.EditorAssetLibrary.load_asset(texture_path)
    if not texture:
        raise RuntimeError("Unable to import " + texture_path)
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)


unreal.EditorAssetLibrary.make_directory(TEXTURE_DIRECTORY)
for texture_name, filename in TEXTURES.items():
    import_texture(texture_name, filename)

if unreal.EditorAssetLibrary.does_asset_exist(ELEMENT_PATH):
    unreal.EditorAssetLibrary.delete_asset(ELEMENT_PATH)
factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property(
    "parent_class", unreal.BotanicusOutdoorEnvironmentWidget.static_class()
)
element = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "WBP_HUD_OutdoorEnvironment",
    ELEMENT_DIRECTORY,
    unreal.WidgetBlueprint,
    factory,
)
if not element or not unreal.BotanicusHudEditorLibrary.build_editable_hud_element(
    element, "OutdoorEnvironment"
):
    raise RuntimeError("Unable to create " + ELEMENT_PATH)
unreal.EditorAssetLibrary.save_loaded_asset(element, False)

master = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
if not master or not unreal.BotanicusHudEditorLibrary.add_outdoor_environment_to_layout(
    master
):
    raise RuntimeError("Unable to add outdoor environment slot to " + MASTER_PATH)
unreal.EditorAssetLibrary.save_loaded_asset(master, False)
unreal.EditorLoadingAndSavingUtils.save_dirty_packages(False, True)
unreal.log("[Botanicus HUD] Editable outdoor environment widget added")
