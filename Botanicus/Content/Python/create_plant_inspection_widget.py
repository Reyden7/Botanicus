"""Import plant inspection art and create its editable Widget Blueprint."""

import os
import unreal


SOURCE_DIR = os.path.join(unreal.Paths.project_saved_dir(), "PlantInspectionUiImport")
TEXTURE_DIR = "/Game/Botanicus/UI/Plant/Inspection/Textures"
WIDGET_DIR = "/Game/Botanicus/UI/Plant/Inspection"
WIDGET_PATH = WIDGET_DIR + "/WBP_PlantInspection"
TEXTURES = {
    "T_PlantInspect_QualityBackground": "background_qualite.png",
    "T_PlantInspect_PriceBackground": "background_prix.png",
    "T_PlantElement_Shadow": "element_tenebres.png",
    "T_PlantElement_Fire": "element_feu.png",
    "T_PlantElement_Normal": "element_normal.png",
    "T_PlantElement_Water": "element_eau.png",
    "T_PlantElement_Ice": "element_glace.png",
}


unreal.EditorAssetLibrary.make_directory(TEXTURE_DIR)
for asset_name, filename in TEXTURES.items():
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SOURCE_DIR, filename)
    task.destination_path = TEXTURE_DIR
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.EditorAssetLibrary.load_asset(TEXTURE_DIR + "/" + asset_name)
    if not texture:
        raise RuntimeError("Unable to import " + asset_name)
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
    )
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
    )
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, False)

if not unreal.EditorAssetLibrary.does_asset_exist(WIDGET_PATH):
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property(
        "parent_class", unreal.BotanicusPlantInspectionWidget.static_class()
    )
    widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "WBP_PlantInspection", WIDGET_DIR, unreal.WidgetBlueprint, factory
    )
    if not widget:
        raise RuntimeError("Unable to create " + WIDGET_PATH)
else:
    widget = unreal.EditorAssetLibrary.load_asset(WIDGET_PATH)

if not widget or not unreal.BotanicusHudEditorLibrary.build_editable_hud_element(
    widget, "PlantInspection"
):
    raise RuntimeError("Unable to rebuild " + WIDGET_PATH)
unreal.EditorAssetLibrary.save_loaded_asset(widget, False)

unreal.log("[Botanicus Plant UI] Editable inspection widget created")
