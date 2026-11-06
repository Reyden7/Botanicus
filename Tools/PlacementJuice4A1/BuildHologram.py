import unreal,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;DEST='/Game/Botanicus/Materials/Juice/Placement';name='M_PlacementHologram'
lib=unreal.MaterialEditingLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();result={'errors':[]}
try:
    fresh=not unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name)
    mat=tools.create_asset(name,DEST,unreal.Material,unreal.MaterialFactoryNew()) if fresh else unreal.load_asset(DEST+'/'+name)
    if fresh:
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('two_sided',True);mat.set_editor_property('used_with_instanced_static_meshes',True)
        def node(cls):return lib.create_material_expression(mat,cls,-700,0)
        inputs={}
        for key,rgba in [('BodyTint',unreal.LinearColor(.06125,.47932,1,1)),('RimTint',unreal.LinearColor(.26225,.7913,1,1))]:
            n=node(unreal.MaterialExpressionVectorParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',rgba);inputs[key]=n
        for key,value in [('Opacity',.42),('FresnelIntensity',.65),('Emission',1.1)]:
            n=node(unreal.MaterialExpressionScalarParameter);n.set_editor_property('parameter_name',key);n.set_editor_property('default_value',value);inputs[key]=n
        inputs['Rim']=node(unreal.MaterialExpressionFresnel);inputs['Rim'].set_editor_property('exponent',3.0)
        inputs['T']=node(unreal.MaterialExpressionTime)
        def custom(code,output):
            c=node(unreal.MaterialExpressionCustom);c.set_editor_property('output_type',output);pins=[]
            for key,n in inputs.items():p=unreal.CustomInput();p.set_editor_property('input_name',key);pins.append(p)
            c.set_editor_property('inputs',pins);c.set_editor_property('code',code)
            for key,n in inputs.items():assert lib.connect_material_expressions(n,'',c,key)
            return c
        emission=custom('return lerp(BodyTint.rgb,RimTint.rgb,saturate(Rim*FresnelIntensity))*Emission*(.985+.015*sin(T*2.5));',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        opacity=custom('return saturate(Opacity+Rim*FresnelIntensity*.10);',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        assert lib.connect_material_property(emission,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        assert lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
        lib.layout_material_expressions(mat);lib.recompile_material(mat)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
    result['asset']=mat.get_path_name();result['new']=fresh
    old=unreal.load_asset('/Game/Botanicus/Materials/Silhouette/M_Silhouette_Hologram_Blue')
    if old:result['legacy_material']=dict(path=old.get_path_name(),blend=str(old.get_editor_property('blend_mode')))
    cat=unreal.load_asset('/Game/Botanicus/Data/DA_BotanicusItemCatalog')
    result['catalog']=[dict(key=str(d.get_editor_property('item_key')),weight=str(d.get_editor_property('weight_class')),cls=str(d.get_editor_property('world_actor_class')),mesh=str(d.get_editor_property('world_mesh'))) for d in cat.get_editor_property('items')]
except Exception:result['errors'].append(traceback.format_exc())
result['passed']=not result['errors'];(OUT/'hologram_asset.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.SystemLibrary.quit_editor()
