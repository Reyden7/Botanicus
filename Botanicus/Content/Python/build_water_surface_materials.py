"""Build pooled wetness and organic cartoon puddle assets."""

import math
import os
import struct
import zlib

import unreal


MATERIAL_FOLDER = "/Game/Botanicus/Materials/Effects"
TEXTURE_FOLDER = "/Game/Botanicus/VFX/Watering"
SOURCE_FOLDER = os.path.join(
    unreal.Paths.project_content_dir(), "Botanicus", "VFX", "Watering", "Source"
)
VORONOI_PATH = "/Game/Botanicus/VFX/Watering/T_Voronoi01"


def png_chunk(kind, payload):
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
    )


def write_png(path, width, height, channels, pixels):
    color_type = 0 if channels == 1 else 2
    stride = width * channels
    rows = [
        b"\x00" + bytes(pixels[y * stride : (y + 1) * stride])
        for y in range(height)
    ]
    with open(path, "wb") as output:
        output.write(b"\x89PNG\r\n\x1a\n")
        output.write(
            png_chunk(
                b"IHDR",
                struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0),
            )
        )
        output.write(png_chunk(b"IDAT", zlib.compress(b"".join(rows), 9)))
        output.write(png_chunk(b"IEND", b""))


def smoothstep(edge0, edge1, value):
    t = max(0.0, min(1.0, (value - edge0) / (edge1 - edge0)))
    return t * t * (3.0 - 2.0 * t)


def generate_source_textures():
    os.makedirs(SOURCE_FOLDER, exist_ok=True)
    cell_size = 256
    width = cell_size * 3
    height = cell_size * 2
    mask_pixels = bytearray(width * height)
    harmonic_sets = [
        ((3, 0.11, 0.2), (5, 0.07, 1.4), (7, 0.035, 2.1)),
        ((2, 0.09, 1.1), (4, 0.08, 0.4), (6, 0.045, 2.7)),
        ((3, 0.08, 2.0), (6, 0.07, 0.7), (8, 0.030, 1.2)),
        ((2, 0.10, 2.4), (5, 0.065, 0.1), (9, 0.025, 2.0)),
        ((4, 0.09, 0.8), (7, 0.055, 1.7), (10, 0.020, 0.3)),
        ((3, 0.10, 1.5), (5, 0.055, 2.8), (8, 0.035, 0.6)),
    ]
    for variant, harmonics in enumerate(harmonic_sets):
        tile_x = variant % 3
        tile_y = variant // 3
        aspect_x = (0.88, 1.02, 0.93, 1.04, 0.90, 1.00)[variant]
        aspect_y = (1.00, 0.90, 1.04, 0.88, 1.02, 0.94)[variant]
        for local_y in range(cell_size):
            for local_x in range(cell_size):
                x = (local_x + 0.5 - cell_size * 0.5) / (
                    cell_size * 0.36 * aspect_x
                )
                y = (local_y + 0.5 - cell_size * 0.5) / (
                    cell_size * 0.36 * aspect_y
                )
                angle = math.atan2(y, x)
                boundary = 1.0
                for frequency, amplitude, phase in harmonics:
                    boundary += amplitude * math.sin(frequency * angle + phase)
                distance = math.sqrt(x * x + y * y)
                value = 1.0 - smoothstep(
                    boundary - 0.075, boundary + 0.025, distance
                )
                world_x = tile_x * cell_size + local_x
                world_y = tile_y * cell_size + local_y
                mask_pixels[world_y * width + world_x] = round(value * 255.0)

    mask_source = os.path.join(SOURCE_FOLDER, "T_PuddleMasks_Atlas.png")
    write_png(mask_source, width, height, 1, mask_pixels)

    normal_size = 256
    normal_pixels = bytearray(normal_size * normal_size * 3)
    tau = math.pi * 2.0
    for y in range(normal_size):
        v = y / normal_size
        for x in range(normal_size):
            u = x / normal_size
            dx = (
                0.55 * tau * 3.0 * math.cos(tau * (3.0 * u + 2.0 * v))
                + 0.30
                * tau
                * 5.0
                * math.cos(tau * (5.0 * u - 3.0 * v + 0.2))
            )
            dy = (
                0.55 * tau * 2.0 * math.cos(tau * (3.0 * u + 2.0 * v))
                - 0.30
                * tau
                * 3.0
                * math.cos(tau * (5.0 * u - 3.0 * v + 0.2))
            )
            nx = -dx * 0.018
            ny = -dy * 0.018
            nz = 1.0
            length = math.sqrt(nx * nx + ny * ny + nz * nz)
            pixel = (y * normal_size + x) * 3
            normal_pixels[pixel] = round((nx / length * 0.5 + 0.5) * 255.0)
            normal_pixels[pixel + 1] = round((ny / length * 0.5 + 0.5) * 255.0)
            normal_pixels[pixel + 2] = round((nz / length * 0.5 + 0.5) * 255.0)

    normal_source = os.path.join(SOURCE_FOLDER, "T_PuddleNormal_Soft.png")
    write_png(normal_source, normal_size, normal_size, 3, normal_pixels)
    return mask_source, normal_source


def import_texture(source, destination_name, compression, srgb, clamp):
    destination = f"{TEXTURE_FOLDER}/{destination_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        unreal.EditorAssetLibrary.delete_asset(destination)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", TEXTURE_FOLDER)
    task.set_editor_property("destination_name", destination_name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", True)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = unreal.load_asset(destination)
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError(f"Import de texture impossible: {destination}")
    texture.set_editor_property("srgb", srgb)
    texture.set_editor_property("compression_settings", compression)
    if clamp:
        texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
        texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    return texture


def node(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )


def connect(source, target, input_name, output_name=""):
    if not unreal.MaterialEditingLibrary.connect_material_expressions(
        source, output_name, target, input_name
    ):
        raise RuntimeError(
            f"Connexion matériau impossible: {source.get_class().get_name()}"
            f".{output_name or '<default>'} -> "
            f"{target.get_class().get_name()}.{input_name}"
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


def binary(material, expression_class, a, b, x, y):
    result = node(material, expression_class, x, y)
    connect(a, result, "A")
    connect(b, result, "B")
    return result


def multiply(material, a, b, x, y):
    return binary(material, unreal.MaterialExpressionMultiply, a, b, x, y)


def component_rg(material, source, x, y):
    result = node(material, unreal.MaterialExpressionComponentMask, x, y)
    result.set_editor_property("r", True)
    result.set_editor_property("g", True)
    connect(source, result, "None", "RGB")
    return result


def centered_uv_distance(material, texcoord, x, y):
    center = color(
        material,
        "Shape Center",
        unreal.LinearColor(0.5, 0.5, 0.0, 0.0),
        x,
        y + 100,
    )
    centered = binary(
        material,
        unreal.MaterialExpressionSubtract,
        texcoord,
        component_rg(material, center, x + 180, y + 100),
        x + 360,
        y,
    )
    distance = node(material, unreal.MaterialExpressionLength, x + 540, y)
    connect(centered, distance, "None")
    return distance


def make_wetness_mask(material, x, y):
    texcoord = node(material, unreal.MaterialExpressionTextureCoordinate, x, y)
    distance = centered_uv_distance(material, texcoord, x + 160, y)
    normalized = multiply(
        material,
        distance,
        scalar(material, "Mask Radius Scale", 2.15, x + 650, y + 100),
        x + 820,
        y,
    )
    inverted = node(material, unreal.MaterialExpressionOneMinus, x + 990, y)
    connect(normalized, inverted, "None")
    radial = node(material, unreal.MaterialExpressionSaturate, x + 1150, y)
    connect(inverted, radial, "None")
    texture_sample = node(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        x + 620,
        y + 230,
    )
    texture_sample.set_editor_property("parameter_name", "Irregular Mask")
    texture_sample.set_editor_property("texture", unreal.load_asset(VORONOI_PATH))
    irregular = node(
        material, unreal.MaterialExpressionLinearInterpolate, x + 850, y + 250
    )
    connect(
        scalar(material, "Irregular Minimum", 0.62, x + 640, y + 350),
        irregular,
        "A",
    )
    connect(
        scalar(material, "Irregular Maximum", 1.0, x + 640, y + 430),
        irregular,
        "B",
    )
    connect(texture_sample, irregular, "Alpha", "R")
    return multiply(material, radial, irregular, x + 1320, y + 40)


def per_instance(material, index, x, y):
    result = node(material, unreal.MaterialExpressionPerInstanceCustomData, x, y)
    result.set_editor_property("data_index", index)
    return result


def make_atlas_mask(material, mask_texture, shape_data, texcoord, x, y):
    shape_index = multiply(
        material,
        shape_data,
        scalar(material, "Shape Count Minus Epsilon", 5.999, x, y + 100),
        x + 190,
        y,
    )
    floored_index = node(material, unreal.MaterialExpressionFloor, x + 370, y)
    connect(shape_index, floored_index, "None")
    columns = scalar(material, "Atlas Columns", 3.0, x + 360, y + 120)
    column = binary(
        material,
        unreal.MaterialExpressionFmod,
        floored_index,
        columns,
        x + 550,
        y,
    )
    row_divided = binary(
        material,
        unreal.MaterialExpressionDivide,
        floored_index,
        columns,
        x + 550,
        y + 130,
    )
    row = node(material, unreal.MaterialExpressionFloor, x + 730, y + 130)
    connect(row_divided, row, "None")
    column_offset = multiply(
        material,
        column,
        scalar(material, "Atlas Column Size", 1.0 / 3.0, x + 720, y - 80),
        x + 900,
        y,
    )
    row_offset = multiply(
        material,
        row,
        scalar(material, "Atlas Row Size", 0.5, x + 720, y + 230),
        x + 900,
        y + 130,
    )
    offset = node(
        material, unreal.MaterialExpressionAppendVector, x + 1080, y + 50
    )
    connect(column_offset, offset, "A")
    connect(row_offset, offset, "B")
    atlas_scale_color = color(
        material,
        "Atlas UV Scale",
        unreal.LinearColor(1.0 / 3.0, 0.5, 0.0, 0.0),
        x + 680,
        y + 330,
    )
    scaled_uv = multiply(
        material,
        texcoord,
        component_rg(material, atlas_scale_color, x + 880, y + 330),
        x + 1080,
        y + 260,
    )
    atlas_uv = binary(
        material,
        unreal.MaterialExpressionAdd,
        scaled_uv,
        offset,
        x + 1260,
        y + 150,
    )
    sample = node(
        material,
        unreal.MaterialExpressionTextureSampleParameter2D,
        x + 1450,
        y + 120,
    )
    sample.set_editor_property("parameter_name", "Puddle Shape Atlas")
    sample.set_editor_property("texture", mask_texture)
    connect(atlas_uv, sample, "UVs")
    return sample


def create_material(name, replace=False):
    path = f"{MATERIAL_FOLDER}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        if not replace:
            unreal.log(f"Botanicus matériau conservé: {path}")
            return None
        if not unreal.EditorAssetLibrary.delete_asset(path):
            raise RuntimeError(f"Impossible de reconstruire {path}")
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name,
        MATERIAL_FOLDER,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )


def build_wetness():
    material = create_material("M_BotanicusWetness", replace=True)
    material.set_editor_property(
        "material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL
    )
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    mask = make_wetness_mask(material, -1450, -250)
    wetness = scalar(material, "WetnessAmount", 0.5, 0, 0)
    opacity = multiply(
        material,
        multiply(material, mask, wetness, 180, -80),
        scalar(material, "Opacity Strength", 0.72, 0, 100),
        360,
        -60,
    )
    tint = color(
        material,
        "Wet Tint",
        unreal.LinearColor(0.012, 0.035, 0.045, 1.0),
        180,
        180,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        tint, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        scalar(material, "Wet Roughness", 0.16, 180, 300),
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        scalar(material, "Wet Specular", 0.75, 180, 400),
        "",
        unreal.MaterialProperty.MP_SPECULAR,
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)


def build_puddle(mask_texture, normal_texture):
    material = create_material("M_BotanicusPuddleCartoon", replace=True)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("two_sided", True)
    material.set_editor_property("used_with_instanced_static_meshes", True)
    texcoord = node(material, unreal.MaterialExpressionTextureCoordinate, -1650, -150)
    water_amount = per_instance(material, 0, -1650, 70)
    impact_pulse = per_instance(material, 1, -1650, 170)
    shape_data = per_instance(material, 2, -1650, 270)
    mask_sample = make_atlas_mask(
        material, mask_texture, shape_data, texcoord, -1450, -140
    )

    border_power = node(material, unreal.MaterialExpressionPower, 300, -120)
    connect(mask_sample, border_power, "Base", "R")
    connect(
        scalar(material, "Border Softness", 1.8, 80, -20),
        border_power,
        "Exp",
    )
    edge_color = color(
        material,
        "Cartoon Edge Color",
        unreal.LinearColor(0.16, 0.68, 0.90, 1.0),
        80,
        100,
    )
    center_color = color(
        material,
        "Cartoon Center Color",
        unreal.LinearColor(0.012, 0.26, 0.48, 1.0),
        80,
        210,
    )
    body_color = node(
        material, unreal.MaterialExpressionLinearInterpolate, 360, 120
    )
    connect(edge_color, body_color, "A")
    connect(center_color, body_color, "B")
    connect(border_power, body_color, "Alpha")
    highlight = color(
        material,
        "White Surface Highlight",
        unreal.LinearColor(0.72, 0.96, 1.0, 1.0),
        320,
        270,
    )
    fresnel = node(material, unreal.MaterialExpressionFresnel, 520, 250)
    fresnel_amount = multiply(
        material,
        fresnel,
        scalar(material, "Fresnel Strength", 0.18, 500, 370),
        700,
        250,
    )
    final_color = node(
        material, unreal.MaterialExpressionLinearInterpolate, 880, 150
    )
    connect(body_color, final_color, "A")
    connect(highlight, final_color, "B")
    connect(fresnel_amount, final_color, "Alpha")

    opacity_mix = node(
        material, unreal.MaterialExpressionLinearInterpolate, 330, -330
    )
    connect(
        scalar(material, "Edge Opacity", 0.78, 90, -410), opacity_mix, "A"
    )
    connect(
        scalar(material, "Center Opacity", 0.54, 90, -320), opacity_mix, "B"
    )
    connect(border_power, opacity_mix, "Alpha")
    water_visibility = node(
        material, unreal.MaterialExpressionLinearInterpolate, 320, -470
    )
    connect(
        scalar(material, "Young Puddle Visibility", 0.82, 80, -560),
        water_visibility,
        "A",
    )
    connect(
        scalar(material, "Full Puddle Visibility", 1.0, 80, -650),
        water_visibility,
        "B",
    )
    connect(water_amount, water_visibility, "Alpha")
    opacity = multiply(
        material,
        multiply(material, mask_sample, opacity_mix, 550, -350),
        water_visibility,
        750,
        -350,
    )

    distance = centered_uv_distance(material, texcoord, -900, 520)
    ripple_radius = node(material, unreal.MaterialExpressionOneMinus, -150, 560)
    connect(impact_pulse, ripple_radius, "None")
    ripple_radius_scaled = multiply(
        material,
        ripple_radius,
        scalar(material, "Ripple Maximum Radius", 0.70, -120, 680),
        80,
        570,
    )
    ripple_difference = binary(
        material,
        unreal.MaterialExpressionSubtract,
        distance,
        ripple_radius_scaled,
        250,
        560,
    )
    ripple_abs = node(material, unreal.MaterialExpressionAbs, 420, 560)
    connect(ripple_difference, ripple_abs, "None")
    ripple_width = binary(
        material,
        unreal.MaterialExpressionDivide,
        ripple_abs,
        scalar(material, "Ripple Width", 0.055, 390, 690),
        590,
        560,
    )
    ripple_inverted = node(
        material, unreal.MaterialExpressionOneMinus, 760, 560
    )
    connect(ripple_width, ripple_inverted, "None")
    ripple_band = node(material, unreal.MaterialExpressionSaturate, 920, 560)
    connect(ripple_inverted, ripple_band, "None")
    ripple_strength = multiply(
        material,
        multiply(material, ripple_band, impact_pulse, 1090, 530),
        mask_sample,
        1270,
        520,
    )
    ripple_emissive = multiply(
        material,
        multiply(material, highlight, ripple_strength, 1440, 500),
        scalar(material, "Ripple Brightness", 0.72, 1240, 650),
        1630,
        500,
    )

    normal_uv = node(material, unreal.MaterialExpressionTextureCoordinate, 900, 760)
    normal_uv.set_editor_property("u_tiling", 2.2)
    normal_uv.set_editor_property("v_tiling", 2.2)
    panner_a = node(material, unreal.MaterialExpressionPanner, 1090, 730)
    panner_a.set_editor_property("speed_x", 0.018)
    panner_a.set_editor_property("speed_y", 0.011)
    connect(normal_uv, panner_a, "Coordinate")
    panner_b = node(material, unreal.MaterialExpressionPanner, 1090, 850)
    panner_b.set_editor_property("speed_x", -0.013)
    panner_b.set_editor_property("speed_y", 0.016)
    connect(normal_uv, panner_b, "Coordinate")
    normal_a = node(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 1280, 730
    )
    normal_a.set_editor_property("parameter_name", "Surface Normal A")
    normal_a.set_editor_property("texture", normal_texture)
    normal_a.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    )
    connect(panner_a, normal_a, "UVs")
    normal_b = node(
        material, unreal.MaterialExpressionTextureSampleParameter2D, 1280, 850
    )
    normal_b.set_editor_property("parameter_name", "Surface Normal B")
    normal_b.set_editor_property("texture", normal_texture)
    normal_b.set_editor_property(
        "sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
    )
    connect(panner_b, normal_b, "UVs")
    combined_normal = binary(
        material, unreal.MaterialExpressionAdd, normal_a, normal_b, 1490, 790
    )
    normalized_normal = node(
        material, unreal.MaterialExpressionNormalize, 1670, 790
    )
    connect(combined_normal, normalized_normal, "VectorInput")

    unreal.MaterialEditingLibrary.connect_material_property(
        final_color, "", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        ripple_emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        opacity, "", unreal.MaterialProperty.MP_OPACITY
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        scalar(material, "Roughness", 0.09, 1000, 320),
        "",
        unreal.MaterialProperty.MP_ROUGHNESS,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        scalar(material, "Specular", 0.78, 1000, 410),
        "",
        unreal.MaterialProperty.MP_SPECULAR,
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        normalized_normal, "", unreal.MaterialProperty.MP_NORMAL
    )
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)


unreal.EditorAssetLibrary.make_directory(MATERIAL_FOLDER)
unreal.EditorAssetLibrary.make_directory(TEXTURE_FOLDER)
mask_source, normal_source = generate_source_textures()
mask_texture = import_texture(
    mask_source,
    "T_PuddleMasks_Atlas",
    unreal.TextureCompressionSettings.TC_MASKS,
    False,
    True,
)
normal_texture = import_texture(
    normal_source,
    "T_PuddleNormal_Soft",
    unreal.TextureCompressionSettings.TC_NORMALMAP,
    False,
    False,
)
build_wetness()
build_puddle(mask_texture, normal_texture)
niagara_builder = os.path.join(
    unreal.Paths.project_content_dir(),
    "Python",
    "build_watering_niagara_systems.py",
)
with open(niagara_builder, "r", encoding="utf-8") as script:
    exec(compile(script.read(), niagara_builder, "exec"))
unreal.log("Botanicus: flaque cartoon organique reconstruite")
