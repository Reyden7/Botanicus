import unreal


LEGACY_ASSET_PATH = "/Game/Botanicus/blueprints"
ASSET_PATH = "/Game/Botanicus/blueprints/zones"


def migrate_legacy_asset(asset_name):
    source = f"{LEGACY_ASSET_PATH}/{asset_name}"
    destination = f"{ASSET_PATH}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        return
    if unreal.EditorAssetLibrary.does_asset_exist(source):
        if not unreal.EditorAssetLibrary.rename_asset(source, destination):
            raise RuntimeError(
                f"Unable to move asset from {source} to {destination}"
            )


for legacy_asset_name in (
    "BP_Zone_Parking",
    "BP_Zone_Vente",
    "BP_Zone_Caisse",
    "BP_Zone_Livraison",
    "BP_Zone_Remboursement",
    "BP_Route",
    "BP_Route_Joueur",
):
    migrate_legacy_asset(legacy_asset_name)


def create_actor_blueprint(asset_name, parent_class_path):
    existing = unreal.load_asset(f"{ASSET_PATH}/{asset_name}")
    if existing:
        unreal.log(f"Blueprint already exists: {asset_name}")
        return existing

    parent_class = unreal.load_class(None, parent_class_path)
    if not parent_class:
        raise RuntimeError(f"Unable to load parent class: {parent_class_path}")

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        asset_name,
        ASSET_PATH,
        unreal.Blueprint,
        factory,
    )
    if not blueprint:
        raise RuntimeError(f"Unable to create Blueprint: {asset_name}")
    return blueprint


parking = create_actor_blueprint(
    "BP_Zone_Parking",
    "/Script/Botanicus.BotanicusVisitorZoneActor",
)
parking_cdo = unreal.get_default_object(parking.generated_class())
parking_cdo.set_editor_property(
    "zone_type", unreal.BotanicusVisitorZoneType.PARKING
)
parking_cdo.set_editor_property("box_extent", unreal.Vector(500.0, 400.0, 8.0))

sales = create_actor_blueprint(
    "BP_Zone_Vente",
    "/Script/Botanicus.BotanicusVisitorZoneActor",
)
sales_cdo = unreal.get_default_object(sales.generated_class())
sales_cdo.set_editor_property(
    "zone_type", unreal.BotanicusVisitorZoneType.SALES_AREA
)
sales_cdo.set_editor_property("box_extent", unreal.Vector(600.0, 500.0, 8.0))

checkout = create_actor_blueprint(
    "BP_Zone_Caisse",
    "/Script/Botanicus.BotanicusVisitorZoneActor",
)
checkout_cdo = unreal.get_default_object(checkout.generated_class())
checkout_cdo.set_editor_property(
    "zone_type", unreal.BotanicusVisitorZoneType.CHECKOUT
)
checkout_cdo.set_editor_property("box_extent", unreal.Vector(300.0, 250.0, 8.0))

delivery = create_actor_blueprint(
    "BP_Zone_Livraison",
    "/Script/Botanicus.BotanicusDeliveryZoneActor",
)
delivery_cdo = unreal.get_default_object(delivery.generated_class())
delivery_cdo.set_editor_property("zone_size", unreal.Vector2D(480.0, 480.0))

refund = create_actor_blueprint(
    "BP_Zone_Remboursement",
    "/Script/Botanicus.BotanicusRefundZoneActor",
)
refund_cdo = unreal.get_default_object(refund.generated_class())
refund_cdo.set_editor_property("box_extent", unreal.Vector(220.0, 150.0, 6.0))

route = create_actor_blueprint(
    "BP_Route",
    "/Script/Botanicus.BotanicusPathActor",
)
route_cdo = unreal.get_default_object(route.generated_class())
route_cdo.set_editor_property(
    "path_type", unreal.BotanicusPathType.VISITOR_ROUTE
)
route_cdo.set_editor_property("path_width", 220.0)
route_cdo.set_editor_property(
    "editor_path_points",
    [unreal.Vector(-500.0, 0.0, 0.0), unreal.Vector(500.0, 0.0, 0.0)],
)

player_route = create_actor_blueprint(
    "BP_Route_Joueur",
    "/Script/Botanicus.BotanicusPathActor",
)
player_route_cdo = unreal.get_default_object(player_route.generated_class())
player_route_cdo.set_editor_property(
    "path_type", unreal.BotanicusPathType.STANDARD
)
player_route_cdo.set_editor_property("path_width", 220.0)
player_route_cdo.set_editor_property(
    "editor_path_points",
    [unreal.Vector(-500.0, 0.0, 0.0), unreal.Vector(500.0, 0.0, 0.0)],
)

for asset in (
    parking,
    sales,
    checkout,
    delivery,
    refund,
    route,
    player_route,
):
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

unreal.log("Starter zone Blueprints created successfully.")
