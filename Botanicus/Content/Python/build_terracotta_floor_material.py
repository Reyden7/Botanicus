"""Import sol.png and build a scale-independent, world-tiled floor material."""

from pathlib import Path
import unreal


FOLDER = "/Game/Botanicus/Materials/Floor"
TEXTURE_NAME = "T_Floor_Terracotta_Sol"
MATERIAL_NAME = "M_Floor_Terracotta_WorldTiled"
INSTANCE_NAME = "MI_Floor_Terracotta_20cm"
FLOOR_MESH = "/Game/Botanicus/Items/itemsMesh/kit_modulaire/sol1/sol1"
SOURCE = Path(unreal.Paths.project_content_dir()) / "Python" / "FloorTextureSources" / "sol.png"


def asset_path(name):
    return f"{FOLDER}/{name}"


def expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )


def connect(source, target, input_name, output_name=""):
    connected = unreal.MaterialEditingLibrary.connect_material_expressions(
        source, output_name, target, input_name
    )
    if connected is False:
        raise RuntimeError(f"Could not connect {source} to {target}.{input_name}")


def scalar(material, name, value, x, y):
    node = expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


def create_or_load(name, asset_class, factory):
    existing = unreal.load_asset(asset_path(name))
    if existing:
        if not isinstance(existing, asset_class):
            raise RuntimeError(f"Unexpected asset type: {asset_path(name)}")
        return existing
    result = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        name, FOLDER, asset_class, factory
    )
    if not isinstance(result, asset_class):
        raise RuntimeError(f"Could not create {asset_path(name)}")
    return result


def build():
    if not SOURCE.is_file():
        raise RuntimeError(f"Missing floor source image: {SOURCE}")
    unreal.EditorAssetLibrary.make_directory(FOLDER)

    texture = unreal.load_asset(asset_path(TEXTURE_NAME))
    if not isinstance(texture, unreal.Texture2D):
        task = unreal.AssetImportTask()
        task.filename = str(SOURCE)
        task.destination_path = FOLDER
        task.destination_name = TEXTURE_NAME
        task.automated = True
        task.replace_existing = False
        task.save = True
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
        texture = unreal.load_asset(asset_path(TEXTURE_NAME))
    if not isinstance(texture, unreal.Texture2D):
        raise RuntimeError("Floor texture import failed")
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_WRAP)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)

    material = create_or_load(
        MATERIAL_NAME, unreal.Material, unreal.MaterialFactoryNew()
    )
    unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

    # The source image contains six tiles along each axis. Projecting the
    # horizontal world position in centimetres makes their physical size
    # independent of the floor mesh UVs and actor scale. Adjacent pieces align.
    world_position = expression(
        material, unreal.MaterialExpressionWorldPosition, -1000, -220
    )
    xy = expression(material, unreal.MaterialExpressionComponentMask, -780, -220)
    xy.set_editor_property("r", True)
    xy.set_editor_property("g", True)
    xy.set_editor_property("b", False)
    xy.set_editor_property("a", False)
    connect(world_position, xy, "None")

    tile_size = scalar(material, "TailleCarreau_cm", 20.0, -1000, 70)
    tiles_per_image = scalar(material, "CarreauxParImage", 6.0, -1000, 230)
    image_size = expression(material, unreal.MaterialExpressionMultiply, -780, 130)
    connect(tile_size, image_size, "A")
    connect(tiles_per_image, image_size, "B")
    minimum_size = expression(material, unreal.MaterialExpressionMax, -560, 130)
    connect(image_size, minimum_size, "A")
    one = expression(material, unreal.MaterialExpressionConstant, -780, 340)
    one.set_editor_property("r", 1.0)
    connect(one, minimum_size, "B")

    world_uv = expression(material, unreal.MaterialExpressionDivide, -330, -130)
    connect(xy, world_uv, "A")
    connect(minimum_size, world_uv, "B")

    sample = expression(
        material, unreal.MaterialExpressionTextureSampleParameter2D, -80, -130
    )
    sample.set_editor_property("parameter_name", "TextureSol")
    sample.set_editor_property("texture", texture)
    connect(world_uv, sample, "UVs")

    roughness = scalar(material, "Rugosite", 0.82, -100, 170)
    unreal.MaterialEditingLibrary.connect_material_property(
        sample, "RGB", unreal.MaterialProperty.MP_BASE_COLOR
    )
    unreal.MaterialEditingLibrary.connect_material_property(
        roughness, "", unreal.MaterialProperty.MP_ROUGHNESS
    )
    material.set_editor_property("two_sided", False)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)

    instance = create_or_load(
        INSTANCE_NAME,
        unreal.MaterialInstanceConstant,
        unreal.MaterialInstanceConstantFactoryNew(),
    )
    instance.set_editor_property("parent", material)
    unreal.MaterialEditingLibrary.set_material_instance_scalar_parameter_value(
        instance, "TailleCarreau_cm", 20.0
    )
    unreal.EditorAssetLibrary.save_loaded_asset(instance, only_if_is_dirty=False)

    floor_mesh = unreal.load_asset(FLOOR_MESH)
    if not isinstance(floor_mesh, unreal.StaticMesh):
        raise RuntimeError(f"Floor mesh not found: {FLOOR_MESH}")
    if len(floor_mesh.get_editor_property("static_materials")) != 1:
        raise RuntimeError("Expected exactly one material slot on sol1")
    floor_mesh.set_material(0, instance)
    unreal.EditorAssetLibrary.save_loaded_asset(floor_mesh, only_if_is_dirty=False)
    unreal.log(f"FLOOR_BUILD_OK {asset_path(INSTANCE_NAME)}")


build()
