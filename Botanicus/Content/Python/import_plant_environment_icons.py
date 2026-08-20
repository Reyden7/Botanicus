"""Import the six plant climate warning icons as cooked UI textures."""

import os
import unreal


SOURCE_DIR = os.path.join(
    os.path.expanduser("~"),
    "Documents",
    "Botanicus",
    "image du jeu",
    "UI",
    "plante",
)
DESTINATION_DIR = "/Game/Botanicus/UI/Plant/Environment"
WIDGET_PATH = DESTINATION_DIR + "/WBP_PlantEnvironmentAlerts"
TEXTURES = {
    "T_PlantEnvironment_TemperatureLow": "faible chaleur.png",
    "T_PlantEnvironment_TemperatureHigh": "Trop chaleur.png",
    "T_PlantEnvironment_HumidityLow": "faible humidité.png",
    "T_PlantEnvironment_HumidityHigh": "trop humidité.png",
    "T_PlantEnvironment_LuminosityLow": "faible luminosité.png",
    "T_PlantEnvironment_LuminosityHigh": "trop luminosité.png",
}


def main():
    missing = [
        filename
        for filename in TEXTURES.values()
        if not os.path.isfile(os.path.join(SOURCE_DIR, filename))
    ]
    if missing:
        raise RuntimeError("Missing plant environment icons: " + ", ".join(missing))

    unreal.EditorAssetLibrary.make_directory(DESTINATION_DIR)
    for asset_name, filename in TEXTURES.items():
        task = unreal.AssetImportTask()
        task.filename = os.path.join(SOURCE_DIR, filename)
        task.destination_path = DESTINATION_DIR
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.replace_existing_settings = True
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

        texture = unreal.EditorAssetLibrary.load_asset(
            DESTINATION_DIR + "/" + asset_name
        )
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
        texture.modify()
        unreal.EditorAssetLibrary.save_loaded_asset(texture, False)

    if not unreal.EditorAssetLibrary.does_asset_exist(WIDGET_PATH):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property(
            "parent_class",
            unreal.BotanicusPlantEnvironmentAlertWidget.static_class(),
        )
        widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "WBP_PlantEnvironmentAlerts",
            DESTINATION_DIR,
            unreal.WidgetBlueprint,
            factory,
        )
    else:
        widget = unreal.EditorAssetLibrary.load_asset(WIDGET_PATH)

    if not widget or not unreal.BotanicusHudEditorLibrary.build_editable_hud_element(
        widget, "PlantEnvironmentAlerts"
    ):
        raise RuntimeError("Unable to create editable widget " + WIDGET_PATH)
    unreal.EditorAssetLibrary.save_loaded_asset(widget, False)

    unreal.log("[Botanicus Plant UI] Environment icons and editable WBP created")


main()
