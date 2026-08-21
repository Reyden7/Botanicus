"""Load and recompile the runtime soil material for automated validation."""

import unreal


MATERIAL_PATH = (
    "/Game/Botanicus/Materials/Soil/"
    "M_BotanicusSoilWetPatchesV2.M_BotanicusSoilWetPatchesV2"
)

material = unreal.load_asset(MATERIAL_PATH)
if not isinstance(material, unreal.Material):
    raise RuntimeError(f"Matériau introuvable: {MATERIAL_PATH}")

unreal.MaterialEditingLibrary.recompile_material(material)
unreal.log(f"Botanicus validation matériau réussie: {MATERIAL_PATH}")
