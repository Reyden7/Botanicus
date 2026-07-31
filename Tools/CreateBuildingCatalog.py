"""Create the building catalogue and tag its two map template groups."""

import unreal


ASSET_PATH = "/Game/Botanicus/Data/DA_BotanicusBuildingCatalog"
MAP_PATH = "/Game/Botanicus/Maps/Lvl_BuildingTest"
BUILDING_CLASS_ROOT = "/Game/EasyBuildingSystem/Blueprints/BuildingObjects/"
STRUCTURAL_TOKENS = (
    "foundation",
    "wall",
    "ceiling",
    "roof",
    "ramp",
    "stairs",
    "fence",
    "doorframe",
    "windowframe",
)
BUILDINGS = (
    {
        "key": "GreenhouseCompact",
        "name": "Serre compacte",
        "description": (
            "Une serre fonctionnelle pour démarrer une petite production."
        ),
        "price": 900,
        "label_prefix": "Botanicus_TestBuilding_A_",
        "target_xy": (4950.0, -350.0),
        "template_tag": "BotanicusTemplate_GreenhouseCompact",
    },
    {
        "key": "GreenhouseWorkshop",
        "name": "Serre atelier",
        "description": (
            "Un bâtiment plus vaste adapté aux installations avancées."
        ),
        "price": 1400,
        "label_prefix": "Botanicus_TestBuilding_B_",
        "target_xy": (-3700.0, -2850.0),
        "template_tag": "BotanicusTemplate_GreenhouseWorkshop",
    },
)


def create_definition(building):
    definition = unreal.BotanicusBuildingDefinition()
    definition.set_editor_property("building_key", building["key"])
    definition.set_editor_property("display_name", building["name"])
    definition.set_editor_property("description", building["description"])
    definition.set_editor_property("template_tag", building["template_tag"])
    definition.set_editor_property(
        "legacy_template_group_index",
        0 if building["key"] == "GreenhouseCompact" else 1,
    )
    prefab_class_name = (
        "BotanicusCompactGreenhouseActor"
        if building["key"] == "GreenhouseCompact"
        else "BotanicusWorkshopGreenhouseActor"
    )
    prefab_class = unreal.load_class(
        None,
        f"/Script/Botanicus.{prefab_class_name}",
    )
    if prefab_class is None:
        raise RuntimeError(f"Could not load {prefab_class_name}")
    definition.set_editor_property(
        "fallback_prefab_class",
        prefab_class,
    )
    definition.set_editor_property("price", building["price"])
    definition.set_editor_property("unlocked_by_default", True)
    definition.set_editor_property(
        "required_development_level",
        1 if building["key"] == "GreenhouseCompact" else 2,
    )
    definition.set_editor_property(
        "preview_offset",
        unreal.Vector(3500.0, 0.0, 0.0),
    )
    return definition


asset = unreal.load_asset(ASSET_PATH)
if asset is None:
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property(
        "data_asset_class",
        unreal.BotanicusBuildingCatalog,
    )
    asset = asset_tools.create_asset(
        "DA_BotanicusBuildingCatalog",
        "/Game/Botanicus/Data",
        unreal.BotanicusBuildingCatalog,
        factory,
    )

if asset is None:
    raise RuntimeError("Could not create DA_BotanicusBuildingCatalog")

asset.set_editor_property(
    "buildings",
    [create_definition(building) for building in BUILDINGS],
)
unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
if not level_editor.load_level(MAP_PATH):
    raise RuntimeError(f"Could not load {MAP_PATH}")

actors = actor_subsystem.get_all_level_actors()
tagged_template_count = 0
catalogue_tags = {
    unreal.Name(building["template_tag"]) for building in BUILDINGS
}
for actor in actors:
    tags = list(actor.get_editor_property("tags"))
    filtered_tags = [tag for tag in tags if tag not in catalogue_tags]
    if len(filtered_tags) != len(tags):
        actor.set_editor_property("tags", filtered_tags)

for building in BUILDINGS:
    structural_actors = []
    for actor in actors:
        class_path = actor.get_class().get_path_name()
        class_name = actor.get_class().get_name().lower()
        if (
            BUILDING_CLASS_ROOT in class_path
            and any(token in class_name for token in STRUCTURAL_TOKENS)
        ):
            structural_actors.append(actor)

    if not structural_actors:
        unreal.log_warning(
            f"[Botanicus] No editor template actor for {building['key']}; "
            "the runtime legacy group index will be used."
        )
        continue

    labelled = [
        actor
        for actor in structural_actors
        if actor.get_actor_label().startswith(building["label_prefix"])
    ]
    target_x, target_y = building["target_xy"]
    candidates = labelled if labelled else structural_actors
    candidates.sort(
        key=lambda actor: (
            actor.get_actor_location().x - target_x
        )
        ** 2
        + (
            actor.get_actor_location().y - target_y
        )
        ** 2
    )
    seed = candidates[0]
    seed_location = seed.get_actor_location()
    if (
        (seed_location.x - target_x) ** 2
        + (seed_location.y - target_y) ** 2
        > 1800.0 ** 2
    ):
        raise RuntimeError(
            f"No template near expected position for {building['key']}"
        )
    tags = list(seed.get_editor_property("tags"))
    tags.append(unreal.Name(building["template_tag"]))
    seed.set_editor_property("tags", tags)
    tagged_template_count += 1
    unreal.log(
        f"[Botanicus] {building['key']} template: "
        f"{seed.get_actor_label()} at "
        f"({seed_location.x:.0f}, {seed_location.y:.0f})"
    )

if tagged_template_count > 0 and not level_editor.save_current_level():
    raise RuntimeError(f"Could not save {MAP_PATH}")

unreal.log(
    f"[Botanicus] Building catalogue created: {ASSET_PATH}"
)
unreal.SystemLibrary.quit_editor()
