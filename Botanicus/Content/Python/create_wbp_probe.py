import unreal
f=unreal.WidgetBlueprintFactory()
f.set_editor_property('parent_class', unreal.load_class(None,'/Script/Botanicus.BotanicusOrderCatalogWidget'))
a=unreal.AssetToolsHelpers.get_asset_tools().create_asset('WBP_CommandComputer','/Game/Botanicus/UI/Command',unreal.WidgetBlueprint,f)
unreal.log_warning('ASSET '+str(a))
if a:
 for n in dir(a):
  if 'widget' in n.lower() or 'tree' in n.lower() or 'root' in n.lower(): unreal.log_warning('MEM '+n)
 unreal.EditorAssetLibrary.save_loaded_asset(a)
