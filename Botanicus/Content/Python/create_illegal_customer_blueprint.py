"""Create an editable appearance Blueprint for the nocturnal customer."""

import unreal


FOLDER = "/Game/Botanicus/blueprints"
NAME = "BP_IllegalCustomer"
PATH = f"{FOLDER}/{NAME}"


def main():
    blueprint = unreal.EditorAssetLibrary.load_asset(PATH)
    if blueprint is None:
        parent = unreal.load_class(
            None, "/Script/Botanicus.BotanicusIllegalCustomerCharacter"
        )
        if parent is None:
            raise RuntimeError("BotanicusIllegalCustomerCharacter C++ class is unavailable")
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        blueprint = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            NAME, FOLDER, None, factory
        )
    if blueprint is None or blueprint.generated_class() is None:
        raise RuntimeError("Unable to create " + PATH)
    unreal.EditorAssetLibrary.save_loaded_asset(blueprint, only_if_is_dirty=False)
    unreal.log_warning("BOTANICUS_ILLEGAL_CUSTOMER_BLUEPRINT_READY " + PATH)


main()
