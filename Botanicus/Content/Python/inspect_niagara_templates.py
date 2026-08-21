"""Log exposed parameters from useful built-in Niagara system templates."""

import unreal


PATHS = [
    "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight.FountainLightweight",
    "/Niagara/DefaultAssets/Templates/Systems/AttributeReaderTrails.AttributeReaderTrails",
    "/Niagara/DefaultAssets/Templates/Systems/DirectionalBurst.DirectionalBurst",
]

for path in PATHS:
    asset = unreal.load_asset(path)
    unreal.log(f"NIAGARA INSPECT {path}: {asset} class={asset.get_class() if asset else None}")
    if not asset:
        continue
    unreal.log(
        "  METHODS: "
        + ", ".join(
            name
            for name in dir(asset)
            if "param" in name.lower() or "emitter" in name.lower()
        )
    )
    for property_name in [
        "exposed_parameters",
        "system_spawn_script",
        "system_update_script",
        "emitter_handles",
    ]:
        try:
            value = asset.get_editor_property(property_name)
            unreal.log(f"  PROPERTY {property_name}: {value}")
        except Exception as error:
            unreal.log_warning(f"  NO PROPERTY {property_name}: {error}")
