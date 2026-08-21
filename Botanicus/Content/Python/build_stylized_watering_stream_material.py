"""Create the bright, cartoon-styled material used by the watering stream."""

import unreal


ASSET_FOLDER = "/Game/Botanicus/Materials/Effects"
ASSET_NAME = "M_BotanicusWaterStreamStylized"
ASSET_PATH = f"{ASSET_FOLDER}/{ASSET_NAME}"


def node(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )


def connect(source, target, input_name, output_name=""):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source, output_name, target, input_name
    )


def scalar(material, name, value, x, y):
    result = node(material, unreal.MaterialExpressionScalarParameter, x, y)
    result.set_editor_property("parameter_name", name)
    result.set_editor_property("default_value", value)
    return result


def color(material, name, value, x, y):
    result = node(material, unreal.MaterialExpressionVectorParameter, x, y)
    result.set_editor_property("parameter_name", name)
    result.set_editor_property("default_value", value)
    return result


def build():
    unreal.EditorAssetLibrary.make_directory(ASSET_FOLDER)
    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        raise RuntimeError(
            f"{ASSET_PATH} existe déjà; utilisez un nouveau nom pour une reconstruction sûre"
        )

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_FOLDER,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Impossible de créer {ASSET_PATH}")

    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)

    deep_blue = color(
        material,
        "Water Blue",
        unreal.LinearColor(0.005, 0.20, 0.82, 1.0),
        -720,
        -120,
    )
    bright_edge = color(
        material,
        "Foam Highlight",
        unreal.LinearColor(0.65, 0.95, 1.0, 1.0),
        -720,
        20,
    )
    fresnel = node(material, unreal.MaterialExpressionFresnel, -700, 170)
    edge_strength = scalar(material, "White Edge Strength", 1.35, -700, 300)
    boosted_edge = node(material, unreal.MaterialExpressionMultiply, -460, 190)
    connect(fresnel, boosted_edge, "A")
    connect(edge_strength, boosted_edge, "B")

    water_color = node(
        material, unreal.MaterialExpressionLinearInterpolate, -260, -60
    )
    connect(deep_blue, water_color, "A")
    connect(bright_edge, water_color, "B")
    connect(boosted_edge, water_color, "Alpha")

    emissive_strength = scalar(material, "Emissive Strength", 0.24, -260, 170)
    emissive = node(material, unreal.MaterialExpressionMultiply, -20, 80)
    connect(water_color, emissive, "A")
    connect(emissive_strength, emissive, "B")

    center_opacity = scalar(material, "Center Opacity", 0.82, -250, 310)
    edge_opacity = scalar(material, "Edge Opacity", 0.98, -250, 410)
    opacity = node(
        material, unreal.MaterialExpressionLinearInterpolate, 0, 340
    )
    connect(center_opacity, opacity, "A")
    connect(edge_opacity, opacity, "B")
    connect(fresnel, opacity, "Alpha")

    roughness = scalar(material, "Roughness", 0.10, 0, 480)
    specular = scalar(material, "Specular", 0.80, 0, 570)

    unreal.MaterialEditingLibrary.connect_material_property(
        water_color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        specular, "", unreal.MaterialProperty.MP_SPECULAR
    )

    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
    unreal.log(f"Botanicus: matériau de jet d'eau créé: {ASSET_PATH}")


build()
