import re
import unreal


CATALOG_PATH = "/Game/Botanicus/Data/DA_BotanicusItemCatalog"
OUTPUT_FOLDER = "/Game/Botanicus/blueprints"
LEGACY_OUTPUT_FOLDER = "/Game/Botanicus/Items/Blueprints"
PREPARATION_WORKBENCH_PATH = OUTPUT_FOLDER + "/BP_WorkBench"
GENERIC_PARENT_PATH = "/Script/Botanicus.BotanicusPlaceableItemActor"
LARGE_EQUIPMENT_PARENT_PATH = "/Script/Botanicus.BotanicusLargeEquipmentActor"
SPECIALIZED_PARENT_PATHS = {
	"PlantPot": "/Script/Botanicus.BotanicusPlantPotActor",
    "PlantPotSquare": "/Script/Botanicus.BotanicusPlantPotActor",
    "WateringCan": "/Script/Botanicus.BotanicusWateringCanActor",
    "WaterReserve": "/Script/Botanicus.BotanicusWaterReserveActor",
	"SalePot": "/Script/Botanicus.BotanicusSalePotActor",
    "SalePotSquare": "/Script/Botanicus.BotanicusSalePotActor",
    "SalesDisplay": "/Script/Botanicus.BotanicusSalesDisplayActor",
    "PreparationWorkbench": (
        "/Game/Botanicus/blueprints/BP_WorkBench.BP_WorkBench_C"
    ),
    "CommandComputer": "/Script/Botanicus.BotanicusComputerActor",
    "CashRegister": "/Script/Botanicus.BotanicusCashRegisterActor",
    "SelfCheckout": "/Script/Botanicus.BotanicusSelfCheckoutActor",
    "WorkSurfaceSmall": "/Script/Botanicus.BotanicusWorkSurfaceActor",
    "WorkSurfaceMedium": "/Script/Botanicus.BotanicusWorkSurfaceActor",
    "WorkSurfaceLarge": "/Script/Botanicus.BotanicusWorkSurfaceActor",
    "StorageShelfFloorSmall": "/Script/Botanicus.BotanicusStorageShelfActor",
    "StorageShelfFloorLarge": "/Script/Botanicus.BotanicusStorageShelfActor",
    "StorageShelfWallSmall": "/Script/Botanicus.BotanicusStorageShelfActor",
    "StorageShelfWallLarge": "/Script/Botanicus.BotanicusStorageShelfActor",
}


def log(message):
    unreal.log("[Botanicus Item Blueprints] " + message)


def sanitize_asset_name(item_key):
    cleaned = re.sub(r"[^A-Za-z0-9_]", "_", str(item_key))
    return "BP_Item_" + cleaned


def blueprint_asset_path(item_key):
    if item_key == "PreparationWorkbench":
        return PREPARATION_WORKBENCH_PATH
    return OUTPUT_FOLDER + "/" + sanitize_asset_name(item_key)


def migrate_generated_blueprints(definitions):
    """Move the generated assets through Unreal so references follow them."""
    for definition in definitions:
        item_key = str(definition.get_editor_property("item_key"))
        if not item_key or item_key in ("None", "PreparationWorkbench"):
            continue
        asset_name = sanitize_asset_name(item_key)
        old_path = LEGACY_OUTPUT_FOLDER + "/" + asset_name
        new_path = OUTPUT_FOLDER + "/" + asset_name
        if (
            unreal.EditorAssetLibrary.does_asset_exist(old_path)
            and not unreal.EditorAssetLibrary.does_asset_exist(new_path)
        ):
            if not unreal.EditorAssetLibrary.rename_asset(old_path, new_path):
                raise RuntimeError("Could not move " + old_path + " to " + new_path)
            log("Moved " + old_path + " to " + new_path)


def load_soft_class(soft_class):
    if soft_class is None:
        return None
    if isinstance(soft_class, unreal.Class):
        return soft_class
    try:
        loaded = soft_class.load_synchronous()
        if loaded:
            return loaded
    except Exception:
        pass
    try:
        path = str(soft_class)
        if path and path != "None":
            return unreal.load_class(None, path)
    except Exception:
        pass
    return None


def build_item_data_map():
    result = {}
    assets = unreal.EditorAssetLibrary.list_assets(
        "/Game/Botanicus/Items", recursive=True, include_folder=False
    )
    for asset_path in assets:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        if asset is None or not hasattr(asset, "get_item_key"):
            continue
        try:
            key = str(asset.get_item_key())
            if key and key != "None":
                result[key] = asset
        except Exception:
            continue
    return result


def set_blueprint_default_appearance(blueprint, item_data, definition):
    generated_class = blueprint.generated_class()
    if generated_class is None:
        raise RuntimeError("Generated class unavailable for " + blueprint.get_name())
    cdo = unreal.get_default_object(generated_class)
    cdo.set_editor_property("use_blueprint_appearance", True)

    mesh_component = cdo.get_editor_property("mesh")
    item_mesh = None
    uses_item_data_mesh = False
    if item_data is not None:
        try:
            mesh_soft = item_data.get_item_static_mesh()
            item_mesh = mesh_soft.load_synchronous() if mesh_soft else None
            uses_item_data_mesh = item_mesh is not None
        except Exception:
            item_mesh = None

    if item_mesh is None:
        try:
            mesh_soft = definition.get_editor_property("world_mesh")
            item_mesh = mesh_soft.load_synchronous() if mesh_soft else None
        except Exception:
            item_mesh = None

    if mesh_component is not None and item_mesh is not None:
        mesh_component.set_editor_property("static_mesh", item_mesh)
        if uses_item_data_mesh:
            mesh_component.set_editor_property(
                "relative_scale3d", unreal.Vector(1.0, 1.0, 1.0)
            )
        else:
            mesh_component.set_editor_property(
                "relative_scale3d",
                definition.get_editor_property("world_scale"),
            )


def create_or_update_blueprint(asset_tools, definition, item_data):
    item_key = str(definition.get_editor_property("item_key"))
    if item_key == "PreparationWorkbench":
        blueprint = unreal.EditorAssetLibrary.load_asset(
            PREPARATION_WORKBENCH_PATH
        )
        if blueprint is None or blueprint.generated_class() is None:
            raise RuntimeError(
                "Existing preparation workbench Blueprint not found: "
                + PREPARATION_WORKBENCH_PATH
            )
        cdo = unreal.get_default_object(blueprint.generated_class())
        cdo.set_editor_property("use_blueprint_appearance", True)
        unreal.EditorAssetLibrary.save_loaded_asset(
            blueprint, only_if_is_dirty=False
        )
        log("Using existing " + PREPARATION_WORKBENCH_PATH)
        return blueprint

    asset_name = sanitize_asset_name(item_key)
    asset_path = blueprint_asset_path(item_key)

    blueprint = None
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
    if blueprint is None:
        parent_class = None
        specialized_parent_path = SPECIALIZED_PARENT_PATHS.get(item_key)
        if specialized_parent_path:
            parent_class = unreal.load_class(None, specialized_parent_path)
        if parent_class is None:
            parent_soft = definition.get_editor_property("world_actor_class")
            parent_class = load_soft_class(parent_soft)
        if parent_class is None:
            weight_class = definition.get_editor_property("weight_class")
            if weight_class in (
                unreal.BotanicusItemWeightClass.ONE_PLAYER_CARRY,
                unreal.BotanicusItemWeightClass.TWO_PLAYER_CARRY,
            ):
                parent_class = unreal.load_class(
                    None, LARGE_EQUIPMENT_PARENT_PATH
                )
            else:
                parent_class = unreal.load_class(None, GENERIC_PARENT_PATH)
        if parent_class is None:
            raise RuntimeError("Parent class unavailable for " + item_key)

        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        blueprint = asset_tools.create_asset(
            asset_name, OUTPUT_FOLDER, None, factory
        )
        if blueprint is None:
            raise RuntimeError("Blueprint creation failed for " + item_key)
        log("Created " + asset_path + " from " + parent_class.get_name())
    else:
        log("Updated " + asset_path)

    set_blueprint_default_appearance(blueprint, item_data, definition)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    return blueprint


def main():
    catalog = unreal.EditorAssetLibrary.load_asset(CATALOG_PATH)
    if catalog is None:
        raise RuntimeError("Catalog not found: " + CATALOG_PATH)

    unreal.EditorAssetLibrary.make_directory(OUTPUT_FOLDER)
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    item_data_by_key = build_item_data_map()
    definitions = list(catalog.get_editor_property("items"))
    if not definitions:
        raise RuntimeError("The Botanicus item catalog is empty")

    migrate_generated_blueprints(definitions)

    updated_definitions = []
    generated_count = 0
    for definition in definitions:
        item_key = str(definition.get_editor_property("item_key"))
        if not item_key or item_key == "None":
            updated_definitions.append(definition)
            continue

        blueprint = create_or_update_blueprint(
            asset_tools, definition, item_data_by_key.get(item_key)
        )
        generated_class = blueprint.generated_class()
        definition.set_editor_property(
            "world_actor_class",
            generated_class,
        )
        updated_definitions.append(definition)
        generated_count += 1

    catalog.set_editor_property("items", updated_definitions)
    unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
    unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
        save_map_packages=False, save_content_packages=True
    )

    redundant_workbench_path = (
        LEGACY_OUTPUT_FOLDER + "/BP_Item_PreparationWorkbench"
    )
    if unreal.EditorAssetLibrary.does_asset_exist(redundant_workbench_path):
        if not unreal.EditorAssetLibrary.delete_asset(redundant_workbench_path):
            raise RuntimeError(
                "Could not remove redundant " + redundant_workbench_path
            )
        log("Removed redundant " + redundant_workbench_path)

    validation_errors = []
    for definition in updated_definitions:
        item_key = str(definition.get_editor_property("item_key"))
        if not item_key or item_key == "None":
            continue
        asset_path = blueprint_asset_path(item_key)
        blueprint = unreal.EditorAssetLibrary.load_asset(asset_path)
        if blueprint is None or blueprint.generated_class() is None:
            validation_errors.append(item_key + ": Blueprint missing")
            continue
        generated_class = blueprint.generated_class()
        linked_class = load_soft_class(
            definition.get_editor_property("world_actor_class")
        )
        if linked_class != generated_class:
            validation_errors.append(item_key + ": catalog link mismatch")
        cdo = unreal.get_default_object(generated_class)
        if not cdo.get_editor_property("use_blueprint_appearance"):
            validation_errors.append(item_key + ": Blueprint appearance disabled")

    if validation_errors:
        raise RuntimeError("; ".join(validation_errors))
    log(
        "Finished and validated: {} item Blueprints generated and linked".format(
            generated_count
        )
    )


main()
