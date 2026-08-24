import unreal


result = unreal.BotanicusCommandWidgetBuilder.upgrade_botanist_notebook_note_scroll_boxes()
unreal.log(f"BOTANICUS_NOTE_SCROLLBOXES={result}")
if not result:
    raise RuntimeError("Impossible de convertir les notes du carnet en ScrollBox")
