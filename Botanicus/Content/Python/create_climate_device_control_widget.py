"""Import the climate panel background and create its editable WBP."""

import unreal


SOURCE = r"C:/Users/quent/Documents/Botanicus/image du jeu/UI/commande/icons_botanicus_separes/bg général.png"
TEXTURE_DIR = "/Game/Botanicus/UI/Command/Textures"
WIDGET_DIR = "/Game/Botanicus/UI/ClimateDevice"
WIDGET_NAME = "WBP_ClimateDeviceControl"
WIDGET_PATH = f"{WIDGET_DIR}/{WIDGET_NAME}"

task = unreal.AssetImportTask()
task.filename = SOURCE
task.destination_path = TEXTURE_DIR
task.destination_name = "T_Command_GeneralBackground"
task.automated = True
task.replace_existing = True
task.save = True
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

texture = unreal.load_asset(f"{TEXTURE_DIR}/T_Command_GeneralBackground")
if not texture:
    raise RuntimeError("Unable to import the climate control background")
texture.set_editor_property(
    "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON
)
texture.set_editor_property(
    "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS
)
texture.set_editor_property("never_stream", True)
unreal.EditorAssetLibrary.save_loaded_asset(texture)

if unreal.EditorAssetLibrary.does_asset_exist(WIDGET_PATH):
    unreal.EditorAssetLibrary.delete_asset(WIDGET_PATH)

factory = unreal.WidgetBlueprintFactory()
factory.set_editor_property(
    "parent_class", unreal.BotanicusClimateDeviceControlWidget.static_class()
)
widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    WIDGET_NAME, WIDGET_DIR, unreal.WidgetBlueprint, factory
)
if not widget:
    raise RuntimeError(f"Unable to create {WIDGET_PATH}")
if not unreal.BotanicusHudEditorLibrary.build_editable_hud_element(
    widget, "ClimateDeviceControl"
):
    raise RuntimeError(f"Unable to build {WIDGET_PATH}")
if not unreal.EditorAssetLibrary.save_loaded_asset(widget, False):
    raise RuntimeError(f"Unable to save {WIDGET_PATH}")

unreal.log(f"Created editable climate device widget: {WIDGET_PATH}")
