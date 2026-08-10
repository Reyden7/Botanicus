import unreal


POT_BLUEPRINTS = (
    ("/Game/Botanicus/blueprints/BP_Item_PlantPot", "plant"),
    ("/Game/Botanicus/blueprints/BP_Item_PlantPotSquare", "plant"),
    ("/Game/Botanicus/blueprints/BP_Item_SalePot", "sale"),
    ("/Game/Botanicus/blueprints/BP_Item_SalePotSquare", "sale"),
)

SOIL_CATEGORY = "Botanicus|Appearance|Soil"
PLANT_CATEGORY = "Botanicus|Appearance|Plant"
WORKBENCH_CATEGORY = "Botanicus|Appearance|Workbench"


def pin_type(type_name):
    if type_name == "float":
        return unreal.BlueprintEditorLibrary.get_basic_type_by_name("real")
    if type_name == "bool":
        return unreal.BlueprintEditorLibrary.get_basic_type_by_name("bool")
    if type_name == "Vector2D":
        return unreal.BlueprintEditorLibrary.get_struct_type(
            unreal.Vector2D.static_struct()
        )
    raise RuntimeError("Unsupported Blueprint variable type: " + type_name)


def add_variable(blueprint, existing_names, name, type_name, category):
    if name not in existing_names:
        if not unreal.BlueprintEditorLibrary.add_member_variable(
            blueprint, name, pin_type(type_name)
        ):
            raise RuntimeError(
                "Could not add Blueprint variable {} to {}".format(
                    name, blueprint.get_path_name()
                )
            )
        existing_names.add(name)

    unreal.BlueprintEditorLibrary.set_blueprint_variable_instance_editable(
        blueprint, unreal.Name(name), True
    )
    unreal.BlueprintEditorLibrary.set_blueprint_variable_category(
        blueprint, unreal.Name(name), unreal.Text(category)
    )


def native_defaults(cdo, pot_kind):
    values = {
        "SoilHorizontalPosition": cdo.get_editor_property(
            "soil_horizontal_offset"
        ),
        "SoilMaximumHeightSetting": cdo.get_editor_property(
            "soil_surface_height"
        ),
        "SoilBottomRadiiSetting": cdo.get_editor_property(
            "soil_bottom_radii"
        ),
        "SoilTopRadiiSetting": cdo.get_editor_property("soil_top_radii"),
        "SoilVolumeHeightSetting": cdo.get_editor_property(
            "soil_volume_height"
        ),
        "UseSquareSoilProfile": cdo.get_editor_property(
            "square_soil_profile"
        ),
    }
    if pot_kind == "plant":
        values["PlantingHeightSetting"] = cdo.get_editor_property(
            "plant_base_height"
        )
    else:
        values["WorkbenchHeightAdjustmentSetting"] = cdo.get_editor_property(
            "preparation_height_adjustment"
        )
    return values


def configure_blueprint(asset_path, pot_kind):
    blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if blueprint is None or blueprint.generated_class() is None:
        raise RuntimeError("Pot Blueprint missing: " + asset_path)

    old_cdo = unreal.get_default_object(blueprint.generated_class())
    defaults = native_defaults(old_cdo, pot_kind)
    existing_names = {
        str(name)
        for name in unreal.BlueprintEditorLibrary.list_member_variable_names(
            blueprint, False
        )
    }

    variables = (
        ("SoilHorizontalPosition", "Vector2D", SOIL_CATEGORY),
        ("SoilMaximumHeightSetting", "float", SOIL_CATEGORY),
        ("SoilBottomRadiiSetting", "Vector2D", SOIL_CATEGORY),
        ("SoilTopRadiiSetting", "Vector2D", SOIL_CATEGORY),
        ("SoilVolumeHeightSetting", "float", SOIL_CATEGORY),
        ("UseSquareSoilProfile", "bool", SOIL_CATEGORY),
    )
    for name, type_name, category in variables:
        add_variable(blueprint, existing_names, name, type_name, category)

    if pot_kind == "plant":
        add_variable(
            blueprint,
            existing_names,
            "PlantingHeightSetting",
            "float",
            PLANT_CATEGORY,
        )
    else:
        add_variable(
            blueprint,
            existing_names,
            "WorkbenchHeightAdjustmentSetting",
            "float",
            WORKBENCH_CATEGORY,
        )

    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    cdo = unreal.get_default_object(blueprint.generated_class())
    for property_name, value in defaults.items():
        cdo.set_editor_property(property_name, value)

    unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    )
    unreal.log_warning(
        "POT_BLUEPRINT_POSITIONING_READY {} variables={}".format(
            asset_path, len(defaults)
        )
    )


def main():
    for asset_path, pot_kind in POT_BLUEPRINTS:
        configure_blueprint(asset_path, pot_kind)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
        save_map_packages=False, save_content_packages=True
    )


main()
