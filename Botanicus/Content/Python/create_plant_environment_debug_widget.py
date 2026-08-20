"""Create the editable world-space plant environment debug Widget Blueprint."""

import unreal


WIDGET_DIR = "/Game/Botanicus/UI/Plant/Environment"
WIDGET_PATH = WIDGET_DIR + "/WBP_PlantEnvironmentDebug"


def main():
    unreal.EditorAssetLibrary.make_directory(WIDGET_DIR)
    if not unreal.EditorAssetLibrary.does_asset_exist(WIDGET_PATH):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property(
            "parent_class",
            unreal.BotanicusPlantEnvironmentDebugWidget.static_class(),
        )
        widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            "WBP_PlantEnvironmentDebug",
            WIDGET_DIR,
            unreal.WidgetBlueprint,
            factory,
        )
    else:
        widget = unreal.EditorAssetLibrary.load_asset(WIDGET_PATH)

    if not widget or not unreal.BotanicusHudEditorLibrary.build_editable_hud_element(
        widget, "PlantEnvironmentDebug"
    ):
        raise RuntimeError("Unable to create editable widget " + WIDGET_PATH)
    unreal.EditorAssetLibrary.save_loaded_asset(widget, False)
    unreal.log("[Botanicus Plant UI] Editable environment debug WBP created")


main()
