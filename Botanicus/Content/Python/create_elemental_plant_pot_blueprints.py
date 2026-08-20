import unreal


SOURCE_BLUEPRINT = "/Game/Botanicus/blueprints/BP_Item_PlantPot"
OUTPUT_FOLDER = "/Game/Botanicus/blueprints"
SOIL_BLUEPRINT = "/Game/Botanicus/blueprints/BP_TerreauPot"

POTS = (
    (
        "BP_Item_PlantPotFire",
        "/Game/Botanicus/Items/itemsMesh/potPreparation/meshs/pot_culture_feu",
    ),
    (
        "BP_Item_PlantPotWater",
        "/Game/Botanicus/Items/itemsMesh/potPreparation/meshs/pot_culture_eau",
    ),
    (
        "BP_Item_PlantPotIce",
        "/Game/Botanicus/Items/itemsMesh/potPreparation/meshs/pot_culture_glace",
    ),
    (
        "BP_Item_PlantPotShadow",
        "/Game/Botanicus/Items/itemsMesh/potPreparation/meshs/pot_culture_ténèbre",
    ),
)


def configure_pot(asset_tools, source, asset_name, mesh_path, soil_class):
    asset_path = OUTPUT_FOLDER + "/" + asset_name
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if blueprint is None:
        blueprint = asset_tools.duplicate_asset(
            asset_name, OUTPUT_FOLDER, source
        )
    if blueprint is None or blueprint.generated_class() is None:
        raise RuntimeError("Could not create " + asset_path)

    mesh = unreal.EditorAssetLibrary.load_asset(mesh_path)
    if mesh is None:
        raise RuntimeError("Missing elemental pot mesh: " + mesh_path)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
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
    top_radius_x = max(2.0, size_x * 0.34)
    top_radius_y = max(2.0, size_y * 0.34)
    # Mesh bounds include the thick ornamental rim. Keeping the initial soil
    # around the lower-middle of the full height prevents it from floating
    # above differently shaped openings. Designers can then fine-tune each BP.
    soil_height = bounds.min.z + size_z * 0.58
    soil_volume_height = max(2.0, size_z * 0.48)

    # Blueprint-owned settings read by ABotanicusPlantPotActor at runtime.
    cdo.set_editor_property(
        "SoilHorizontalPosition", unreal.Vector2D(0.0, 0.0)
    )
    cdo.set_editor_property("SoilMaximumHeightSetting", soil_height)
    cdo.set_editor_property(
        "SoilTopRadiiSetting", unreal.Vector2D(top_radius_x, top_radius_y)
    )
    cdo.set_editor_property(
        "SoilBottomRadiiSetting",
        unreal.Vector2D(top_radius_x * 0.72, top_radius_y * 0.72),
    )
    cdo.set_editor_property("SoilVolumeHeightSetting", soil_volume_height)
    cdo.set_editor_property("UseSquareSoilProfile", False)
    cdo.set_editor_property("PlantingHeightSetting", soil_height - 0.5)

    cdo.set_editor_property("soil_surface_height", soil_height)
    cdo.set_editor_property(
        "soil_top_radii", unreal.Vector2D(top_radius_x, top_radius_y)
    )
    cdo.set_editor_property(
        "soil_bottom_radii",
        unreal.Vector2D(top_radius_x * 0.72, top_radius_y * 0.72),
    )
    cdo.set_editor_property("soil_volume_height", soil_volume_height)
    cdo.set_editor_property("plant_base_height", soil_height - 0.5)

    soil_component = cdo.get_editor_property("soil_shape_visual")
    soil_component.set_editor_property("child_actor_class", soil_class)
    legacy_soil = cdo.get_editor_property("soil_mesh")
    if legacy_soil is not None:
        legacy_soil.set_editor_property("static_mesh", None)

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    )
    unreal.log_warning(
        "ELEMENTAL_POT_BLUEPRINT_READY {} soil_height={:.2f}".format(
            asset_path, soil_height
        )
    )


def main():
    source = unreal.EditorAssetLibrary.load_asset(SOURCE_BLUEPRINT)
    if source is None:
        raise RuntimeError("Missing source Blueprint: " + SOURCE_BLUEPRINT)
    soil_blueprint = unreal.EditorAssetLibrary.load_asset(SOIL_BLUEPRINT)
    if soil_blueprint is None or soil_blueprint.generated_class() is None:
        raise RuntimeError("Missing adaptive soil Blueprint: " + SOIL_BLUEPRINT)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    for asset_name, mesh_path in POTS:
        configure_pot(
            asset_tools,
            source,
            asset_name,
            mesh_path,
            soil_blueprint.generated_class(),
        )
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
        save_map_packages=False, save_content_packages=True
    )


main()
