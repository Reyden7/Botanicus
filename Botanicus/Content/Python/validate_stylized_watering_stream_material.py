"""Force compilation of the stylised watering-stream material."""

import unreal


PATH = (
    "/Game/Botanicus/Materials/Effects/"
    "M_BotanicusWaterStreamStylized.M_BotanicusWaterStreamStylized"
)
material = unreal.load_asset(PATH)
if not isinstance(material, unreal.Material):
    raise RuntimeError(f"Matériau introuvable: {PATH}")
unreal.MaterialEditingLibrary.recompile_material(material)
unreal.log(f"Botanicus validation matériau réussie: {PATH}")
