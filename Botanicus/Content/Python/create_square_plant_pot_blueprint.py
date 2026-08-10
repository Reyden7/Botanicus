import unreal


ASSET_PATH = "/Game/Botanicus/blueprints/BP_Item_PlantPotSquare"
ASSET_NAME = "BP_Item_PlantPotSquare"
ASSET_FOLDER = "/Game/Botanicus/blueprints"
PARENT_PATH = "/Script/Botanicus.BotanicusPlantPotActor"
MESH_PATH = (
    "/Game/Botanicus/Items/itemsMesh/potPreparation/carré/"
    "pot_preparation-carré"
)
SOIL_BP_PATH = "/Game/Botanicus/blueprints/BP_TerreauPot"


def main():
    mesh = unreal.EditorAssetLibrary.load_asset(MESH_PATH)
    if mesh is None:
        raise RuntimeError("Square culture-pot mesh missing: " + MESH_PATH)

    blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if blueprint is None:
        parent_class = unreal.load_class(None, PARENT_PATH)
        if parent_class is None:
            raise RuntimeError("Plant-pot parent class is unavailable")
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            ASSET_NAME, ASSET_FOLDER, None, factory
        )
    if blueprint is None or blueprint.generated_class() is None:
        raise RuntimeError("Could not create " + ASSET_PATH)

    cdo = unreal.get_default_object(blueprint.generated_class())
    cdo.set_editor_property("use_blueprint_appearance", True)
    pot_mesh = cdo.get_editor_property("mesh")
    pot_mesh.set_editor_property("static_mesh", mesh)
    pot_mesh.set_editor_property(
        "relative_scale3d", unreal.Vector(1.0, 1.0, 1.0)
    )

    bounds = mesh.get_bounding_box()
    size_x = bounds.max.x - bounds.min.x
    size_y = bounds.max.y - bounds.min.y
    size_z = bounds.max.z - bounds.min.z
    soil_half_x = max(1.0, size_x * 0.5 * 0.72)
    soil_half_y = max(1.0, size_y * 0.5 * 0.72)
    # Keep the finished soil surface clearly below the square rim.
    soil_height = bounds.max.z - max(3.0, size_z * 0.25)
    soil_volume_height = max(2.0, size_z * 0.65)

    cdo.set_editor_property("square_soil_profile", True)
    cdo.set_editor_property(
        "soil_top_radii", unreal.Vector2D(soil_half_x, soil_half_y)
    )
    cdo.set_editor_property(
        "soil_bottom_radii",
        unreal.Vector2D(soil_half_x * 0.72, soil_half_y * 0.72),
    )
    cdo.set_editor_property("soil_surface_height", soil_height)
    cdo.set_editor_property("soil_volume_height", soil_volume_height)
    cdo.set_editor_property("plant_base_height", soil_height - 0.5)
    # These Blueprint-owned variables are the live appearance controls.
    cdo.set_editor_property("UseSquareSoilProfile", True)
    cdo.set_editor_property(
        "SoilTopRadiiSetting", unreal.Vector2D(soil_half_x, soil_half_y)
    )
    cdo.set_editor_property(
        "SoilBottomRadiiSetting",
        unreal.Vector2D(soil_half_x * 0.72, soil_half_y * 0.72),
    )
    cdo.set_editor_property("SoilMaximumHeightSetting", soil_height)
    cdo.set_editor_property("SoilVolumeHeightSetting", soil_volume_height)
    cdo.set_editor_property("PlantingHeightSetting", soil_height - 0.5)

    soil_bp = unreal.EditorAssetLibrary.load_asset(SOIL_BP_PATH)
    if soil_bp is None or soil_bp.generated_class() is None:
        raise RuntimeError("Adaptive soil Blueprint missing: " + SOIL_BP_PATH)
    soil_component = cdo.get_editor_property("soil_shape_visual")
    soil_component.set_editor_property(
        "child_actor_class", soil_bp.generated_class()
    )
    legacy_soil = cdo.get_editor_property("soil_mesh")
    if legacy_soil is not None:
        legacy_soil.set_editor_property("static_mesh", None)

    unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    )
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
        save_map_packages=False, save_content_packages=True
    )
    unreal.log_warning(
        "SQUARE_PLANT_POT_READY {} mesh={} soil=({:.2f}, {:.2f}, {:.2f})".format(
            ASSET_PATH, MESH_PATH, soil_half_x, soil_half_y, soil_height
        )
    )


main()
