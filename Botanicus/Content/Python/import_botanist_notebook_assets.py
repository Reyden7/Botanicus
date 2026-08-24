import os
import tempfile
import zipfile

import unreal


SOURCE_ROOT = r"C:\Users\quent\Documents\Botanicus\image du jeu\UI\carnet"
TEXTURE_DESTINATION = "/Game/Botanicus/UI/Botanist/Textures"
FONT_DESTINATION = "/Game/Botanicus/UI/Botanist/Fonts"


TEXTURES = {
    "bg.png": "T_Notebook_Background",
    "page vide.png": "T_Notebook_BlankPage",
    "illustration fleure - inconu.png": "T_Notebook_UnknownPlant",
    "feuille tittle gauche.png": "T_Notebook_TitleLeft",
    "feuille tittle droite.png": "T_Notebook_TitleRight",
    "fleure bas gauche.png": "T_Notebook_FlowerBottomLeft",
    "fleure bas droite.png": "T_Notebook_FlowerBottomRight",
    "bt fermer - normal.png": "T_Notebook_CloseNormal",
    "bt fermer - clicked.png": "T_Notebook_ClosePressed",
    "bt Page precedente - normal.png": "T_Notebook_PreviousNormal",
    "bt Page precedente - clicked.png": "T_Notebook_PreviousPressed",
    "bt Page suivante - normal.png": "T_Notebook_NextNormal",
    "bt Page suivante - clicked.png": "T_Notebook_NextPressed",
    "température.png": "T_Notebook_Temperature",
    "humidité.png": "T_Notebook_Humidity",
    "luminosité.png": "T_Notebook_Luminosity",
    "tereau.png": "T_Notebook_Soil",
    "temps de croissance.png": "T_Notebook_GrowthTime",
    "languette normal.png": "T_Notebook_ElementTab_Normal",
    "languette feu.png": "T_Notebook_ElementTab_Fire",
    "languette eau.png": "T_Notebook_ElementTab_Water",
    "languette ténèbre.png": "T_Notebook_ElementTab_Shadow",
    "languette glace.png": "T_Notebook_ElementTab_Ice",
}

for letter in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
    TEXTURES[os.path.join("Languettes", f"{letter}.png")] = f"T_Notebook_Tab_{letter}"


def import_asset(filename, destination_path, destination_name):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination_path
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.replace_existing_settings = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    if not task.imported_object_paths:
        raise RuntimeError(f"Échec de l'import de {filename}")
    return task.imported_object_paths[0]


for relative_path, asset_name in TEXTURES.items():
    source = os.path.join(SOURCE_ROOT, relative_path)
    expected_path = f"{TEXTURE_DESTINATION}/{asset_name}"
    texture = unreal.load_asset(expected_path)
    if not isinstance(texture, unreal.Texture2D):
        # A few historical notebook assets are redirectors after previous
        # renames. Leave those intact and only import genuinely missing files.
        if unreal.EditorAssetLibrary.does_asset_exist(expected_path):
            unreal.log_warning(
                f"BOTANICUS_NOTEBOOK_SKIP_EXISTING={expected_path}")
            continue
        object_path = import_asset(source, TEXTURE_DESTINATION, asset_name)
        texture = unreal.load_asset(object_path)
    if isinstance(texture, unreal.Texture2D):
        texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("srgb", True)
        unreal.EditorAssetLibrary.save_loaded_asset(texture)

zip_path = os.path.join(SOURCE_ROOT, "notebook_6.zip")
with tempfile.TemporaryDirectory(prefix="botanicus_notebook_font_") as temp_dir:
    with zipfile.ZipFile(zip_path, "r") as archive:
        archive.extract("Notebook.otf", temp_dir)
    font_path = os.path.join(temp_dir, "Notebook.otf")
    imported_font = import_asset(font_path, FONT_DESTINATION, "Notebook")
    unreal.log(f"BOTANICUS_NOTEBOOK_FONT={imported_font}")

unreal.EditorAssetLibrary.save_directory(
    "/Game/Botanicus/UI/Botanist", only_if_is_dirty=False, recursive=True)
unreal.log(f"BOTANICUS_NOTEBOOK_TEXTURE_COUNT={len(TEXTURES)}")
