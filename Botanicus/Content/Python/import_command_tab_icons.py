import unreal


source_root = r"C:/Users/quent/Documents/Botanicus/image du jeu/UI/commande"
destination = "/Game/Botanicus/UI/Command/Textures"
textures = {
    "T_Command_TabBuildings": "onglet bâtiment.png",
    "T_Command_TabSeeds": "onglet graine.png",
    "T_Command_TabTools": "onglet outil.png",
    "T_Command_TabCare": "onglet soin.png",
    "T_Command_TabSales": "onglet vente.png",
}

tasks = []
for asset_name, source_name in textures.items():
    task = unreal.AssetImportTask()
    task.filename = f"{source_root}/{source_name}"
    task.destination_path = destination
    task.destination_name = asset_name
    task.automated = True
    task.replace_existing = True
    task.save = True
    tasks.append(task)

unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
for asset_name in textures:
    texture = unreal.load_asset(f"{destination}/{asset_name}")
    if not texture:
        raise RuntimeError(f"Texture import failed: {asset_name}")
    texture.set_editor_property(
        "compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
    texture.set_editor_property(
        "mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    texture.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(texture)

if not unreal.BotanicusCommandWidgetBuilder.upgrade_command_computer_tab_icons():
    raise RuntimeError("Command tab icon widget upgrade failed")
unreal.log("BOTANICUS_COMMAND_TAB_ICONS imported=5 upgraded=1")
