import unreal


MATERIAL_PATH = "/Game/Botanicus/Items/itemsMesh/carton/cardboard"


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


def build():
    material = unreal.load_asset(MATERIAL_PATH)
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Material introuvable: {MATERIAL_PATH}")

    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    # A compact procedural kraft paper graph. The two differently scaled noise
    # layers give the surface broad painted variation plus thin paper fibres.
    texcoord = expression(material, unreal.MaterialExpressionTextureCoordinate, -1250, 30)

    coarse_scale = scalar_parameter(material, "Coarse Grain Scale", 3.5, -1250, -160)
    coarse_multiply = expression(material, unreal.MaterialExpressionMultiply, -1000, -80)
    connect(texcoord, coarse_multiply, "A")
    connect(coarse_scale, coarse_multiply, "B")
    coarse_noise = expression(material, unreal.MaterialExpressionNoise, -760, -80)
    coarse_noise.set_editor_property("quality", 2)
    coarse_noise.set_editor_property("levels", 3)
    connect(coarse_multiply, coarse_noise, "Position")

    fibre_scale = scalar_parameter(material, "Fibre Scale", 42.0, -1250, 260)
    fibre_multiply = expression(material, unreal.MaterialExpressionMultiply, -1000, 210)
    connect(texcoord, fibre_multiply, "A")
    connect(fibre_scale, fibre_multiply, "B")
    fibre_noise = expression(material, unreal.MaterialExpressionNoise, -760, 210)
    fibre_noise.set_editor_property("quality", 1)
    fibre_noise.set_editor_property("levels", 2)
    connect(fibre_multiply, fibre_noise, "Position")

    fibre_strength = scalar_parameter(material, "Fibre Strength", 0.22, -740, 410)
    fibre_amount = expression(material, unreal.MaterialExpressionMultiply, -510, 225)
    connect(fibre_noise, fibre_amount, "A")
    connect(fibre_strength, fibre_amount, "B")

    grain_mix = expression(material, unreal.MaterialExpressionAdd, -290, 20)
    connect(coarse_noise, grain_mix, "A")
    connect(fibre_amount, grain_mix, "B")
    grain_clamp = expression(material, unreal.MaterialExpressionSaturate, -80, 20)
    connect(grain_mix, grain_clamp, "Input")

    dark_kraft = vector_parameter(
        material,
        "Kraft Dark",
        unreal.LinearColor(0.24, 0.105, 0.025, 1.0),
        -300,
        -260,
    )
    light_kraft = vector_parameter(
        material,
        "Kraft Light",
        unreal.LinearColor(0.66, 0.36, 0.075, 1.0),
        -300,
        -140,
    )
    base_colour = expression(material, unreal.MaterialExpressionLinearInterpolate, 170, -90)
    connect(dark_kraft, base_colour, "A")
    connect(light_kraft, base_colour, "B")
    connect(grain_clamp, base_colour, "Alpha")

    rough_min = scalar_parameter(material, "Roughness Min", 0.72, -290, 330)
    rough_max = scalar_parameter(material, "Roughness Max", 0.94, -290, 450)
    roughness = expression(material, unreal.MaterialExpressionLinearInterpolate, 180, 360)
    connect(rough_min, roughness, "A")
    connect(rough_max, roughness, "B")
    connect(fibre_noise, roughness, "Alpha")

    specular = scalar_parameter(material, "Specular", 0.16, 190, 520)

    unreal.MaterialEditingLibrary.connect_material_property(
        base_colour, "", unreal.MaterialProperty.MP_BASE_COLOR
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
    unreal.log("Botanicus: matériau cardboard reconstruit avec succès.")


build()
