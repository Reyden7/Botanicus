"""Import the crosshair and add editable crosshair widgets without rebuilding the HUD."""

import os
import unreal


SOURCE = os.path.join(unreal.Paths.project_saved_dir(), "HudTextureImport", "cross.png")
TEXTURE_DIRECTORY = "/Game/Botanicus/UI/HUD/Textures"
TEXTURE_PATH = TEXTURE_DIRECTORY + "/T_HUD_Crosshair"
ELEMENT_DIRECTORY = "/Game/Botanicus/UI/HUD/Elements"
ELEMENT_PATH = ELEMENT_DIRECTORY + "/WBP_HUD_Crosshair"
MASTER_PATH = "/Game/Botanicus/UI/HUD/WBP_BotanicusHUD"


task = unreal.AssetImportTask()
task.filename = SOURCE
task.destination_path = TEXTURE_DIRECTORY
task.destination_name = "T_HUD_Crosshair"
task.automated = True
task.replace_existing = True
task.replace_existing_settings = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_PATH)
if not texture:
    raise RuntimeError("Unable to import " + TEXTURE_PATH)
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property("srgb", True)
unreal.EditorAssetLibrary.save_loaded_asset(texture, False)

if not unreal.EditorAssetLibrary.does_asset_exist(ELEMENT_PATH):
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.BotanicusCrosshairWidget.static_class())
    element = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WBP_HUD_Crosshair", ELEMENT_DIRECTORY, unreal.WidgetBlueprint, factory
    )
    if not element or not unreal.BotanicusHudEditorLibrary.build_editable_hud_element(
        element, "Crosshair"
    ):
        raise RuntimeError("Unable to create " + ELEMENT_PATH)
    unreal.EditorAssetLibrary.save_loaded_asset(element, False)

master = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
if not master or not unreal.BotanicusHudEditorLibrary.add_crosshair_to_layout(master):
    raise RuntimeError("Unable to add the crosshair slot to " + MASTER_PATH)
unreal.EditorAssetLibrary.save_loaded_asset(master, False)
unreal.log("[Botanicus HUD] Editable crosshair added")
