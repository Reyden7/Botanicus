import unreal


SOURCE_ROOT = r"C:/Users/quent/Documents/Botanicus/image du jeu/UI/commande"
PREPARED_ROOT = r"C:/Users/quent/Documents/repos/Botanicus/Botanicus/Content/Python/CommandTextureSources"
DESTINATION = "/Game/Botanicus/UI/Command/Textures"

TEXTURES = {
    "T_Command_Frame": "cadre ordinateur.png",
    "T_Command_Seeds": "icons_botanicus_separes/01_graine.png",
    "T_Command_WateringCan": "icons_botanicus_separes/02_arrosoir.png",
    "T_Command_Preparation": "icons_botanicus_separes/03_atelier_preparation.png",
    "T_Command_Sales": "icons_botanicus_separes/04_terminal.png",
    "T_Command_Buildings": "icons_botanicus_separes/05_boutique.png",
    "T_Command_Close": "icons_botanicus_separes/06_fermer.png",
    "T_Command_Cart": "icons_botanicus_separes/07_panier.png",
    "T_Command_Level": "icons_botanicus_separes/08_etoile_bleue.png",
    "T_Command_ScrollTrack": "icons_botanicus_separes/09_curseur_vertical.png",
    "T_Command_Fire": "icons_botanicus_separes/10_feu.png",
    "T_Command_Nature": "icons_botanicus_separes/11_feuille.png",
    "T_Command_Ice": "icons_botanicus_separes/12_flocon.png",
    "T_Command_Water": "icons_botanicus_separes/13_eau.png",
    "T_Command_TabBackground": "icons_botanicus_separes/bg onglet.png",
    "T_Command_ItemBackground": "icons_botanicus_separes/bg item.png",
    "T_Command_ShopClosedIcon": "icons_botanicus_separes/fermé.png",
    "T_Command_ShopOpenIcon": "icons_botanicus_separes/ouvert.png",
    "T_Command_ShopClosedBackground": "icons_botanicus_separes/magasin fermé.png",
    "T_Command_ShopOpenBackground": "icons_botanicus_separes/magasin ouvert.png",
    "T_Command_ScrollThumb": "icons_botanicus_separes/scroll bar.png",
    "T_Command_ScrollArrow": "icons_botanicus_separes/fleche scroll bar.png",
    "T_Command_CreditCoin": "icons_botanicus_separes/credit.png",
    "T_Command_ShopPanel": "icons_botanicus_separes/cadre boutique.png",
    "T_Command_CreditsBackground": "@prepared/bg_credit.png",
    "T_Command_LevelBackground": "@prepared/bg_niveau.png",
    "T_Command_BadgeWater": "@prepared/bg_eau.png",
    "T_Command_BadgeIce": "@prepared/bg_glace.png",
    "T_Command_BadgeNature": "@prepared/bg_normal.png",
    "T_Command_BadgeShadow": "@prepared/bg_tenebre.png",
    "T_Command_BadgeFire": "@prepared/bg_feu.png",
    "T_Command_OrderNormal": "@prepared/bt_commander_normal.png",
    "T_Command_OrderPressed": "@prepared/bt_commander_pressed.png",
    "T_Command_CloseNormal": "icons_botanicus_separes/bt_fermer_normal.png",
    "T_Command_ClosePressed": "icons_botanicus_separes/bt_fermer_pressed.png",
    "T_Command_Parcel": "icons_botanicus_separes/icon carton.png",
}


asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
tasks = []
for asset_name, relative_source in TEXTURES.items():
    task = unreal.AssetImportTask()
    task.filename = (
        f"{PREPARED_ROOT}/{relative_source.removeprefix('@prepared/')}"
        if relative_source.startswith("@prepared/")
        else f"{SOURCE_ROOT}/{relative_source}"
    )
    task.destination_path = DESTINATION
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)

asset_tools.import_asset_tasks(tasks)

for asset_name in TEXTURES:
    texture = unreal.load_asset(f"{DESTINATION}/{asset_name}")
    if not texture:
        raise RuntimeError(f"Texture import failed: {asset_name}")
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

unreal.log(f"Imported {len(TEXTURES)} command-panel textures into {DESTINATION}")
