"""Create the Botanicus soil material whose appearance reacts to wetness."""

import unreal


ASSET_FOLDER = "/Game/Botanicus/Materials/Soil"
ASSET_NAME = "M_BotanicusSoilWetPatchesV2"
ASSET_PATH = f"{ASSET_FOLDER}/{ASSET_NAME}"
BASE_COLOR_PATH = (
    "/Game/Botanicus/Items/itemsMesh/TerreauDansPot/"
    "base_color_001.base_color_001"
)
NORMAL_PATH = (
    "/Game/Botanicus/Items/itemsMesh/TerreauDansPot/normal_001.normal_001"
)


def expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )


def connect(source, target, target_input, source_output=""):
    unreal.MaterialEditingLibrary.connect_material_expressions(
        source, source_output, target, target_input
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


def texture_parameter(material, name, texture, x, y, sampler_type=None):
    node = expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, x, y
    )
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("texture", texture)
    if sampler_type is not None:
        node.set_editor_property("sampler_type", sampler_type)
    return node


def get_or_create_material():
    material = unreal.load_asset(ASSET_PATH)
    if isinstance(material, unreal.Material):
        return material
    unreal.EditorAssetLibrary.make_directory(ASSET_FOLDER)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_FOLDER,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )
    if not isinstance(material, unreal.Material):
        raise RuntimeError(f"Impossible de créer {ASSET_PATH}")
    return material


def build():
    base_texture = unreal.load_asset(BASE_COLOR_PATH)
    normal_texture = unreal.load_asset(NORMAL_PATH)
    if base_texture is None or normal_texture is None:
        raise RuntimeError("Textures de terreau introuvables")

    material = get_or_create_material()
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    base_sample = texture_parameter(
        material, "Soil Base Texture", base_texture, -900, -180
    )
    dry_tint = vector_parameter(
        material,
        "Dry Tint",
        unreal.LinearColor(1.05, 1.0, 0.92, 1.0),
        -900,
        20,
    )
    wet_tint = vector_parameter(
        material,
        "Wet Tint",
        unreal.LinearColor(0.30, 0.24, 0.18, 1.0),
        -900,
        150,
    )
    wetness = scalar_parameter(material, "Wetness", 0.0, -900, 300)

    # Wetness does not cover the surface uniformly. A broad procedural noise
    # controls which patches become wet first. The threshold is built so that
    # Wetness=0 remains completely dry and Wetness=1 is guaranteed to cover
    # the entire surface.
    texcoord = expression(
        material, unreal.MaterialExpressionTextureCoordinate, -900, 470
    )
    patch_scale = scalar_parameter(
        material, "Wet Patch Scale", 3.2, -900, 580
    )
    scaled_uv = expression(
        material, unreal.MaterialExpressionMultiply, -650, 500
    )
    connect(texcoord, scaled_uv, "A")
    connect(patch_scale, scaled_uv, "B")
    patch_noise = expression(material, unreal.MaterialExpressionNoise, -440, 500)
    patch_noise.set_editor_property("quality", 2)
    patch_noise.set_editor_property("levels", 3)
    connect(scaled_uv, patch_noise, "Position")

    patchiness = scalar_parameter(
        material, "Wet Patchiness", 0.70, -420, 650
    )
    noise_threshold = expression(
        material, unreal.MaterialExpressionMultiply, -190, 510
    )
    connect(patch_noise, noise_threshold, "A")
    connect(patchiness, noise_threshold, "B")

    wetness_range = scalar_parameter(
        material, "Wetness Coverage Range", 1.70, -420, 790
    )
    scaled_wetness = expression(
        material, unreal.MaterialExpressionMultiply, -190, 760
    )
    connect(wetness, scaled_wetness, "A")
    connect(wetness_range, scaled_wetness, "B")
    coverage_threshold = expression(
        material, unreal.MaterialExpressionSubtract, 40, 590
    )
    connect(scaled_wetness, coverage_threshold, "A")
    connect(noise_threshold, coverage_threshold, "B")
    transition_softness = scalar_parameter(
        material, "Wet Edge Softness", 0.22, 20, 780
    )
    softened_coverage = expression(
        material, unreal.MaterialExpressionDivide, 260, 590
    )
    connect(coverage_threshold, softened_coverage, "A")
    connect(transition_softness, softened_coverage, "B")
    # Clamp explicitly with Max/Min nodes. MaterialExpressionSaturate does not
    # expose a connectable input under the same name on every UE version.
    clamp_minimum = scalar_parameter(
        material, "Wet Mask Minimum", 0.0, 260, 720
    )
    clamp_low = expression(material, unreal.MaterialExpressionMax, 480, 590)
    connect(softened_coverage, clamp_low, "A")
    connect(clamp_minimum, clamp_low, "B")
    clamp_maximum = scalar_parameter(
        material, "Wet Mask Maximum", 1.0, 480, 720
    )
    patch_wetness = expression(material, unreal.MaterialExpressionMin, 700, 590)
    connect(clamp_low, patch_wetness, "A")
    connect(clamp_maximum, patch_wetness, "B")

    tint = expression(material, unreal.MaterialExpressionLinearInterpolate, -570, 70)
    connect(dry_tint, tint, "A")
    connect(wet_tint, tint, "B")
    connect(patch_wetness, tint, "Alpha")
    tinted_soil = expression(material, unreal.MaterialExpressionMultiply, -300, -120)
    connect(base_sample, tinted_soil, "A", "RGB")
    connect(tint, tinted_soil, "B")

    normal_sample = texture_parameter(
        material,
        "Soil Normal Texture",
        normal_texture,
        -300,
        160,
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL,
    )
    dry_roughness = scalar_parameter(material, "Dry Roughness", 0.92, -300, 360)
    wet_roughness = scalar_parameter(material, "Wet Roughness", 0.38, -300, 470)
    roughness = expression(
        material, unreal.MaterialExpressionLinearInterpolate, 20, 390
    )
    connect(dry_roughness, roughness, "A")
    connect(wet_roughness, roughness, "B")
    connect(patch_wetness, roughness, "Alpha")

    dry_specular = scalar_parameter(material, "Dry Specular", 0.12, -300, 600)
    wet_specular = scalar_parameter(material, "Wet Specular", 0.48, -300, 710)
    specular = expression(
        material, unreal.MaterialExpressionLinearInterpolate, 20, 620
    )
    connect(dry_specular, specular, "A")
    connect(wet_specular, specular, "B")
    connect(patch_wetness, specular, "Alpha")

    unreal.MaterialEditingLibrary.connect_material_property(
        tinted_soil, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normal_sample, "RGB", unreal.MaterialProperty.MP_NORMAL
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
    unreal.log(f"Botanicus: {ASSET_PATH} créé avec le paramètre Wetness.")


build()
