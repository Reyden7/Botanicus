"""Log the effective native building catalogue definitions."""

import unreal


catalogue = unreal.load_asset(
    "/Game/Botanicus/Data/DA_BotanicusBuildingCatalog"
)
if catalogue is None:
    raise RuntimeError("Building catalogue asset not found")

for definition in catalogue.get_editor_property("buildings"):
    prefab = definition.get_editor_property("fallback_prefab_class")
    unreal.log(
        "[Botanicus Building Inspect] "
        f"key={definition.get_editor_property('building_key')} "
        f"price={definition.get_editor_property('price')} "
        f"prefab={prefab}"
    )

unreal.SystemLibrary.quit_editor()
