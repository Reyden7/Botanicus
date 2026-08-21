import unreal


result = unreal.BotanicusCommandWidgetBuilder.rebuild_botanist_notebook_widget()
unreal.log(f"BOTANICUS_BOTANIST_NOTEBOOK={result}")
if not result:
    raise RuntimeError("Impossible de créer WBP_BotanistNotebook")
