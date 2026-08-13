import unreal
bp=unreal.load_asset('/Game/Botanicus/UI/Command/WBP_CommandComputer')
for prop in ['widget_tree','WidgetTree','simple_construction_script']:
 try: unreal.log_warning(prop+'='+str(bp.get_editor_property(prop)))
 except Exception as e: unreal.log_warning(prop+' ERR '+str(e))
for n in dir(unreal.WidgetTree): unreal.log_warning('WT '+n)
