"""Run with UE 5.8 editor Python. Only creates assets under Materials/Juice/Growth.
Never saves the source meshes/materials or Interchange plugin content.
"""
import unreal,json,hashlib,traceback
from pathlib import Path
OUT=Path(unreal.Paths.project_saved_dir())/'Diagnostics'/'VisualJuice3B'
OUT.mkdir(parents=True,exist_ok=True)
DEST='/Game/Botanicus/Materials/Juice/Growth'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
report={'variants':{},'bases':{},'errors':[]}
def save(asset):
    assert asset.get_path_name().startswith(DEST+'/')
    assert unreal.EditorAssetLibrary.save_loaded_asset(asset,False)
def create(name,cls,factory):
    return unreal.load_asset(DEST+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name) else tools.create_asset(name,DEST,cls,factory)
def duplicate(src,name):
    return unreal.load_asset(DEST+'/'+name) if unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name) else tools.duplicate_asset(name,DEST,src)
try:
    collection=create('MPC_PlantMotion',unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
    params=[]
    for name,value in [('BotanicusMotionAmplitude',.25),('BotanicusMotionSpeed',.8),('BotanicusMotionDistance',1800)]:
        p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',name);p.set_editor_property('default_value',value);params.append(p)
    # Only set on the first run: keep expression GUIDs stable when regenerating.
    if not collection.get_editor_property('scalar_parameters'):collection.set_editor_property('scalar_parameters',params)
    save(collection)
    originals={}
    cat=unreal.load_asset('/Game/Botanicus/Data/DA_BotanicusPlantCatalog')
    meshes=[]
    for p in cat.get_editor_property('plants'):
        for key in ['small_growth_mesh','medium_growth_mesh','large_growth_mesh','mature_growth_mesh']:
            mesh=p.get_editor_property(key)
            if isinstance(mesh,unreal.StaticMesh):meshes.append(mesh)
    illegal=unreal.get_default_object(unreal.BotanicusIllegalTradeSettings)
    for plant in illegal.get_editor_property('plants'):
        for key in ['seedling_mesh','young_mesh','mature_mesh']:
            ref=plant.get_editor_property(key)
            if isinstance(ref,unreal.StaticMesh):meshes.append(ref)
    for mesh in meshes:
        for slot in mesh.get_editor_property('static_materials'):
            mat=slot.get_editor_property('material_interface')
            if mat:originals[mat.get_path_name()]=mat
    mapping={};bases={}
    for path,original in sorted(originals.items()):
        base=original.get_base_material();basepath=base.get_path_name()
        if base.get_editor_property('use_material_attributes') or lib.get_material_property_input_node(base,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
            report['errors'].append('Incompatible, untouched: '+path);continue
        if basepath not in bases:
            name='M_Living_'+hashlib.sha1(basepath.encode()).hexdigest()[:10]
            mat=duplicate(base,name)
            if not lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET):
                def node(cls):return lib.create_material_expression(mat,cls,-900,0)
                custom=node(unreal.MaterialExpressionCustom)
                custom.set_editor_property('description','Rooted slow foliage motion; world centimetres; no gameplay clock')
                custom.set_editor_property('output_type',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
                inputs=[]
                for key in ['Pos','Centre','Bounds','Camera','T','Amp','Speed','MaxDistance','Alive']:
                    i=unreal.CustomInput();i.set_editor_property('input_name',key);inputs.append(i)
                custom.set_editor_property('inputs',inputs)
                custom.set_editor_property('code','''float h=saturate((Pos.z-Centre.z+Bounds.z)/max(2.0*Bounds.z,0.01));
float phase=dot(Centre.xy,float2(0.031,0.047));
float fade=1.0-smoothstep(MaxDistance*0.7,MaxDistance,distance(Camera,Centre));
float t=T*Speed;
return float3(sin(t+phase),0.65*sin(t*0.83+phase*1.37),0.0)*h*h*Amp*Alive*fade;''')
                sources={
                    'Pos':node(unreal.MaterialExpressionWorldPosition),
                    'Centre':node(unreal.MaterialExpressionObjectPositionWS),
                    'Bounds':node(unreal.MaterialExpressionObjectBounds),
                    'Camera':node(unreal.MaterialExpressionCameraPositionWS),
                    'T':node(unreal.MaterialExpressionTime)}
                sources['Pos'].set_editor_property('world_position_shader_offset',unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
                for key,param in [('Amp','BotanicusMotionAmplitude'),('Speed','BotanicusMotionSpeed'),('MaxDistance','BotanicusMotionDistance')]:
                    n=node(unreal.MaterialExpressionCollectionParameter);n.set_editor_property('collection',collection);n.set_editor_property('parameter_name',param);sources[key]=n
                alive=node(unreal.MaterialExpressionScalarParameter);alive.set_editor_property('parameter_name','BotanicusMotionAlive');alive.set_editor_property('default_value',1);sources['Alive']=alive
                for key,n in sources.items():assert lib.connect_material_expressions(n,'',custom,key),key
                quality=node(unreal.MaterialExpressionQualitySwitch);zero=node(unreal.MaterialExpressionConstant3Vector)
                assert lib.connect_material_expressions(custom,'',quality,'Default')
                assert lib.connect_material_expressions(zero,'',quality,'Low')
                assert lib.connect_material_property(quality,'',unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
                mat.set_editor_property('max_world_position_offset_displacement',1.0)
                lib.layout_material_expressions(mat);lib.recompile_material(mat)
            save(mat);bases[basepath]=mat;report['bases'][basepath]=mat.get_path_name()
        if isinstance(original,unreal.MaterialInstanceConstant):
            # Retain every imported texture/scalar/vector/static switch override.
            parent=original.get_editor_property('parent')
            assert parent==base,'Nested MI requires an explicit preserving clone: '+path
            variant=duplicate(original,'MI_Living_'+hashlib.sha1(path.encode()).hexdigest()[:10])
            lib.set_material_instance_parent(variant,bases[basepath]);lib.update_material_instance(variant);save(variant)
        else:variant=bases[basepath]
        mapping[original]=variant;report['variants'][path]=variant.get_path_name()
    catalog_class=unreal.load_class(None,'/Script/Botanicus.BotanicusPlantMotionCatalog')
    factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',catalog_class)
    catalog=create('DA_PlantMotion',catalog_class,factory)
    catalog.set_editor_property('variants',mapping);catalog.set_editor_property('parameters',collection);save(catalog)
except Exception:report['errors'].append(traceback.format_exc())
(OUT/'motion_build.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
if report['errors']:raise RuntimeError(str(report['errors']))
unreal.log('BOTANICUS_MOTION_BUILT '+str(len(mapping)))
