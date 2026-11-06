import unreal,json,traceback
from pathlib import Path
O=Path(__file__).parent;R=Path(__file__).resolve().parents[2];tools=unreal.AssetToolsHelpers.get_asset_tools();r={'created':[],'errors':[]}
def save(obj):
    assert unreal.EditorAssetLibrary.save_loaded_asset(obj,only_if_is_dirty=False),obj.get_path_name()
def fresh(name,folder,cls,factory):
    path=folder+'/'+name
    if unreal.EditorAssetLibrary.does_asset_exist(path):return unreal.load_asset(path),False
    obj=tools.create_asset(name,folder,cls,factory);assert obj,path;r['created'].append(obj.get_path_name());return obj,True
try:
    unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
    for name,cls in [('BP_BotanicusScanner','BotanicusCheckoutScanner'),('BP_BotanicusPaymentCoins','BotanicusPaymentCoins')]:
        factory=unreal.BlueprintFactory();factory.set_editor_property('parent_class',unreal.load_class(None,'/Script/Botanicus.'+cls))
        bp,new=fresh(name,'/Game/Botanicus/blueprints/Checkout',unreal.Blueprint,factory)
        if new:unreal.BlueprintEditorLibrary.compile_blueprint(bp);save(bp)
    lib=unreal.MaterialEditingLibrary;folder='/Game/Botanicus/Materials/Juice/Checkout'
    for name,beam in [('M_CheckoutBody',False),('M_CheckoutBeam',True)]:
        mat,new=fresh(name,folder,unreal.Material,unreal.MaterialFactoryNew())
        if new:
            mat.set_editor_property('used_with_instanced_static_meshes',True)
            color=lib.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-500,0);color.set_editor_property('parameter_name','Color');color.set_editor_property('default_value',unreal.LinearColor(.08,.55,.9,1) if beam else unreal.LinearColor(.5,.5,.5,1))
            if beam:
                mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_ADDITIVE);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT);mat.set_editor_property('two_sided',True)
                fade=lib.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,-500,200);fade.set_editor_property('parameter_name','Fade');fade.set_editor_property('default_value',1.)
                lib.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);lib.connect_material_property(fade,'',unreal.MaterialProperty.MP_OPACITY)
            else:
                lib.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
                rough=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-500,200);rough.set_editor_property('r',.45);lib.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
                metal=lib.create_material_expression(mat,unreal.MaterialExpressionConstant,-500,300);metal.set_editor_property('r',.35);lib.connect_material_property(metal,'',unreal.MaterialProperty.MP_METALLIC)
            lib.recompile_material(mat);save(mat)
    concurrency,new=fresh('SCON_CheckoutCoins','/Game/Botanicus/Audio/Settings',unreal.SoundConcurrency,unreal.SoundConcurrencyFactory())
    if new:
        settings=unreal.SoundConcurrencySettings();settings.set_editor_property('max_count',3);settings.set_editor_property('resolution_rule',unreal.MaxConcurrentResolutionRule.STOP_OLDEST);settings.set_editor_property('retrigger_time',.04);settings.set_editor_property('voice_steal_release_time',.025);concurrency.set_editor_property('concurrency',settings);save(concurrency)
    catalog=unreal.load_asset('/Game/Botanicus/Audio/DA_BotanicusAudio');assert catalog
    def feedback_values(entries):return {str(k):dict(sound=v.sound.get_path_name() if v.sound else None,volume=v.volume,pitch=v.pitch,interval=v.minimum_interval,concurrency=v.concurrency.get_path_name() if v.concurrency else None) for k,v in entries.items()}
    entries=dict(catalog.get_editor_property('feedback'));before=feedback_values(entries);sounds=[]
    for row in json.loads((R/'Botanicus/SourceArt/Audio/Checkout/manifest.json').read_text(encoding='utf-8')):
        name='SW_Checkout'+row['id'];path='/Game/Botanicus/Audio/Sounds/'+name
        if not unreal.EditorAssetLibrary.does_asset_exist(path):
            task=unreal.AssetImportTask();task.filename=row['wav'];task.destination_path='/Game/Botanicus/Audio/Sounds';task.destination_name=name;task.automated=True;task.replace_existing=False;task.save=False
            tools.import_asset_tasks([task]);assert task.imported_object_paths,name;r['created']+=list(task.imported_object_paths)
            sound=unreal.load_asset(task.imported_object_paths[0]);sound.set_editor_property('looping',False);save(sound)
        else:sound=unreal.load_asset(path)
        e=unreal.BotanicusSoundFeedback();e.sound=sound;e.volume=.45 if row['id']=='CoinPickup' else .6;e.pitch=1.;e.minimum_interval=.055 if row['id']=='CoinPickup' else .1
        e.concurrency=concurrency if row['id']=='CoinPickup' else unreal.load_asset('/Game/Botanicus/Audio/Settings/SCON_Feedback');entries[row['id']]=e
        sounds.append(dict(id=row['id'],path=sound.get_path_name(),duration=sound.get_editor_property('duration'),channels=sound.get_editor_property('num_channels')))
    catalog.set_editor_property('feedback',entries)
    # All old entries, ambience, loops and attenuation retain their values.
    after=feedback_values(dict(catalog.get_editor_property('feedback')));assert all(after[k]==v for k,v in before.items()), 'Existing feedback changed'
    save(catalog);r.update(sounds=sounds,preserved_existing_feedback=list(before),passed=True)
except Exception:r['errors'].append(traceback.format_exc());r['passed']=False
(O/'assets.json').write_text(json.dumps(r,indent=2),encoding='utf-8');unreal.SystemLibrary.quit_editor()
