import unreal


SOIL_BP_PATH = "/Game/Botanicus/blueprints/BP_TerreauPot"
SOIL_BP_NAME = "BP_TerreauPot"
SOIL_BP_FOLDER = "/Game/Botanicus/blueprints"
SOIL_PARENT_PATH = "/Script/Botanicus.BotanicusPotSoilVisualActor"
POT_BLUEPRINTS = (
    "/Game/Botanicus/blueprints/BP_Item_PlantPot",
    "/Game/Botanicus/blueprints/BP_Item_SalePot",
)

# Calibrated to the real culture-pot mesh: the soil disk intersects the inner
# wall slightly so the pot hides its edge, and its final surface stays below
# the upper lip.
PLANT_POT_SOIL_SURFACE_HEIGHT = 18.5
PLANT_POT_SOIL_TOP_RADIUS = 34.0
PLANT_POT_PLANT_BASE_HEIGHT = 18.0
SALE_POT_SOIL_SURFACE_HEIGHT = 6.0
SALE_POT_SOIL_TOP_RADIUS = 13.5


def main():
    soil_bp = unreal.EditorAssetLibrary.load_asset(SOIL_BP_PATH)
    if soil_bp is None:
        parent_class = unreal.load_class(None, SOIL_PARENT_PATH)
        if parent_class is None:
            raise RuntimeError("Soil visual parent class is unavailable")
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        soil_bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            SOIL_BP_NAME, SOIL_BP_FOLDER, None, factory
        )
    if soil_bp is None or soil_bp.generated_class() is None:
        raise RuntimeError("Could not create " + SOIL_BP_PATH)

    soil_class = soil_bp.generated_class()
    unreal.EditorAssetLibrary.save_loaded_asset(soil_bp, only_if_is_dirty=False)

    for pot_path in POT_BLUEPRINTS:
        pot_bp = unreal.EditorAssetLibrary.load_asset(pot_path)
        if pot_bp is None or pot_bp.generated_class() is None:
            raise RuntimeError("Pot Blueprint missing: " + pot_path)
        cdo = unreal.get_default_object(pot_bp.generated_class())
        if pot_path.endswith("PlantPot"):
            cdo.set_editor_property(
                "soil_surface_height", PLANT_POT_SOIL_SURFACE_HEIGHT
            )
            cdo.set_editor_property(
                "soil_top_radii",
                unreal.Vector2D(
                    PLANT_POT_SOIL_TOP_RADIUS,
                    PLANT_POT_SOIL_TOP_RADIUS,
                ),
            )
            cdo.set_editor_property(
                "plant_base_height", PLANT_POT_PLANT_BASE_HEIGHT
            )
        elif pot_path.endswith("SalePot"):
            cdo.set_editor_property(
                "soil_surface_height", SALE_POT_SOIL_SURFACE_HEIGHT
            )
            cdo.set_editor_property(
                "soil_top_radii",
                unreal.Vector2D(
                    SALE_POT_SOIL_TOP_RADIUS,
                    SALE_POT_SOIL_TOP_RADIUS,
                ),
            )
        child_component = cdo.get_editor_property("soil_shape_visual")
        if child_component is None:
            raise RuntimeError("Adaptive soil component missing: " + pot_path)
        child_component.set_editor_property("child_actor_class", soil_class)
        legacy_property = (
            "soil_mesh" if pot_path.endswith("PlantPot") else "soil_visual"
        )
        legacy_component = cdo.get_editor_property(legacy_property)
        if legacy_component is not None:
            legacy_component.set_editor_property("static_mesh", None)
        unreal.EditorAssetLibrary.save_loaded_asset(
            pot_bp, only_if_is_dirty=False
        )
        if child_component.get_editor_property("child_actor_class") != soil_class:
            raise RuntimeError("Soil Blueprint link mismatch: " + pot_path)

    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
        save_map_packages=False, save_content_packages=True
    )
    unreal.log_warning(
        "SOIL_BLUEPRINT_READY {} linked to {} pots".format(
            SOIL_BP_PATH, len(POT_BLUEPRINTS)
        )
    )


main()
