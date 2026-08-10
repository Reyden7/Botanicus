"""Create the editable master Botanicus HUD Widget Blueprint."""

import unreal


ASSET_DIRECTORY = "/Game/Botanicus/UI/HUD"
ASSET_NAME = "WBP_BotanicusHUD"
ASSET_PATH = f"{ASSET_DIRECTORY}/{ASSET_NAME}"


def create_hud_blueprint():
    existing = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if existing:
        unreal.EditorAssetLibrary.delete_asset(ASSET_PATH)

    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property(
        "parent_class", unreal.BotanicusHudLayoutWidget.static_class())
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_DIRECTORY,
        unreal.WidgetBlueprint,
        factory,
    )
    if not blueprint:
        raise RuntimeError(f"Unable to create {ASSET_PATH}")

    if not unreal.BotanicusHudEditorLibrary.build_editable_hud_layout(
        blueprint
    ):
        raise RuntimeError(f"Unable to build the HUD layout in {ASSET_PATH}")

    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
        raise RuntimeError(f"Unable to save {ASSET_PATH}")
    unreal.log(
        "Created editable HUD Widget Blueprint: "
        f"{ASSET_PATH}; class={blueprint.generated_class().get_path_name()}"
    )


create_hud_blueprint()
