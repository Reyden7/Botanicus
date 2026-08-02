"""Create the inventory ItemData assets for every generated plant variety."""

import unreal


ASSET_DIRECTORY = "/Game/Botanicus/Items"

ITEMS = (
    (
        "BoxCutter",
        "Cutter",
        "Outil indispensable pour decouper le scotch des cartons livres.",
        19,
    ),
    (
        "SeedPacket_Orchid",
        "Graines d'orchidee rose",
        "Trois graines pour une fleur premium exigeante en eau.",
        20,
    ),
    (
        "Harvest_Orchid",
        "Orchidee rose",
        "Une orchidee entiere recoltee, prete a etre rempotee.",
        21,
    ),
    (
        "SeedPacket_Monstera",
        "Graines de monstera",
        "Trois graines pour une plante decorative a croissance lente.",
        22,
    ),
    (
        "Harvest_Monstera",
        "Monstera",
        "Un monstera entier recolte, pret a etre rempote.",
        23,
    ),
    (
        "SeedPacket_Lavender",
        "Graines de lavande",
        "Cinq graines pour une plante aromatique peu gourmande en eau.",
        24,
    ),
    (
        "Harvest_Lavender",
        "Lavande violette",
        "Une lavande entiere recoltee, prete a etre rempotee.",
        25,
    ),
)

QUALITY_ITEMS = []
quality_order = 30
for harvest_key, plant_name in (
    ("Harvest_Basil", "Plant de basilic"),
    ("Harvest_Orchid", "Orchidee rose"),
    ("Harvest_Monstera", "Monstera"),
    ("Harvest_Lavender", "Lavande violette"),
):
    QUALITY_ITEMS.append(
        (
            f"{harvest_key}_Beautiful",
            f"{plant_name} - Belle qualite",
            "Une plante entiere bien entretenue, prete a etre rempotee.",
            quality_order,
        )
    )
    quality_order += 1
    QUALITY_ITEMS.append(
        (
            f"{harvest_key}_Exceptional",
            f"{plant_name} - Qualite exceptionnelle",
            "Une plante entiere aux soins exceptionnels, prete a etre rempotee.",
            quality_order,
        )
    )
    quality_order += 1

unreal.EditorAssetLibrary.make_directory(ASSET_DIRECTORY)
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

for item_key, item_name, description, item_order in ITEMS + tuple(QUALITY_ITEMS):
    asset_name = f"DA_{item_key}"
    asset_path = f"{ASSET_DIRECTORY}/{asset_name}"
    item_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not item_asset:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property(
            "data_asset_class",
            unreal.ItemDataAsset,
        )
        item_asset = asset_tools.create_asset(
            asset_name,
            ASSET_DIRECTORY,
            unreal.ItemDataAsset,
            factory,
        )
    if not item_asset:
        raise RuntimeError(f"Unable to create {asset_path}")

    item_asset.set_editor_property("item_key", item_key)
    item_asset.set_editor_property("item_name", item_name)
    item_asset.set_editor_property("item_description", description)
    item_asset.set_editor_property("item_order", item_order)
    unreal.EditorAssetLibrary.save_loaded_asset(
        item_asset,
        only_if_is_dirty=False,
    )
    unreal.log(f"Botanicus plant item ready: {asset_path}")

unreal.SystemLibrary.quit_editor()
