import unreal


ASSET_PATH = "/Game/Botanicus/Items/itemsMesh/carton/M_CartonTape"
ASSET_FOLDER = "/Game/Botanicus/Items/itemsMesh/carton"
ASSET_NAME = "M_CartonTape"


def expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )


def scalar_parameter(material, name, value, x, y):
    node = expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def vector_parameter(material, name, value, x, y):
    node = expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def connect(source, target, target_input, source_output=""):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input
    )


def get_or_create_material():
    material = unreal.load_asset(ASSET_PATH)
    if isinstance(material, unreal.Material):
        return material

    tools = unreal.AssetToolsHelpers.get_asset_tools()
    material = tools.create_asset(
        ASSET_NAME,
        ASSET_FOLDER,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Impossible de créer {ASSET_PATH}")
    return material


def build():
    material = get_or_create_material()
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    # World-space noise keeps the subtle adhesive grain continuous across the
    # sixteen independently hidden tape segments.
    world_position = expression(
        material, unreal.MaterialExpressionWorldPosition, -1050, 20
    )
    grain_scale = scalar_parameter(material, "Adhesive Grain Scale", 0.085, -1050, 180)
    scaled_position = expression(material, unreal.MaterialExpressionMultiply, -790, 60)
    connect(world_position, scaled_position, "A")
    connect(grain_scale, scaled_position, "B")

    noise = expression(material, unreal.MaterialExpressionNoise, -550, 60)
    noise.set_editor_property("quality", 2)
    noise.set_editor_property("levels", 2)
    connect(scaled_position, noise, "Position")

    tape_dark = vector_parameter(
        material,
        "Tape Dark",
        unreal.LinearColor(0.32, 0.008, 0.004, 1.0),
        -350,
        -180,
    )
    tape_light = vector_parameter(
        material,
        "Tape Light",
        unreal.LinearColor(0.82, 0.028, 0.012, 1.0),
        -350,
        -60,
    )
    colour = expression(material, unreal.MaterialExpressionLinearInterpolate, 10, -80)
    connect(tape_dark, colour, "A")
    connect(tape_light, colour, "B")
    connect(noise, colour, "Alpha")

    rough_min = scalar_parameter(material, "Roughness Min", 0.24, -330, 260)
    rough_max = scalar_parameter(material, "Roughness Max", 0.40, -330, 380)
    roughness = expression(material, unreal.MaterialExpressionLinearInterpolate, 20, 270)
    connect(rough_min, roughness, "A")
    connect(rough_max, roughness, "B")
    connect(noise, roughness, "Alpha")

    specular = scalar_parameter(material, "Adhesive Specular", 0.48, 20, 450)

    unreal.MaterialEditingLibrary.connect_material_property(
        colour, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )

    material.set_editor_property("two_sided", False)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log("Botanicus: matériau M_CartonTape créé avec succès.")


build()
