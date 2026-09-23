"""Create/update the editable Blueprint used by the clandestine planter."""

import unreal


ASSET_FOLDER = "/Game/Botanicus/blueprints"
ASSET_NAME = "BP_Item_IllegalPlanter"
ASSET_PATH = f"{ASSET_FOLDER}/{ASSET_NAME}"
PARENT_CLASS_PATH = "/Script/Botanicus.BotanicusIllegalPlanterActor"
SOIL_BLUEPRINT_PATH = "/Game/Botanicus/blueprints/BP_TerreauPot"


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if blueprint is None:
        parent_class = unreal.load_class(None, PARENT_CLASS_PATH)
        if parent_class is None:
            raise RuntimeError("Classe C++ BotanicusIllegalPlanterActor introuvable")
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ASSET_NAME, ASSET_FOLDER, None, factory
        )
    if blueprint is None or blueprint.generated_class() is None:
        raise RuntimeError("Impossible de créer " + ASSET_PATH)

    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("use_blueprint_appearance", True)

    # Reuse the editable soil Blueprint already shared by the normal pots.
    soil_blueprint = unreal.EditorAssetLibrary.load_asset(SOIL_BLUEPRINT_PATH)
    soil_component = cdo.get_editor_property("soil_shape_visual")
    if soil_blueprint is not None and soil_blueprint.generated_class() is not None:
        soil_component.set_editor_property(
            "child_actor_class", soil_blueprint.generated_class()
        )

    unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    )
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
        save_map_packages=False, save_content_packages=True
    )
    unreal.log_warning(
        "ILLEGAL_PLANTER_BLUEPRINT_READY " + ASSET_PATH
    )


main()
