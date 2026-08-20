"""Author species-specific neighbour relationships in the plant catalog."""

import unreal


ASSET_PATH = "/Game/Botanicus/Data/DA_BotanicusPlantCatalog"

# Each entry contains compatible keys, incompatible keys, per-neighbour bonus
# and per-neighbour penalty. Values remain editable in the Data Asset.
RELATIONSHIPS = {
    "AureliaSweet": (["VerdelaCommune"], ["BraiseliaFlamme"], 0.10, 0.20),
    "CoraliaPerchee": (["CoraliaPompon"], ["Noctepine"], 0.12, 0.18),
    "CoraliaPompon": (["CoraliaPerchee"], ["MagmoraSpiral"], 0.12, 0.20),
    "Iralia": (["OmbraeLuridia"], ["PyrosteleRoyal"], 0.10, 0.25),
    "LumineaFlorae": (["NerelisEventail"], ["NoctivoraVentouse", "OmbraeLuridia"], 0.14, 0.22),
    "VerdelaCommune": (["AureliaSweet"], ["VolcaniaBasiera"], 0.10, 0.18),

    "CoralyneBrumes": (["OndeliaRuban", "GlaceoraPerlee"], ["BraiseliaFlamme"], 0.12, 0.25),
    "HydreaLagunaire": (["NerelisEventail", "GlaceoraPerlee"], ["MagmoraSpiral"], 0.10, 0.25),
    "NerelisEventail": (["HydreaLagunaire", "CristalliaLumifleur"], ["PyrosteleRoyal"], 0.12, 0.28),
    "OndeliaRuban": (["CoralyneBrumes", "BonzaiaGivre"], ["VolcaniaBasiera"], 0.11, 0.24),

    "BonzaiaGivre": (["GivrelanceAzure", "OndeliaRuban"], ["MagmoraSpiral"], 0.10, 0.25),
    "CristalliaLumifleur": (["GlaceoraPerlee", "NerelisEventail"], ["PyrosteleRoyal"], 0.14, 0.28),
    "GivrelanceAzure": (["BonzaiaGivre"], ["BraiseliaFlamme"], 0.12, 0.25),
    "GlaceoraPerlee": (["CristalliaLumifleur", "CoralyneBrumes"], ["VolcaniaBasiera"], 0.11, 0.24),

    "BraiseliaFlamme": (["VolcaniaBasiera"], ["CoralyneBrumes", "GivrelanceAzure"], 0.12, 0.30),
    "MagmoraSpiral": (["PyrosteleRoyal"], ["HydreaLagunaire", "BonzaiaGivre"], 0.14, 0.30),
    "PyrosteleRoyal": (["MagmoraSpiral"], ["NerelisEventail", "CristalliaLumifleur"], 0.15, 0.30),
    "VolcaniaBasiera": (["BraiseliaFlamme"], ["OndeliaRuban", "GlaceoraPerlee"], 0.12, 0.28),

    "Noctepine": (["OmbraeLuridia"], ["CoraliaPerchee", "LumineaFlorae"], 0.12, 0.20),
    "NoctivoraVentouse": (["OmbraeLuridia"], ["LumineaFlorae"], 0.14, 0.22),
    "OmbraeLuridia": (["Noctepine", "NoctivoraVentouse", "Iralia"], ["LumineaFlorae"], 0.10, 0.20),
}


catalog = unreal.load_asset(ASSET_PATH)
if catalog is None:
    raise RuntimeError(f"Could not load {ASSET_PATH}")

plants = list(catalog.get_editor_property("plants"))
asset_keys = {str(plant.get_editor_property("plant_key")) for plant in plants}
if asset_keys != set(RELATIONSHIPS):
    raise RuntimeError(
        "Neighbour relationship coverage does not match the plant catalog"
    )

updated_plants = []
for plant in plants:
    plant_key = str(plant.get_editor_property("plant_key"))
    compatible, incompatible, bonus, penalty = RELATIONSHIPS[plant_key]
    unknown = (set(compatible) | set(incompatible)) - asset_keys
    if unknown:
        raise RuntimeError(f"{plant_key}: unknown neighbours {sorted(unknown)}")
    if set(compatible) & set(incompatible):
        raise RuntimeError(f"{plant_key}: a neighbour cannot be in both lists")

    plant.set_editor_property("compatible_neighbour_plant_keys", compatible)
    plant.set_editor_property("incompatible_neighbour_plant_keys", incompatible)
    plant.set_editor_property("compatible_neighbour_growth_bonus", bonus)
    plant.set_editor_property("incompatible_neighbour_growth_penalty", penalty)
    updated_plants.append(plant)
    unreal.log(
        f"[Plant Neighbours] {plant_key}: +{compatible} / -{incompatible}"
    )

catalog.set_editor_property("plants", updated_plants)
unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
unreal.log(f"[Plant Neighbours] Updated {len(updated_plants)} plants")
unreal.SystemLibrary.quit_editor()
