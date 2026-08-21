"""Create editable Botanicus Niagara systems from Unreal's maintained templates."""

import unreal


TARGET_FOLDER = "/Game/Botanicus/VFX/Watering"
ASSETS = {
    "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight":
        f"{TARGET_FOLDER}/NS_BotanicusWateringJet",
    "/Niagara/DefaultAssets/Templates/Systems/DirectionalBurstLightweight.DirectionalBurstLightweight":
        f"{TARGET_FOLDER}/NS_BotanicusWateringImpact",
}


unreal.EditorAssetLibrary.make_directory(TARGET_FOLDER)
for source, destination in ASSETS.items():
    if unreal.EditorAssetLibrary.does_asset_exist(destination):
        unreal.log(f"Botanicus Niagara déjà présent: {destination}")
        continue
    source_asset = unreal.load_asset(source)
    if not source_asset:
        raise RuntimeError(f"Template Niagara introuvable: {source}")
    destination_name = destination.rsplit("/", 1)[-1]
    duplicated = unreal.AssetToolsHelpers.get_asset_tools().duplicate_asset(
        destination_name,
        TARGET_FOLDER,
        source_asset,
    )
    if not duplicated:
        raise RuntimeError(f"Impossible de dupliquer {source} vers {destination}")
    unreal.EditorAssetLibrary.save_asset(destination, only_if_is_dirty=False)
    unreal.log(f"Botanicus Niagara créé: {destination}")
