"""Validate that both watering effects are loadable Niagara systems."""

import unreal


PATHS = [
    "/Game/Botanicus/VFX/Watering/NS_BotanicusWateringJet.NS_BotanicusWateringJet",
    "/Game/Botanicus/VFX/Watering/NS_BotanicusWateringImpact.NS_BotanicusWateringImpact",
	"/Game/Botanicus/VFX/Watering/NS_BotanicusWaterRunoff.NS_BotanicusWaterRunoff",
]

for path in PATHS:
    asset = unreal.load_asset(path)
    if not isinstance(asset, unreal.NiagaraSystem):
        raise RuntimeError(f"Système Niagara invalide ou introuvable: {path}")
    unreal.log(f"Botanicus validation Niagara réussie: {path}")
