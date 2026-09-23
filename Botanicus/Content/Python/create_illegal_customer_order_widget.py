"""Create the Blueprint entry point for the clandestine customer's order card."""

import unreal


FOLDER = "/Game/Botanicus/UI/IllegalTrade"
NAME = "WBP_IllegalCustomerOrder"
PATH = f"{FOLDER}/{NAME}"


def main():
    unreal.EditorAssetLibrary.make_directory(FOLDER)
    widget = unreal.EditorAssetLibrary.load_asset(PATH)
    if widget is None:
        parent = unreal.load_class(None, "/Script/Botanicus.BotanicusIllegalOrderWidget")
        if parent is None:
            raise RuntimeError("BotanicusIllegalOrderWidget C++ class is unavailable")
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        widget = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, FOLDER, unreal.WidgetBlueprint, factory
        )
    if widget is None or widget.generated_class() is None:
        raise RuntimeError("Unable to create " + PATH)
    unreal.EditorAssetLibrary.save_loaded_asset(widget, only_if_is_dirty=False)
    unreal.log_warning("BOTANICUS_ILLEGAL_ORDER_WIDGET_READY " + PATH)


main()
