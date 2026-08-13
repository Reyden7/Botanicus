import unreal
bp=unreal.load_asset('/Game/Botanicus/UI/Command/WBP_CommandComputer')
print('BP=',bp)
print('PARENT=',unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp) if bp else None)
cls=unreal.load_class(None,'/Game/Botanicus/UI/Command/WBP_CommandComputer.WBP_CommandComputer_C')
print('CLASS=',cls)
if cls:
 cdo=unreal.get_default_object(cls)
 print('CDO=',cdo)
 print('TREE=',cdo.get_editor_property('widget_tree'))
 if cdo.get_editor_property('widget_tree'):
  print('WIDGETS=',[str(x.get_name()) for x in cdo.get_editor_property('widget_tree').get_all_widgets()])
