"""Validate and recompile the baseline water-surface assets."""

import unreal


MATERIAL_PATHS = [
    "/Game/Botanicus/Materials/Effects/M_BotanicusWetness",
    "/Game/Botanicus/Materials/Effects/M_BotanicusPuddleCartoon",
]
NIAGARA_PATHS = [
    "/Game/Botanicus/VFX/Watering/NS_BotanicusWateringImpact",
    "/Game/Botanicus/VFX/Watering/NS_BotanicusWaterRunoff",
]
TEXTURE_PATHS = [
    "/Game/Botanicus/VFX/Watering/T_PuddleMasks_Atlas",
    "/Game/Botanicus/VFX/Watering/T_PuddleNormal_Soft",
]

for path in MATERIAL_PATHS:
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.Material):
        raise RuntimeError(f"Matériau de surface d'eau invalide: {path}")
    if path.endswith("M_BotanicusPuddleCartoon") and not asset.get_editor_property(
        "used_with_instanced_static_meshes"
    ):
        raise RuntimeError(
            "Le matériau de flaque doit autoriser les Instanced Static Meshes"
        )
    unreal.MaterialEditingLibrary.recompile_material(asset)
    unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
    unreal.log(f"Botanicus matériau validé: {path}")

for path in NIAGARA_PATHS:
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.NiagaraSystem):
        raise RuntimeError(f"Niagara de surface d'eau invalide: {path}")
    unreal.log(f"Botanicus Niagara validé: {path}")

for path in TEXTURE_PATHS:
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.Texture2D):
        raise RuntimeError(f"Texture de flaque invalide: {path}")
    unreal.log(f"Botanicus texture validée: {path}")
