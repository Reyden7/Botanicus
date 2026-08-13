import unreal


PACKAGE_PATH = "/Game/Botanicus/Materials/Silhouette"
ASSET_NAME = "M_Silhouette_Hologram_Blue"
ASSET_PATH = f"{PACKAGE_PATH}/{ASSET_NAME}"


def make_expression(material, expression_class, x, y):
    return unreal.MaterialEditingLibrary.create_material_expression(
        material, expression_class, x, y
    )


def scalar_parameter(material, name, default, x, y):
    expression = make_expression(
        material, unreal.MaterialExpressionScalarParameter, x, y
    )
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", default)
    return expression


def vector_parameter(material, name, default, x, y):
    expression = make_expression(
        material, unreal.MaterialExpressionVectorParameter, x, y
    )
    expression.set_editor_property("parameter_name", name)
    expression.set_editor_property("default_value", default)
    return expression


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
material = unreal.load_asset(ASSET_PATH)
if material is None:
    material = asset_tools.create_asset(
        ASSET_NAME,
        PACKAGE_PATH,
        unreal.Material,
        unreal.MaterialFactoryNew(),
    )

material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
material.set_editor_property("two_sided", True)
unreal.MaterialEditingLibrary.delete_all_material_expressions(material)

# Animated horizontal scan lines in world space.
world_position = make_expression(material, unreal.MaterialExpressionWorldPosition, -1100, 260)
world_z = make_expression(material, unreal.MaterialExpressionComponentMask, -900, 260)
world_z.set_editor_property("r", False)
world_z.set_editor_property("g", False)
world_z.set_editor_property("b", True)
unreal.MaterialEditingLibrary.connect_material_expressions(world_position, "", world_z, "")

line_density = scalar_parameter(material, "ScanLineDensity", 0.075, -900, 390)
scaled_z = make_expression(material, unreal.MaterialExpressionMultiply, -680, 280)
unreal.MaterialEditingLibrary.connect_material_expressions(world_z, "", scaled_z, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(line_density, "", scaled_z, "B")

time = make_expression(material, unreal.MaterialExpressionTime, -900, 520)
speed = scalar_parameter(material, "ScanSpeed", 1.8, -900, 620)
moving_time = make_expression(material, unreal.MaterialExpressionMultiply, -680, 500)
unreal.MaterialEditingLibrary.connect_material_expressions(time, "", moving_time, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(speed, "", moving_time, "B")

scan_phase = make_expression(material, unreal.MaterialExpressionAdd, -470, 350)
unreal.MaterialEditingLibrary.connect_material_expressions(scaled_z, "", scan_phase, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(moving_time, "", scan_phase, "B")
scan_sine = make_expression(material, unreal.MaterialExpressionSine, -270, 350)
scan_sine.set_editor_property("period", 0.18)
unreal.MaterialEditingLibrary.connect_material_expressions(scan_phase, "", scan_sine, "")

scan_scale = scalar_parameter(material, "ScanLineIntensity", 0.22, -260, 500)
scan = make_expression(material, unreal.MaterialExpressionMultiply, -50, 390)
unreal.MaterialEditingLibrary.connect_material_expressions(scan_sine, "", scan, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(scan_scale, "", scan, "B")

# Brighter silhouette edges, typical of a hologram projection.
fresnel = make_expression(material, unreal.MaterialExpressionFresnel, -260, 80)
fresnel_scale = scalar_parameter(material, "EdgeIntensity", 1.8, -60, 170)
edge_glow = make_expression(material, unreal.MaterialExpressionMultiply, 150, 80)
unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", edge_glow, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(fresnel_scale, "", edge_glow, "B")

base_intensity = scalar_parameter(material, "EmissiveIntensity", 1.35, 140, 260)
glow_plus_base = make_expression(material, unreal.MaterialExpressionAdd, 350, 150)
unreal.MaterialEditingLibrary.connect_material_expressions(edge_glow, "", glow_plus_base, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(base_intensity, "", glow_plus_base, "B")
total_glow = make_expression(material, unreal.MaterialExpressionAdd, 550, 180)
unreal.MaterialEditingLibrary.connect_material_expressions(glow_plus_base, "", total_glow, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(scan, "", total_glow, "B")

cyan = vector_parameter(
    material,
    "HologramColor",
    unreal.LinearColor(0.015, 0.34, 1.0, 1.0),
    350,
    -20,
)
emissive = make_expression(material, unreal.MaterialExpressionMultiply, 760, 90)
unreal.MaterialEditingLibrary.connect_material_expressions(cyan, "", emissive, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(total_glow, "", emissive, "B")
unreal.MaterialEditingLibrary.connect_material_property(
    emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR
)

# Transparent body with visible scan lines and edges.
opacity_base = scalar_parameter(material, "BodyOpacity", 0.16, 140, 570)
opacity_edge_scale = scalar_parameter(material, "EdgeOpacity", 0.42, 140, 680)
opacity_edge = make_expression(material, unreal.MaterialExpressionMultiply, 350, 610)
unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", opacity_edge, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(opacity_edge_scale, "", opacity_edge, "B")
opacity_with_edge = make_expression(material, unreal.MaterialExpressionAdd, 550, 570)
unreal.MaterialEditingLibrary.connect_material_expressions(opacity_base, "", opacity_with_edge, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(opacity_edge, "", opacity_with_edge, "B")
opacity = make_expression(material, unreal.MaterialExpressionAdd, 760, 540)
unreal.MaterialEditingLibrary.connect_material_expressions(opacity_with_edge, "", opacity, "A")
unreal.MaterialEditingLibrary.connect_material_expressions(scan, "", opacity, "B")
unreal.MaterialEditingLibrary.connect_material_property(
    opacity, "", unreal.MaterialProperty.MP_OPACITY
)

unreal.MaterialEditingLibrary.recompile_material(material)
unreal.EditorAssetLibrary.save_loaded_asset(material)

# Remove the obsolete overlay version created by the first implementation.
for obsolete_asset in (
    "/Game/Botanicus/Materials/Placement/MI_Silhouette_Hologram_Blue",
    "/Game/Botanicus/Materials/Placement/M_HologramPlacement",
):
    if unreal.EditorAssetLibrary.does_asset_exist(obsolete_asset):
        unreal.EditorAssetLibrary.delete_asset(obsolete_asset)

unreal.log_warning(f"Created editable silhouette material: {ASSET_PATH}")
