"""Create the Botanicus-facing Easy Building System Blueprint layer.

Run through UnrealEditor-Cmd. Source marketplace assets are never modified:
the script duplicates the three gameplay Blueprints before reparenting them
to the native Botanicus classes.
"""

import unreal


DESTINATION = "/Game/Botanicus/Building"
GAMEPLAY_MAP = "/Game/FirstPerson/Lvl_FirstPerson"
BUILDING_TEST_MAP_SOURCE = "/Game/EasyBuildingSystem/Maps/Demo_Stylized"
BUILDING_TEST_MAP = "/Game/Botanicus/Maps/Lvl_BuildingTest"

ASSETS = (
    (
        "/Game/EasyBuildingSystem/Blueprints/Character/BP_EBS_BuilderCharacter",
        "BP_BotanicusBuilderCharacter",
        "/Script/Botanicus.BotanicusCharacter",
    ),
    (
        "/Game/EasyBuildingSystem/Blueprints/Game/BP_EBS_PlayerController",
        "BP_BotanicusPlayerController",
        "/Script/Botanicus.BotanicusPlayerController",
    ),
    (
        "/Game/EasyBuildingSystem/Blueprints/Game/BP_EBS_GameMode",
        "BP_BotanicusBuildingGameMode",
        "/Script/Botanicus.BotanicusGameMode",
    ),
)


def fail(message):
    unreal.log_error("[Botanicus EBS] " + message)
    raise RuntimeError(message)


def duplicate_and_reparent(source_path, destination_name, parent_class_path):
    destination_path = f"{DESTINATION}/{destination_name}"
    blueprint = None

    if unreal.EditorAssetLibrary.does_asset_exist(destination_path):
        blueprint = unreal.EditorAssetLibrary.load_asset(destination_path)
        if blueprint is None:
            fail(f"Could not load existing integration asset: {destination_path}")
        unreal.log(f"[Botanicus EBS] Reusing {destination_path}")
    else:
        source = unreal.EditorAssetLibrary.load_asset(source_path)
        if source is None:
            fail(f"Missing source Blueprint: {source_path}")

        blueprint = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
            destination_name,
            DESTINATION,
            source,
        )
        if blueprint is None:
            fail(f"Could not duplicate {source_path}")

        unreal.log(f"[Botanicus EBS] Created {destination_path}")

    parent_class = unreal.load_class(None, parent_class_path)
    if parent_class is None:
        fail(f"Missing native parent class: {parent_class_path}")

    unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent_class)
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    ):
        fail(f"Could not save {destination_path}")

    return destination_path


def configure_native_character_defaults(character_class, character_blueprint):
    template_class = unreal.EditorAssetLibrary.load_blueprint_class(
        "/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"
    )
    if template_class is None:
        fail("Could not load the first-person character template")

    template_cdo = unreal.get_default_object(template_class)
    character_cdo = unreal.get_default_object(character_class)

    # Preserve the native Botanicus input assets even though the EBS character
    # also has legacy keyboard/mouse bindings.
    for property_name in (
        "jump_action",
        "move_action",
        "look_action",
        "mouse_look_action",
    ):
        character_cdo.set_editor_property(
            property_name,
            template_cdo.get_editor_property(property_name),
        )

    # The EBS source character predates Botanicus' native first-person mesh.
    # Give that inherited component the same compatible mesh/animation setup
    # as the UE 5.8 first-person template so its head socket is valid.
    template_mesh = template_cdo.get_editor_property("first_person_mesh")
    character_mesh = character_cdo.get_editor_property("first_person_mesh")
    character_mesh.set_editor_property(
        "skeletal_mesh_asset",
        template_mesh.get_editor_property("skeletal_mesh_asset"),
    )
    character_mesh.set_editor_property(
        "animation_mode",
        template_mesh.get_editor_property("animation_mode"),
    )
    character_mesh.set_editor_property(
        "anim_class",
        template_mesh.get_editor_property("anim_class"),
    )

    # Botanicus exposes only its native first-person camera during character
    # gameplay. The controller temporarily switches to a dedicated camera only
    # while the top-down construction state is active.
    character_camera = character_cdo.get_editor_property(
        "first_person_camera_component"
    )
    character_camera.set_editor_property("auto_activate", True)

    unreal.BlueprintEditorLibrary.compile_blueprint(character_blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        character_blueprint, only_if_is_dirty=False
    ):
        fail("Could not save the configured Botanicus builder character")


def configure_map_game_mode(map_path, game_mode_class):
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    if world is None:
        fail(f"Could not load map: {map_path}")

    world_settings = world.get_world_settings()
    world_settings.set_editor_property("default_game_mode", game_mode_class)

    if not unreal.EditorAssetLibrary.save_asset(
        map_path, only_if_is_dirty=False
    ):
        fail(f"Could not save map: {map_path}")

    unreal.log(
        f"[Botanicus EBS] {map_path} now uses "
        "BP_BotanicusBuildingGameMode."
    )


def create_building_test_map():
    if unreal.EditorAssetLibrary.does_asset_exist(BUILDING_TEST_MAP):
        unreal.log(f"[Botanicus EBS] Reusing {BUILDING_TEST_MAP}")
        return

    source_map = unreal.EditorAssetLibrary.load_asset(BUILDING_TEST_MAP_SOURCE)
    if source_map is None:
        fail(f"Could not load building test map source: {BUILDING_TEST_MAP_SOURCE}")

    unreal.EditorAssetLibrary.make_directory("/Game/Botanicus/Maps")
    duplicated_map = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
        "Lvl_BuildingTest",
        "/Game/Botanicus/Maps",
        source_map,
    )
    if duplicated_map is None:
        fail(f"Could not create building test map: {BUILDING_TEST_MAP}")

    if not unreal.EditorAssetLibrary.save_loaded_asset(
        duplicated_map, only_if_is_dirty=False
    ):
        fail(f"Could not save building test map: {BUILDING_TEST_MAP}")

    unreal.log(
        f"[Botanicus EBS] Created {BUILDING_TEST_MAP} with a compatible Landscape."
    )


def main():
    unreal.EditorAssetLibrary.make_directory(DESTINATION)

    created_paths = {}
    for source_path, destination_name, parent_class_path in ASSETS:
        created_paths[destination_name] = duplicate_and_reparent(
            source_path,
            destination_name,
            parent_class_path,
        )

    character_class = unreal.EditorAssetLibrary.load_blueprint_class(
        created_paths["BP_BotanicusBuilderCharacter"]
    )
    controller_class = unreal.EditorAssetLibrary.load_blueprint_class(
        created_paths["BP_BotanicusPlayerController"]
    )
    game_mode_class = unreal.EditorAssetLibrary.load_blueprint_class(
        created_paths["BP_BotanicusBuildingGameMode"]
    )

    if character_class is None or controller_class is None or game_mode_class is None:
        fail("One or more generated integration classes could not be loaded")

    character_blueprint = unreal.EditorAssetLibrary.load_asset(
        created_paths["BP_BotanicusBuilderCharacter"]
    )
    configure_native_character_defaults(character_class, character_blueprint)

    game_mode_cdo = unreal.get_default_object(game_mode_class)
    game_mode_cdo.set_editor_property("default_pawn_class", character_class)
    game_mode_cdo.set_editor_property("player_controller_class", controller_class)

    game_mode_blueprint = unreal.EditorAssetLibrary.load_asset(
        created_paths["BP_BotanicusBuildingGameMode"]
    )
    unreal.BlueprintEditorLibrary.compile_blueprint(game_mode_blueprint)
    if not unreal.EditorAssetLibrary.save_loaded_asset(
        game_mode_blueprint, only_if_is_dirty=False
    ):
        fail("Could not save the configured Botanicus building game mode")

    create_building_test_map()
    configure_map_game_mode(GAMEPLAY_MAP, game_mode_class)
    configure_map_game_mode(BUILDING_TEST_MAP, game_mode_class)

    unreal.log(
        "[Botanicus EBS] Integration ready: "
        "BP_BotanicusBuildingGameMode uses the Botanicus builder character "
        "and player controller."
    )


main()
