import unreal

result = unreal.BotanicusCommandWidgetBuilder.upgrade_botanist_notebook_rich_notes()
unreal.log(f"BOTANICUS_RICH_NOTES={result}")
if not result:
    raise RuntimeError("Impossible de convertir les notes du carnet en texte riche.")
