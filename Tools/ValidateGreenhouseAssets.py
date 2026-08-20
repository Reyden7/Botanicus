"""Validate the unified greenhouse catalogue and native environment defaults."""

import unreal


catalogue = unreal.load_asset(
    "/Game/Botanicus/Data/DA_BotanicusBuildingCatalog"
)
if catalogue is None:
    raise RuntimeError("Building catalogue asset not found")

buildings = catalogue.get_editor_property("buildings")
if len(buildings) != 1:
    raise RuntimeError(
        f"Expected one greenhouse definition, found {len(buildings)}"
    )

definition = buildings[0]
if str(definition.get_editor_property("building_key")) != "Greenhouse":
    raise RuntimeError("The only building must use the Greenhouse key")

prefab = definition.get_editor_property("fallback_prefab_class")
if prefab is None or "BotanicusGreenhouseActor" not in str(prefab):
    raise RuntimeError("Greenhouse must resolve to BotanicusGreenhouseActor")

greenhouse_class = unreal.load_class(
    None,
    "/Script/Botanicus.BotanicusGreenhouseActor",
)
if greenhouse_class is None:
    raise RuntimeError("BotanicusGreenhouseActor class not found")

defaults = unreal.get_default_object(greenhouse_class)
expected_values = {
    "temperature_celsius": 20.0,
    "air_humidity_percent": 50.0,
    "luminosity_percent": 50.0,
}
for property_name, expected in expected_values.items():
    actual = defaults.get_editor_property(property_name)
    if abs(actual - expected) > 0.001:
        raise RuntimeError(
            f"Invalid {property_name}: expected {expected}, found {actual}"
        )

unreal.log(
    "[Botanicus Greenhouse Validation] unified catalogue and environment: OK"
)
unreal.SystemLibrary.quit_editor()
