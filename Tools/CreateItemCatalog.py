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
    price,
    delivery_delay,
    allowed_surfaces=1,
    world_mesh="/Engine/BasicShapes/Cube",
    world_actor_class=None,
    purchasable=True,
    whole_plant=False,
    sale_price=0,
    plant_color_tag="",
    plant_type_tag="",
    plant_quality_tag="Standard",
    visitor_appeal=50,
    catalog_tabs=4,
):
    definition = unreal.BotanicusItemDefinition()
    definition.set_editor_property("item_key", key)
    definition.set_editor_property("display_name", display_name)
    definition.set_editor_property("category", category)
    definition.set_editor_property("catalog_tabs", catalog_tabs)
    definition.set_editor_property("weight_class", weight)
    definition.set_editor_property(
        "carry_movement_speed_multiplier",
        carry_speed_multiplier,
    )
    definition.set_editor_property("world_mesh", unreal.load_asset(world_mesh))
    if world_actor_class is not None:
        definition.set_editor_property(
            "world_actor_class",
            world_actor_class,
        )
    definition.set_editor_property("world_scale", scale)
    definition.set_editor_property("maximum_stack", max_stack)
    definition.set_editor_property("whole_plant", whole_plant)
    definition.set_editor_property("delivery_quantity", delivery_quantity)
    definition.set_editor_property("price", price)
    definition.set_editor_property("purchasable", purchasable)
    definition.set_editor_property("sale_price", sale_price)
    definition.set_editor_property("plant_color_tag", plant_color_tag)
    definition.set_editor_property("plant_type_tag", plant_type_tag)
    definition.set_editor_property("plant_quality_tag", plant_quality_tag)
    definition.set_editor_property("visitor_appeal", visitor_appeal)
    definition.set_editor_property("delivery_delay_seconds", delivery_delay)
    definition.set_editor_property(
        "allowed_placement_surfaces",
        allowed_surfaces,
    )
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

solo_large = make_definition(
    "LargeEquipmentSolo_Test",
    "Gros equipement solo test",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY,
    unreal.Vector(0.8, 0.55, 0.65),
    1,
    1,
    0.7,
    350,
    6.0,
    purchasable=False,
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
    600,
    10.0,
    purchasable=False,
)
plant_pot_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusPlantPotActor",
)
if plant_pot_class is None:
    raise RuntimeError("Could not load BotanicusPlantPotActor")
plant_pot = make_definition(
    "PlantPot",
    "Pot de culture",
    unreal.BotanicusItemCategory.DECORATION,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.42, 0.42, 0.34),
    10,
    1,
    1.0,
    75,
    2.0,
    world_mesh="/Engine/BasicShapes/Cylinder",
    world_actor_class=plant_pot_class,
)
potting_soil = make_definition(
    "PottingSoil",
    "Dose de terreau",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.24, 0.24, 0.24),
    20,
    5,
    1.0,
    25,
    2.0,
    allowed_surfaces=1,
    catalog_tabs=4 | 8,
)
potting_soil.set_editor_property("sale_soil", True)
basil_seeds = make_definition(
    "SeedPacket_Basil",
    "Graines de basilic",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.18, 0.08, 0.24),
    20,
    5,
    1.0,
    30,
    2.0,
    allowed_surfaces=0,
    catalog_tabs=1,
)
orchid_seeds = make_definition(
    "SeedPacket_Orchid",
    "Graines d'orchidee rose",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.18, 0.08, 0.24),
    20,
    3,
    1.0,
    45,
    2.5,
    allowed_surfaces=0,
    catalog_tabs=1,
)
monstera_seeds = make_definition(
    "SeedPacket_Monstera",
    "Graines de monstera",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.18, 0.08, 0.24),
    20,
    3,
    1.0,
    40,
    2.5,
    allowed_surfaces=0,
    catalog_tabs=1,
)
lavender_seeds = make_definition(
    "SeedPacket_Lavender",
    "Graines de lavande",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.18, 0.08, 0.24),
    20,
    5,
    1.0,
    35,
    2.0,
    allowed_surfaces=0,
    catalog_tabs=1,
)
violet_seeds = make_definition(
    "SeedPacket_Violet",
    "Graines de violette",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.18, 0.08, 0.24),
    20,
    5,
    1.0,
    38,
    2.0,
    allowed_surfaces=0,
    catalog_tabs=1,
)
watering_can_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusWateringCanActor",
)
if watering_can_class is None:
    raise RuntimeError("Could not load BotanicusWateringCanActor")
watering_can = make_definition(
    "WateringCan",
    "Arrosoir",
    unreal.BotanicusItemCategory.TOOL,
    unreal.BotanicusItemWeightClass.HANDHELD,
    unreal.Vector(0.38, 0.2, 0.28),
    1,
    1,
    1.0,
    120,
    2.0,
    allowed_surfaces=0,
    world_actor_class=watering_can_class,
    catalog_tabs=2,
)
water_reserve_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusWaterReserveActor",
)
if water_reserve_class is None:
    raise RuntimeError("Could not load BotanicusWaterReserveActor")
water_reserve = make_definition(
    "WaterReserve",
    "Reserve d'eau",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.55, 0.55, 0.8),
    2,
    1,
    1.0,
    250,
    3.0,
    world_mesh="/Engine/BasicShapes/Cylinder",
    world_actor_class=water_reserve_class,
    catalog_tabs=2,
)
garden_trowel = make_definition(
    "GardenTrowel",
    "Petite pelle de jardinage",
    unreal.BotanicusItemCategory.TOOL,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.08, 0.32, 0.08),
    1,
    1,
    1.0,
    90,
    2.0,
    allowed_surfaces=0,
    catalog_tabs=2,
)
box_cutter = make_definition(
    "BoxCutter",
    "Cutter",
    unreal.BotanicusItemCategory.TOOL,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.20, 0.045, 0.035),
    1,
    1,
    1.0,
    35,
    1.5,
    allowed_surfaces=0,
    catalog_tabs=2,
)
basil_harvest = make_definition(
    "Harvest_Basil",
    "Plant de basilic",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.16, 0.16, 0.16),
    1,
    1,
    1.0,
    0,
    0.1,
    allowed_surfaces=0,
    purchasable=False,
    whole_plant=True,
    sale_price=60,
    plant_color_tag="Green",
    plant_type_tag="Aromatic",
    visitor_appeal=62,
    catalog_tabs=8,
)
orchid_harvest = make_definition(
    "Harvest_Orchid",
    "Orchidee rose",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.16, 0.16, 0.16),
    1,
    1,
    1.0,
    0,
    0.1,
    allowed_surfaces=0,
    purchasable=False,
    whole_plant=True,
    sale_price=110,
    plant_color_tag="Pink",
    plant_type_tag="Flowering",
    visitor_appeal=82,
    catalog_tabs=8,
)
monstera_harvest = make_definition(
    "Harvest_Monstera",
    "Monstera",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.16, 0.16, 0.16),
    1,
    1,
    1.0,
    0,
    0.1,
    allowed_surfaces=0,
    purchasable=False,
    whole_plant=True,
    sale_price=95,
    plant_color_tag="Green",
    plant_type_tag="Foliage",
    visitor_appeal=72,
    catalog_tabs=8,
)
lavender_harvest = make_definition(
    "Harvest_Lavender",
    "Lavande violette",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.16, 0.16, 0.16),
    1,
    1,
    1.0,
    0,
    0.1,
    allowed_surfaces=0,
    purchasable=False,
    whole_plant=True,
    sale_price=75,
    plant_color_tag="Purple",
    plant_type_tag="Aromatic",
    visitor_appeal=74,
    catalog_tabs=8,
)
violet_harvest = make_definition(
    "Harvest_Violet",
    "Violette",
    unreal.BotanicusItemCategory.SUPPLY,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.16, 0.16, 0.16),
    1,
    1,
    1.0,
    0,
    0.1,
    allowed_surfaces=0,
    purchasable=False,
    whole_plant=True,
    sale_price=85,
    plant_color_tag="Purple",
    plant_type_tag="Flowering",
    visitor_appeal=78,
    catalog_tabs=8,
)


def make_quality_harvest(
    base_key,
    display_name,
    base_price,
    color_tag,
    type_tag,
    base_appeal,
    suffix,
    quality_label,
    price_multiplier,
    appeal_bonus,
):
    return make_definition(
        f"{base_key}_{suffix}",
        f"{display_name} - {quality_label}",
        unreal.BotanicusItemCategory.SUPPLY,
        unreal.BotanicusItemWeightClass.HOTBAR,
        unreal.Vector(0.16, 0.16, 0.16),
        1,
        1,
        1.0,
        0,
        0.1,
        allowed_surfaces=0,
        purchasable=False,
        whole_plant=True,
        sale_price=int(base_price * price_multiplier + 0.5),
        plant_color_tag=color_tag,
        plant_type_tag=type_tag,
        plant_quality_tag=suffix,
        visitor_appeal=min(100, base_appeal + appeal_bonus),
        catalog_tabs=8,
    )


quality_harvests = []
for plant in (
    ("Harvest_Basil", "Plant de basilic", 60, "Green", "Aromatic", 62),
    ("Harvest_Orchid", "Orchidee rose", 110, "Pink", "Flowering", 82),
    ("Harvest_Monstera", "Monstera", 95, "Green", "Foliage", 72),
    ("Harvest_Lavender", "Lavande violette", 75, "Purple", "Aromatic", 74),
    ("Harvest_Violet", "Violette", 85, "Purple", "Flowering", 78),
):
    quality_harvests.append(
        make_quality_harvest(
            *plant,
            "Beautiful",
            "Belle qualite",
            1.35,
            10,
        )
    )
    quality_harvests.append(
        make_quality_harvest(
            *plant,
            "Exceptional",
            "Qualite exceptionnelle",
            1.75,
            22,
        )
    )
sales_display_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusSalesDisplayActor",
)
if sales_display_class is None:
    raise RuntimeError("Could not load BotanicusSalesDisplayActor")
sales_display = make_definition(
    "SalesDisplay",
    "Presentoir de vente",
    unreal.BotanicusItemCategory.DECORATION,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(1.15, 0.45, 0.65),
    2,
    1,
    1.0,
    200,
    3.0,
    world_actor_class=sales_display_class,
    catalog_tabs=8,
)
sale_pot_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusSalePotActor",
)
if sale_pot_class is None:
    raise RuntimeError("Could not load BotanicusSalePotActor")
sale_pot = make_definition(
    "SalePot",
    "Pot de vente",
    unreal.BotanicusItemCategory.DECORATION,
    unreal.BotanicusItemWeightClass.HOTBAR,
    unreal.Vector(0.28, 0.28, 0.22),
    10,
    1,
    1.0,
    10,
    2.0,
    world_mesh="/Engine/BasicShapes/Cylinder",
    world_actor_class=sale_pot_class,
    catalog_tabs=8,
)
sale_pot.set_editor_property(
    "collision_half_extent_override",
    unreal.Vector(28.0, 28.0, 22.0),
)
preparation_workbench_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusPreparationWorkbenchActor",
)
if preparation_workbench_class is None:
    raise RuntimeError("Could not load BotanicusPreparationWorkbenchActor")
preparation_workbench = make_definition(
    "PreparationWorkbench",
    "Etabli de preparation",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY,
    unreal.Vector(1.2, 0.6, 0.12),
    1,
    1,
    0.7,
    350,
    4.0,
    world_actor_class=preparation_workbench_class,
    catalog_tabs=4,
    purchasable=False,
)
work_surface_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusWorkSurfaceActor",
)
if work_surface_class is None:
    raise RuntimeError("Could not load BotanicusWorkSurfaceActor")


def make_work_surface(key, name, scale, extent, price):
    surface = make_definition(
        key,
        name,
        unreal.BotanicusItemCategory.EQUIPMENT,
        unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY,
        scale,
        1,
        1,
        0.70,
        price,
        3.0,
        allowed_surfaces=1,
        world_mesh="/Engine/BasicShapes/Cube",
        world_actor_class=work_surface_class,
        catalog_tabs=4,
    )
    surface.set_editor_property(
        "collision_half_extent_override",
        extent,
    )
    return surface


work_surfaces = [
    make_work_surface(
        "WorkSurfaceSmall",
        "Petit plan de travail",
        unreal.Vector(1.2, 0.6, 0.9),
        unreal.Vector(60.0, 30.0, 45.0),
        180,
    ),
    make_work_surface(
        "WorkSurfaceMedium",
        "Plan de travail moyen",
        unreal.Vector(2.0, 0.7, 0.9),
        unreal.Vector(100.0, 35.0, 45.0),
        280,
    ),
    make_work_surface(
        "WorkSurfaceLarge",
        "Grand plan de travail",
        unreal.Vector(3.0, 0.8, 0.9),
        unreal.Vector(150.0, 40.0, 45.0),
        420,
    ),
]
storage_shelf_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusStorageShelfActor",
)
if storage_shelf_class is None:
    raise RuntimeError("Could not load BotanicusStorageShelfActor")


def make_storage_shelf(key, name, scale, extent, price, wall=False):
    shelf = make_definition(
        key,
        name,
        unreal.BotanicusItemCategory.EQUIPMENT,
        unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY,
        scale,
        1,
        1,
        0.75,
        price,
        3.0,
        allowed_surfaces=4 if wall else 1,
        world_mesh="/Engine/BasicShapes/Cube",
        world_actor_class=storage_shelf_class,
        catalog_tabs=4,
    )
    shelf.set_editor_property("collision_half_extent_override", extent)
    return shelf


storage_shelves = [
    make_storage_shelf(
        "StorageShelfFloorSmall",
        "Etagere au sol - 4 places",
        unreal.Vector(0.6, 1.7, 1.5),
        unreal.Vector(30.0, 85.0, 75.0),
        180,
    ),
    make_storage_shelf(
        "StorageShelfFloorLarge",
        "Etagere au sol - 8 places",
        unreal.Vector(0.7, 2.5, 1.8),
        unreal.Vector(35.0, 125.0, 90.0),
        320,
    ),
    make_storage_shelf(
        "StorageShelfWallSmall",
        "Etagere murale - 3 places",
        unreal.Vector(0.32, 1.3, 0.5),
        unreal.Vector(16.0, 65.0, 25.0),
        120,
        wall=True,
    ),
    make_storage_shelf(
        "StorageShelfWallLarge",
        "Etagere murale - 6 places",
        unreal.Vector(0.36, 1.9, 0.96),
        unreal.Vector(18.0, 95.0, 48.0),
        220,
        wall=True,
    ),
]
computer_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusComputerActor",
)
if computer_class is None:
    raise RuntimeError("Could not load BotanicusComputerActor")
command_computer = make_definition(
    "CommandComputer",
    "Ordinateur de commande",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.HANDHELD,
    unreal.Vector(0.5, 0.12, 0.34),
    1,
    1,
    1.0,
    250,
    3.0,
    world_mesh="/Engine/BasicShapes/Cube",
    world_actor_class=computer_class,
    catalog_tabs=4,
    purchasable=False,
)
command_computer.set_editor_property(
    "collision_half_extent_override",
    unreal.Vector(55.0, 32.0, 42.0),
)
cash_register_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusCashRegisterActor",
)
if cash_register_class is None:
    raise RuntimeError("Could not load BotanicusCashRegisterActor")
cash_register = make_definition(
    "CashRegister",
    "Caisse",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.HANDHELD,
    unreal.Vector(0.75, 0.42, 0.55),
    1,
    1,
    1.0,
    0,
    3.0,
    world_mesh="/Engine/BasicShapes/Cube",
    world_actor_class=cash_register_class,
    catalog_tabs=8,
    purchasable=False,
)
cash_register.set_editor_property(
    "collision_half_extent_override",
    unreal.Vector(75.0, 45.0, 60.0),
)
self_checkout_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusSelfCheckoutActor",
)
if self_checkout_class is None:
    raise RuntimeError("Could not load BotanicusSelfCheckoutActor")
self_checkout = make_definition(
    "SelfCheckout",
    "Caisse automatique",
    unreal.BotanicusItemCategory.EQUIPMENT,
    unreal.BotanicusItemWeightClass.HANDHELD,
    unreal.Vector(0.6, 0.4, 2.0),
    1,
    1,
    1.0,
    1500,
    5.0,
    world_mesh="/Engine/BasicShapes/Cube",
    world_actor_class=self_checkout_class,
    catalog_tabs=8,
)
self_checkout.set_editor_property(
    "collision_half_extent_override",
    unreal.Vector(30.0, 20.0, 100.0),
)
asset.set_editor_property(
    "items",
    [
        solo_large,
        large,
        plant_pot,
        potting_soil,
        basil_seeds,
        orchid_seeds,
        monstera_seeds,
        lavender_seeds,
        violet_seeds,
        watering_can,
        water_reserve,
        garden_trowel,
        box_cutter,
        basil_harvest,
        orchid_harvest,
        monstera_harvest,
        lavender_harvest,
        violet_harvest,
        *quality_harvests,
        sales_display,
        sale_pot,
        preparation_workbench,
        *work_surfaces,
        *storage_shelves,
        command_computer,
        cash_register,
        self_checkout,
    ],
)
if not unreal.EditorAssetLibrary.save_loaded_asset(
    asset,
    only_if_is_dirty=False,
):
    raise RuntimeError(
        "Could not save item catalog. Check that the asset is not read-only: "
        + ASSET_PATH
    )

test_seed_asset = "/Game/Botanicus/Items/DA_SeedPacket_Test"
if unreal.EditorAssetLibrary.does_asset_exist(test_seed_asset):
    if not unreal.EditorAssetLibrary.delete_asset(test_seed_asset):
        raise RuntimeError("Could not delete obsolete test seed asset")

unreal.log("Botanicus item catalog created: " + ASSET_PATH)
unreal.SystemLibrary.quit_editor()
