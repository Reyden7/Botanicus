"""Create/update the temporary seed packet used by inventory PIE tests."""

import os

import unreal


ASSET_DIRECTORY = "/Game/Botanicus/Items"
ASSET_NAME = "DA_SeedPacket_Test"
ASSET_PATH = f"{ASSET_DIRECTORY}/{ASSET_NAME}"

# Remove only the Botanicus-owned prototype tool definitions. Original EBS
# marketplace assets are intentionally untouched.
for obsolete_asset in (
    f"{ASSET_DIRECTORY}/DA_Tool_Hatchet",
    f"{ASSET_DIRECTORY}/DA_Tool_Pickaxe",
    f"{ASSET_DIRECTORY}/DA_Tool_Mallet",
):
    if unreal.EditorAssetLibrary.does_asset_exist(obsolete_asset):
        unreal.EditorAssetLibrary.delete_asset(obsolete_asset)

# UE's unattended force-delete can leave an untracked source file behind even
# after removing the asset from the registry. Remove only these exact generated
# files inside the project's Botanicus/Items directory.
content_directory = os.path.abspath(unreal.Paths.project_content_dir())
for obsolete_filename in (
    "DA_Tool_Hatchet.uasset",
    "DA_Tool_Pickaxe.uasset",
    "DA_Tool_Mallet.uasset",
):
    obsolete_file = os.path.abspath(
        os.path.join(
            content_directory,
            "Botanicus",
            "Items",
            obsolete_filename,
        )
    )
    if (
        os.path.commonpath((content_directory, obsolete_file))
        == content_directory
        and os.path.isfile(obsolete_file)
    ):
        os.remove(obsolete_file)


item_asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if not item_asset:
    unreal.EditorAssetLibrary.make_directory(ASSET_DIRECTORY)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.ItemDataAsset)
    item_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        ASSET_NAME,
        ASSET_DIRECTORY,
        unreal.ItemDataAsset,
        factory,
    )

if not item_asset:
    raise RuntimeError(f"Unable to create {ASSET_PATH}")

item_asset.set_editor_property("item_key", "SeedPacket_Test")
item_asset.set_editor_property("item_name", "Paquet de graines test")
item_asset.set_editor_property(
    "item_description",
    "Objet temporaire servant à valider l'inventaire et sa réplication.",
)
item_asset.set_editor_property("item_order", 0)

unreal.EditorAssetLibrary.save_loaded_asset(
    item_asset,
    only_if_is_dirty=False,
)
unreal.log(f"Botanicus prototype item ready: {ASSET_PATH}")
