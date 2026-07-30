"""Prepare a flat test map and add two complete movable buildings."""

import unreal


MAP_PATH = "/Game/Botanicus/Maps/Lvl_BuildingTest"
BUILDING_CLASS_ROOT = "/EasyBuildingSystem/Blueprints/BuildingObjects/"
TEST_PREFIX = "Botanicus_TestBuilding_"
TREE_CLASS = (
    "/Game/EasyBuildingSystem/Blueprints/InteractionObjects/"
    "BP_EBS_Tree.BP_EBS_Tree_C"
)
FOLIAGE_CLASS = "/Script/Foliage.InstancedFoliageActor"

# Source rectangles contain two existing complete EBS demonstration houses.
SOURCE_BUILDINGS = [
    {
        "name": "A",
        "bounds": (4200.0, 5700.0, -1200.0, 500.0),
        "target_xy": (-7000.0, -3000.0),
    },
    {
        "name": "B",
        "bounds": (-4400.0, -3000.0, -3500.0, -2200.0),
        "target_xy": (-5200.0, -3000.0),
    },
]


def is_in_bounds(actor, bounds):
    min_x, max_x, min_y, max_y = bounds
    location = actor.get_actor_location()
    return min_x <= location.x <= max_x and min_y <= location.y <= max_y


level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

if not level_editor.load_level(MAP_PATH):
    raise RuntimeError(f"Impossible de charger {MAP_PATH}")

world = unreal.EditorLevelLibrary.get_editor_world()
all_actors = actor_subsystem.get_all_level_actors()

# Keep the existing heightmap data, but neutralize its vertical scale so the
# Landscape remains a Landscape and becomes practically perfectly flat.
landscapes = [
    actor
    for actor in all_actors
    if actor.get_class().get_path_name() == "/Script/Landscape.Landscape"
]
if len(landscapes) != 1:
    raise RuntimeError(f"Landscape attendu: 1, trouve: {len(landscapes)}")

landscape = landscapes[0]
landscape_scale = landscape.get_actor_scale3d()
landscape.set_actor_scale3d(
    unreal.Vector(landscape_scale.x, landscape_scale.y, 0.01)
)
landscape.set_editor_property("landscape_material", None)
flat_ground_z = landscape.get_actor_location().z
unreal.log(f"[Botanicus] Landscape aplati a Z={flat_ground_z:.1f}")

# Remove both individually placed EBS trees and foliage instances.
vegetation = [
    actor
    for actor in all_actors
    if actor.get_class().get_path_name() in (TREE_CLASS, FOLIAGE_CLASS)
]
if vegetation:
    actor_subsystem.destroy_actors(vegetation)
unreal.log(f"[Botanicus] Vegetation supprimee: {len(vegetation)} acteurs")

# The operation is idempotent: previous test-building copies are replaced.
old_test_actors = [
    actor
    for actor in all_actors
    if actor.get_actor_label().startswith(TEST_PREFIX)
]
if old_test_actors:
    actor_subsystem.destroy_actors(old_test_actors)
    unreal.log(f"[Botanicus] {len(old_test_actors)} anciennes copies supprimees")

all_actors = actor_subsystem.get_all_level_actors()
created_total = 0

for building in SOURCE_BUILDINGS:
    sources = [
        actor
        for actor in all_actors
        if BUILDING_CLASS_ROOT in actor.get_class().get_path_name()
        and is_in_bounds(actor, building["bounds"])
        and not actor.get_actor_label().startswith(TEST_PREFIX)
    ]
    if not sources:
        raise RuntimeError(f"Aucun acteur source pour le batiment {building['name']}")

    min_z = min(actor.get_actor_location().z for actor in sources)
    center_x = sum(actor.get_actor_location().x for actor in sources) / len(sources)
    center_y = sum(actor.get_actor_location().y for actor in sources) / len(sources)
    target_x, target_y = building["target_xy"]
    offset = unreal.Vector(
        target_x - center_x,
        target_y - center_y,
        flat_ground_z - min_z,
    )

    duplicates = actor_subsystem.duplicate_actors(sources, world, offset)
    if len(duplicates) != len(sources):
        raise RuntimeError(
            f"Duplication incomplete pour {building['name']}: "
            f"{len(duplicates)}/{len(sources)}"
        )

    for index, (source, duplicate) in enumerate(zip(sources, duplicates), start=1):
        duplicate.set_actor_label(
            f"{TEST_PREFIX}{building['name']}_{index:02d}_{source.get_actor_label()}"
        )

    created_total += len(duplicates)
    unreal.log(
        f"[Botanicus] Batiment {building['name']}: {len(duplicates)} acteurs | "
        f"centre ({target_x:.0f}, {target_y:.0f}) | sol Z={flat_ground_z:.1f}"
    )

if not level_editor.save_current_level():
    raise RuntimeError(f"Echec de sauvegarde de {MAP_PATH}")

unreal.log(
    f"[Botanicus] Termine: 2 batiments, {created_total} acteurs, map sauvegardee"
)
