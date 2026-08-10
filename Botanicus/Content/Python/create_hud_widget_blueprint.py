"""Create the editable master Botanicus HUD Widget Blueprint."""

import unreal


ASSET_DIRECTORY = "/Game/Botanicus/UI/HUD"
ASSET_NAME = "WBP_BotanicusHUD"
ASSET_PATH = f"{ASSET_DIRECTORY}/{ASSET_NAME}"


def create_widget_blueprint(name, directory, parent_class):
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        directory,
        unreal.WidgetBlueprint,
        factory,
    )


def create_hud_element(name, parent_class, element_type):
    directory = f"{ASSET_DIRECTORY}/Elements"
    path = f"{directory}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    blueprint = create_widget_blueprint(name, directory, parent_class)
    if not blueprint:
        raise RuntimeError(f"Unable to create {path}")
    if not unreal.BotanicusHudEditorLibrary.build_editable_hud_element(
        blueprint, element_type
    ):
        raise RuntimeError(f"Unable to build {path}")
    if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
        raise RuntimeError(f"Unable to save {path}")
    unreal.log(f"Created editable HUD element: {path}")


def create_hud_blueprint():
    elements = (
        ("WBP_HUD_Clock", unreal.BotanicusClockWidget.static_class(), "Clock"),
        ("WBP_HUD_Credits", unreal.BotanicusSharedFundsWidget.static_class(), "Credits"),
        ("WBP_HUD_Reputation", unreal.BotanicusReputationWidget.static_class(), "Reputation"),
        ("WBP_HUD_Objectives", unreal.BotanicusShopObjectivesWidget.static_class(), "Objectives"),
        ("WBP_HUD_QuickBar", unreal.BotanicusQuickBarWidget.static_class(), "QuickBar"),
        ("WBP_HUD_Interaction", unreal.BotanicusInteractionTargetWidget.static_class(), "Interaction"),
        ("WBP_HUD_Message", unreal.BotanicusHudMessageWidget.static_class(), "Message"),
    )
    for name, parent_class, element_type in elements:
        create_hud_element(name, parent_class, element_type)

    blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    created_master = False
    if not blueprint:
        created_master = True
        blueprint = create_widget_blueprint(
            ASSET_NAME,
            ASSET_DIRECTORY,
            unreal.BotanicusHudLayoutWidget.static_class(),
        )
        if not blueprint:
            raise RuntimeError(f"Unable to create {ASSET_PATH}")
        if not unreal.BotanicusHudEditorLibrary.build_editable_hud_layout(
            blueprint
        ):
            raise RuntimeError(
                f"Unable to build the HUD layout in {ASSET_PATH}"
            )

    if created_master and not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, False
    ):
        raise RuntimeError(f"Unable to save {ASSET_PATH}")
    unreal.log(
        "Created editable HUD Widget Blueprint: "
        f"{ASSET_PATH}; class={blueprint.generated_class().get_path_name()}"
    )


create_hud_blueprint()
