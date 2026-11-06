import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
V=unreal.BotanicusCashRegisterValidation
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
L.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
S=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:S.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
result={'checks':[],'samples':[],'errors':[]}
jobs=[('Prepare',2),('AimScanner',.4),('Take',.5),('AimCustomer',.4),('Scan',.8),('AimCoins',.4),('Collect',2),('AimDock',.4),('Return',.5)]
index=0;phase='start';start=time.monotonic();L.editor_request_begin_play()
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def finish():
    result['passed']=not result['errors'];(OUT/'smoke_client.json').write_text(json.dumps(result,indent=2),encoding='utf-8');L.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>90:raise RuntimeError('Timeout '+phase)
        if len(ws)!=4:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and elapsed>9:phase='act'
        if phase=='act':
            action,delay=jobs[index]
            ok=V.prepare(server,50) if action=='Prepare' else V.aim(ws[1],action[3:]) if action.startswith('Aim') else V.step(ws[1],action)
            result['checks'].append(dict(action=action,issued=ok))
            if not ok:result['errors'].append('Issue '+action)
            phase='wait';start=time.monotonic();return
        if phase=='wait' and elapsed>=jobs[index][1]:
            dump=[]
            for a in unreal.GameplayStatics.get_all_actors_of_class(server,unreal.Actor):
                if 'Scanner' in a.get_name():dump.append(dict(name=a.get_name(),loc=str(a.get_actor_location()),collision=a.get_actor_enable_collision(),boxes=[dict(name=b.get_name(),loc=str(b.get_world_location()),scale=str(b.get_world_scale()),extent=str(b.get_scaled_box_extent()),collision=str(b.get_collision_enabled())) for b in a.get_components_by_class(unreal.BoxComponent)]))
            result['samples'].append(dict(action=jobs[index][0],scannerDump=dump,peers=[json.loads(V.snapshot(w)) for w in ws]));index+=1
            if index==len(jobs):finish();return
            phase='act'
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
