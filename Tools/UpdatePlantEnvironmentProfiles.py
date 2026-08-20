"""Balance every authored plant environment profile without touching other stats."""

import unreal


ASSET_PATH = "/Game/Botanicus/Data/DA_BotanicusPlantCatalog"

FIELDS = (
    "minimum_tolerated_temperature_celsius",
    "minimum_ideal_temperature_celsius",
    "maximum_ideal_temperature_celsius",
    "maximum_tolerated_temperature_celsius",
    "minimum_tolerated_air_humidity_percent",
    "minimum_ideal_air_humidity_percent",
    "maximum_ideal_air_humidity_percent",
    "maximum_tolerated_air_humidity_percent",
    "minimum_tolerated_luminosity_percent",
    "minimum_ideal_luminosity_percent",
    "maximum_ideal_luminosity_percent",
    "maximum_tolerated_luminosity_percent",
)

# Values are ordered as tolerated min, ideal min, ideal max and tolerated max
# for temperature (C), air humidity (%) and luminosity (%).
PROFILES = {
    # Normal: temperate and forgiving, with distinct light preferences.
    "AureliaSweet": (10, 18, 26, 34, 25, 45, 65, 82, 25, 45, 70, 90),
    "CoraliaPerchee": (12, 20, 28, 36, 30, 50, 72, 88, 35, 55, 80, 95),
    "CoraliaPompon": (8, 16, 24, 32, 35, 55, 75, 90, 20, 35, 60, 80),
    "Iralia": (6, 14, 22, 30, 25, 40, 60, 78, 15, 30, 55, 75),
    "LumineaFlorae": (14, 22, 30, 38, 25, 45, 65, 80, 45, 70, 90, 100),
    "VerdelaCommune": (5, 12, 24, 32, 20, 35, 60, 78, 25, 40, 75, 92),

    # Water: warm, saturated air and moderate light.
    "CoralyneBrumes": (12, 18, 26, 34, 50, 70, 90, 100, 20, 40, 65, 85),
    "HydreaLagunaire": (14, 20, 28, 36, 60, 80, 100, 100, 25, 45, 70, 90),
    "NerelisEventail": (16, 22, 30, 38, 55, 75, 95, 100, 35, 55, 80, 100),
    "OndeliaRuban": (10, 18, 25, 33, 50, 72, 92, 100, 15, 35, 60, 82),

    # Ice: sub-zero or cool conditions and restrained light.
    "BonzaiaGivre": (-15, -5, 6, 16, 30, 45, 65, 82, 15, 30, 55, 75),
    "CristalliaLumifleur": (-10, 0, 10, 20, 35, 50, 70, 88, 35, 55, 80, 95),
    "GivrelanceAzure": (-20, -8, 4, 14, 25, 40, 60, 78, 20, 35, 60, 80),
    "GlaceoraPerlee": (-12, -2, 8, 18, 45, 60, 80, 95, 10, 25, 50, 70),

    # Fire: ideal temperature starts at 40 C or above, very dry and bright.
    "BraiseliaFlamme": (25, 40, 65, 100, 0, 5, 18, 35, 50, 75, 100, 100),
    "MagmoraSpiral": (35, 50, 80, 100, 0, 3, 12, 28, 60, 80, 100, 100),
    "PyrosteleRoyal": (40, 60, 90, 100, 0, 0, 10, 25, 70, 85, 100, 100),
    "VolcaniaBasiera": (30, 45, 75, 100, 0, 5, 20, 40, 55, 75, 95, 100),

    # Shadow: mild temperatures, humid air and near-darkness.
    "Noctepine": (8, 14, 22, 30, 45, 65, 85, 100, 0, 3, 18, 40),
    "NoctivoraVentouse": (12, 18, 26, 34, 55, 75, 95, 100, 0, 0, 12, 30),
    "OmbraeLuridia": (6, 12, 20, 28, 50, 70, 90, 100, 0, 2, 15, 35),
}


def validate_profile(plant_key, values):
    if len(values) != len(FIELDS):
        raise RuntimeError(f"{plant_key}: invalid profile field count")
    for offset, label in ((0, "temperature"), (4, "humidity"), (8, "light")):
        ordered = values[offset:offset + 4]
        if list(ordered) != sorted(ordered):
            raise RuntimeError(f"{plant_key}: unordered {label} range {ordered}")
    for value in values[4:]:
        if value < 0 or value > 100:
            raise RuntimeError(f"{plant_key}: percentage outside 0-100")


catalog = unreal.load_asset(ASSET_PATH)
if catalog is None:
    raise RuntimeError(f"Could not load {ASSET_PATH}")

plants = list(catalog.get_editor_property("plants"))
asset_keys = {str(plant.get_editor_property("plant_key")) for plant in plants}
profile_keys = set(PROFILES)
if asset_keys != profile_keys:
    missing = sorted(asset_keys - profile_keys)
    unknown = sorted(profile_keys - asset_keys)
    raise RuntimeError(
        f"Profile coverage mismatch; missing={missing}, unknown={unknown}"
    )

updated_plants = []
for plant in plants:
    plant_key = str(plant.get_editor_property("plant_key"))
    values = PROFILES[plant_key]
    validate_profile(plant_key, values)
    environment = plant.get_editor_property("environment")
    for field, value in zip(FIELDS, values):
        environment.set_editor_property(field, float(value))
    plant.set_editor_property("environment", environment)
    updated_plants.append(plant)
    unreal.log(f"[Plant Environment] Updated {plant_key}: {values}")

catalog.set_editor_property("plants", updated_plants)
unreal.EditorAssetLibrary.save_loaded_asset(catalog, only_if_is_dirty=False)
unreal.log(f"[Plant Environment] Updated {len(updated_plants)} plants in {ASSET_PATH}")
unreal.SystemLibrary.quit_editor()
