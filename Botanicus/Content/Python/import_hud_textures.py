import os
import unreal


SOURCE_DIR = os.path.join(
    unreal.Paths.project_saved_dir(), "HudTextureImport"
)
DESTINATION = "/Game/Botanicus/UI/HUD/Textures"

TEXTURES = {
    "T_HUD_Star": "etoile.png",
    "T_HUD_ObjectiveComplete": "cocheVerte.png",
    "T_HUD_Credit": "credit.png",
    "T_HUD_SlotNormal": "slotHotbarNormal.png",
    "T_HUD_SlotSelected": "slotSelected.png",
    "T_HUD_ObjectiveIncomplete": "cochePasVerte.png",
    "T_HUD_ActionBackground": "bg_action.png",
    "T_HUD_CreditsBackground": "bgCredit.png",
    "T_HUD_ReputationBackground": "bgReputation.png",
    "T_HUD_ObjectiveToggle": "btUpoBJECTIF.png",
    "T_HUD_Calendar": "calendrierHeure.png",
    "T_HUD_ObjectivesBackground": "ListeObjectif.png",
    "T_HUD_Crosshair": "cross.png",
}


def import_texture(asset_name, source_name):
    source_path = os.path.join(SOURCE_DIR, source_name)
    if not os.path.isfile(source_path):
        raise RuntimeError("Missing HUD source texture: " + source_path)

    task = unreal.AssetImportTask()
    task.filename = source_path
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])

    asset_path = DESTINATION + "/" + asset_name
    texture = unreal.EditorAssetLibrary.load_asset(asset_path)
    if texture is None:
        raise RuntimeError("HUD texture import failed: " + asset_path)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    texture.set_editor_property("srgb", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture, only_if_is_dirty=False)
    unreal.log("[Botanicus HUD] Imported " + asset_path)


unreal.EditorAssetLibrary.make_directory(DESTINATION)
for texture_name, filename in TEXTURES.items():
    import_texture(texture_name, filename)

unreal.EditorLoadingAndSavingUtils.save_dirty_packages(
    save_map_packages=False, save_content_packages=True
)
unreal.log("[Botanicus HUD] All HUD textures imported")
