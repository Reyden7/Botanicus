"""UE Python: supported factories/duplication only; never saves original assets."""
import unreal,json,hashlib,traceback
from pathlib import Path
OUT=Path(__file__).parent;DEST='/Game/Botanicus/Materials/Juice/Noctiflore'
tools=unreal.AssetToolsHelpers.get_asset_tools();lib=unreal.MaterialEditingLibrary
r={'variants':{},'created':[],'errors':[]}
def exists(name):return unreal.EditorAssetLibrary.does_asset_exist(DEST+'/'+name)
def save(a):
    assert a.get_path_name().startswith(DEST+'/')
    assert unreal.EditorAssetLibrary.save_loaded_asset(a,False)
    if a.get_path_name() not in r['created']:r['created'].append(a.get_path_name())
def create(name,cls,factory):return unreal.load_asset(DEST+'/'+name) if exists(name) else tools.create_asset(name,DEST,cls,factory)
def dup(a,name):return unreal.load_asset(DEST+'/'+name) if exists(name) else tools.duplicate_asset(name,DEST,a)
def node(mat,cls):return lib.create_material_expression(mat,cls,-800,0)
def custom(mat,inputs,code,output):
    n=node(mat,unreal.MaterialExpressionCustom);n.set_editor_property('output_type',output)
    pins=[]
    for key,value in inputs.items():
        p=unreal.CustomInput();p.set_editor_property('input_name',key);pins.append(p)
    n.set_editor_property('inputs',pins);n.set_editor_property('code',code)
    for key,(src,pin) in inputs.items():assert lib.connect_material_expressions(src,pin,n,key)
    return n
try:
    mpc=create('MPC_Noctiflore',unreal.MaterialParameterCollection,unreal.MaterialParameterCollectionFactoryNew())
    if not mpc.get_editor_property('scalar_parameters'):
        values=[]
        for key,value in [('NoctGlowIntensity',.65),('NoctPulseSpeed',.65),('NoctGlowDistance',1800)]:
            p=unreal.CollectionScalarParameter();p.set_editor_property('parameter_name',key);p.set_editor_property('default_value',value);values.append(p)
        mpc.set_editor_property('scalar_parameters',values)
        p=unreal.CollectionVectorParameter();p.set_editor_property('parameter_name','NoctGlowTint');p.set_editor_property('default_value',unreal.LinearColor(.42,.28,.65,1));mpc.set_editor_property('vector_parameters',[p])
    save(mpc)
    motion=unreal.load_asset('/Game/Botanicus/Materials/Juice/Growth/DA_PlantMotion');living=dict(motion.get_editor_property('variants'))
    plants=unreal.get_default_object(unreal.BotanicusIllegalTradeSettings).get_editor_property('plants');originals={}
    for plant in plants:
        if str(plant.get_editor_property('plant_id'))!='Noctiflore':continue
        for stage in ['seedling_mesh','young_mesh','mature_mesh']:
            for slot in plant.get_editor_property(stage).get_editor_property('static_materials'):
                mat=slot.get_editor_property('material_interface');originals[mat.get_path_name()]=mat
    mapping={};bases={}
    for path,original in originals.items():
        source=living[original];base=source.get_base_material();bp=base.get_path_name()
        assert not base.get_editor_property('use_material_attributes')
        if bp not in bases:
            name='M_NoctGlow_'+hashlib.sha1(bp.encode()).hexdigest()[:10];fresh=not exists(name);mat=dup(base,name)
            if fresh:
                old=lib.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
                oldpin=lib.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
                inputs={}
                for key,cls in [('Pos',unreal.MaterialExpressionWorldPosition),('Centre',unreal.MaterialExpressionObjectPositionWS),('Bounds',unreal.MaterialExpressionObjectBounds),('Camera',unreal.MaterialExpressionCameraPositionWS),('T',unreal.MaterialExpressionTime)]:inputs[key]=(node(mat,cls),'')
                inputs['Pos'][0].set_editor_property('world_position_shader_offset',unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
                for key,param in [('Amp','NoctGlowIntensity'),('Speed','NoctPulseSpeed'),('MaxDistance','NoctGlowDistance'),('Tint','NoctGlowTint')]:
                    n=node(mat,unreal.MaterialExpressionCollectionParameter);n.set_editor_property('collection',mpc);n.set_editor_property('parameter_name',param);inputs[key]=(n,'')
                strength=node(mat,unreal.MaterialExpressionScalarParameter);strength.set_editor_property('parameter_name','NoctGrowthStrength');strength.set_editor_property('default_value',1);inputs['Strength']=(strength,'')
                glow=custom(mat,inputs,'''if (Amp<=0.0) return float3(0,0,0);
float h=saturate((Pos.z-Centre.z+Bounds.z)/max(2.0*Bounds.z,0.01));
float phase=dot(Centre.xy,float2(0.031,0.047));
float fade=1.0-smoothstep(MaxDistance*0.7,MaxDistance,distance(Camera,Centre));
float pulse=0.86+0.14*sin(T*Speed+phase);
return Tint.rgb*Amp*Strength*pulse*(0.10+0.90*smoothstep(0.45,0.95,h))*fade;''',unreal.CustomMaterialOutputType.CMOT_FLOAT3)
                glow.set_editor_property('description','Noctiflore only: subtle additive emission; stable spatial phase across existing meshes')
                quality=node(mat,unreal.MaterialExpressionQualitySwitch);zero=node(mat,unreal.MaterialExpressionConstant3Vector)
                assert lib.connect_material_expressions(glow,'',quality,'Default');assert lib.connect_material_expressions(zero,'',quality,'Low')
                add=node(mat,unreal.MaterialExpressionAdd)
                if old:assert lib.connect_material_expressions(old,oldpin,add,'A')
                assert lib.connect_material_expressions(quality,'',add,'B');assert lib.connect_material_property(add,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
                lib.layout_material_expressions(mat);lib.recompile_material(mat)
            save(mat);bases[bp]=mat
        name='MI_NoctGlow_'+hashlib.sha1(path.encode()).hexdigest()[:10];fresh=not exists(name);variant=dup(source,name)
        if fresh:
            lib.set_material_instance_parent(variant,bases[bp]);lib.update_material_instance(variant);save(variant)
        elif variant.get_path_name() not in r['created']:r['created'].append(variant.get_path_name())
        mapping[original]=variant;r['variants'][path]=variant.get_path_name()
    cls=unreal.load_class(None,'/Script/Botanicus.BotanicusPlantMotionCatalog');factory=unreal.DataAssetFactory();factory.set_editor_property('data_asset_class',cls)
    catalog=create('DA_NoctifloreGlow',cls,factory)
    existing=dict(catalog.get_editor_property('variants'))
    for original,variant in mapping.items():existing.setdefault(original,variant)
    catalog.set_editor_property('variants',existing)
    if not catalog.get_editor_property('parameters'):catalog.set_editor_property('parameters',mpc)
    save(catalog)
    fresh=not exists('M_NoctifloreSpore');mat=create('M_NoctifloreSpore',unreal.Material,unreal.MaterialFactoryNew())
    if fresh:
        mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_TRANSLUCENT);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
        mat.set_editor_property('two_sided',True);mat.set_editor_property('used_with_niagara_sprites',True)
        color=node(mat,unreal.MaterialExpressionParticleColor);uv=node(mat,unreal.MaterialExpressionTextureCoordinate);life=node(mat,unreal.MaterialExpressionParticleRelativeTime)
        assert lib.connect_material_property(color,'RGB',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        opacity=custom(mat,{'UV':(uv,''),'Age':(life,''),'Alpha':(color,'A')},'float2 p=(UV-.5)*2;float edge=saturate(1-dot(p,p));return edge*edge*smoothstep(0,.18,Age)*(1-smoothstep(.6,1,Age))*Alpha;',unreal.CustomMaterialOutputType.CMOT_FLOAT1)
        assert lib.connect_material_property(opacity,'',unreal.MaterialProperty.MP_OPACITY)
        lib.layout_material_expressions(mat);lib.recompile_material(mat)
    save(mat)
    assert unreal.BotanicusJuiceAssetTools.build_noctiflore_spores()
    r['created'].append('/Game/Botanicus/VFX/Noctiflore/NS_NoctifloreSpores.NS_NoctifloreSpores')
    r['niagara']=unreal.BotanicusJuiceAssetTools.inspect_niagara('/Game/Botanicus/VFX/Noctiflore/NS_NoctifloreSpores')
except Exception:r['errors'].append(traceback.format_exc())
r['passed']=not r['errors'];(OUT/'assets.json').write_text(json.dumps(r,indent=2),encoding='utf-8');unreal.SystemLibrary.quit_editor()
