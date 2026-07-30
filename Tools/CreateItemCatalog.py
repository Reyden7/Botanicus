import unreal


ASSET_PATH = "/Game/Botanicus/Data/DA_BotanicusItemCatalog"


def make_definition(
    key,
    display_name,
    category,
    weight,
    scale,
    max_stack,
    delivery_quantity,
    carry_speed_multiplier,
):
    definition = unreal.BotanicusItemDefinition()
    definition.set_editor_property("item_key", key)
    definition.set_editor_property("display_name", display_name)
    definition.set_editor_property("category", category)
    definition.set_editor_property("weight_class", weight)
    definition.set_editor_property(
        "carry_movement_speed_multiplier",
        carry_speed_multiplier,
    )
    definition.set_editor_property("world_mesh", unreal.load_asset("/Engine/BasicShapes/Cube"))
    definition.set_editor_property("world_scale", scale)
    definition.set_editor_property("maximum_stack", max_stack)
    definition.set_editor_property("delivery_quantity", delivery_quantity)
    definition.set_editor_property("allowed_placement_surfaces", 1)
    definition.set_editor_property("rotation_step", 5.0)
    definition.set_editor_property("fine_rotation_step", 1.0)
    definition.set_editor_property("show_alignment_guides", True)
    definition.set_editor_property("alignment_edge_tolerance", 5.0)
    definition.set_editor_property("alignment_angle_tolerance", 2.0)
    return definition


asset = unreal.load_asset(ASSET_PATH)
if asset is None:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.BotanicusItemCatalog)
    asset = asset_tools.create_asset(
        "DA_BotanicusItemCatalog",
        "/Game/Botanicus/Data",
        unreal.BotanicusItemCatalog,
        factory,
    )

if asset is None:
    raise RuntimeError("Could not create DA_BotanicusItemCatalog")

small = make_definition(
    "SeedPacket_Test",
    "Paquet de graines test",
    unreal.BotanicusItemCategory.DECORATION,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.4, 0.4, 0.4),
    99,
    10,
    1.0,
)
solo_large = make_definition(
    "LargeEquipmentSolo_Test",
    "Gros equipement solo test",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY,
    unreal.Vector(0.8, 0.55, 0.65),
    1,
    1,
    0.7,
)
large = make_definition(
    "LargeEquipment_Test",
    "Gros equipement test",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.TWO_PLAYER_CARRY,
    unreal.Vector(1.0, 0.65, 0.75),
    1,
    1,
    0.55,
)
asset.set_editor_property("items", [small, solo_large, large])
unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

unreal.log("Botanicus item catalog created: " + ASSET_PATH)
unreal.SystemLibrary.quit_editor()
