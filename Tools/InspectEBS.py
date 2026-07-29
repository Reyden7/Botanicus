"""Log the reflected Python surface of the core EBS Blueprint classes."""

import unreal


CLASS_PATHS = (
    "/Game/EasyBuildingSystem/Blueprints/BuildingObjects/Base/"
    "BP_EBS_Building_BaseObject.BP_EBS_Building_BaseObject_C",
    "/Game/EasyBuildingSystem/Blueprints/Components/"
    "BP_EBS_BuildingComponent.BP_EBS_BuildingComponent_C",
    "/Game/EasyBuildingSystem/Blueprints/Interfaces/"
    "BPI_EBS_Ownership.BPI_EBS_Ownership_C",
)


for class_path in CLASS_PATHS:
    reflected_class = unreal.load_class(None, class_path)
    if reflected_class is None:
        unreal.log_error(f"[EBS Inspect] Could not load {class_path}")
        continue

    default_object = unreal.get_default_object(reflected_class)
    unreal.log(f"[EBS Inspect] CLASS {class_path}")
    for property_name in (
        "building_type",
        "building_object_handle",
        "owner_names",
        "manual_build",
        "current_floor",
        "replicate_movement",
    ):
        try:
            value = default_object.get_editor_property(property_name)
            unreal.log(f"[EBS Inspect]   PROPERTY {property_name}={value}")
        except Exception as error:
            unreal.log(
                f"[EBS Inspect]   PROPERTY {property_name}=<unavailable: {error}>"
            )

    for name in sorted(dir(default_object)):
        lowered = name.lower()
        if any(
            token in lowered
            for token in (
                "owner",
                "build",
                "transform",
                "socket",
                "floor",
                "rotate",
                "snap",
            )
        ):
            unreal.log(f"[EBS Inspect]   {name}")


unreal.EditorLoadingAndSavingUtils.load_map(
    "/Game/Botanicus/Maps/Lvl_BuildingTest"
)
class_counts = {}
for actor in unreal.EditorLevelLibrary.get_all_level_actors():
    class_path = actor.get_class().get_path_name()
    if "/EasyBuildingSystem/Blueprints/BuildingObjects/" not in class_path:
        continue
    class_counts[class_path] = class_counts.get(class_path, 0) + 1

for class_path, count in sorted(class_counts.items()):
    unreal.log(f"[EBS Inspect] MAP_ACTOR {count} {class_path}")
