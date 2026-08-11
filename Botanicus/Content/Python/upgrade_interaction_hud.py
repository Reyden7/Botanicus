"""Preserve the interaction layout while replacing its key label by an icon."""

import unreal


ASSET_PATH = "/Game/Botanicus/UI/HUD/Elements/WBP_HUD_Interaction"
blueprint = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
if not blueprint:
    raise RuntimeError(f"Unable to load {ASSET_PATH}")
if not unreal.BotanicusHudEditorLibrary.upgrade_interaction_element(blueprint):
    raise RuntimeError(f"Unable to upgrade {ASSET_PATH}")
if not unreal.EditorAssetLibrary.save_loaded_asset(blueprint, False):
    raise RuntimeError(f"Unable to save {ASSET_PATH}")
unreal.log(f"Upgraded adaptive interaction HUD: {ASSET_PATH}")
