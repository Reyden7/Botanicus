import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;CAP=Path(unreal.Paths.project_saved_dir())/'captures';CAP.mkdir(exist_ok=True)
assert '/isolated_user/' in str(CAP).replace('\\','/')
V=unreal.BotanicusCashRegisterValidation;J=unreal.BotanicusJuiceValidation
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);L.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
S=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:S.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
jobs=[('Prepare',2),('AimScanner',.7),('KeyE',.8),('AimCustomer',1),('Click',1.15),('AimCoins',1.25),('Click',2.5),('AimDock',.8),('KeyE',1)]
result={'errors':[],'frames':[],'steps':[],'protocol':'Native E/left-click InputKey injection in actual PIE tick. Actual rendered frames sampled with wall-time timestamps; no fabricated animation or audio track.'}
index=0;phase='start';start=time.monotonic();record_start=0;last_frame=0;L.editor_request_begin_play()
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def finish():
    result['passed']=not result['errors'];(OUT/'capture_checkout.json').write_text(json.dumps(result,indent=2),encoding='utf-8');L.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global index,phase,start,record_start,last_frame
    try:
        ws=worlds();now=time.monotonic();elapsed=now-start
        if elapsed>90:raise RuntimeError('Timeout '+phase)
        if len(ws)!=4:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and elapsed>10:
            for w in ws:
                J.step(w,'Quiesce')
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(x in a.get_class().get_name() for x in ['VisitorManager','IllegalTradeManager']):a.set_actor_tick_enabled(False)
            phase='act'
        if record_start and now-last_frame>=1/24:
            path=CAP/('checkout_%05d.bgra'%len(result['frames']))
            if not V.capture(server,str(path)):raise RuntimeError('Viewport readback failed')
            state=json.loads(V.snapshot(server));result['frames'].append(dict(path=str(path),seconds=now-record_start,state=state));last_frame=now
        if phase=='act':
            action,delay=jobs[index]
            ok=V.prepare(server,100) if action=='Prepare' else V.aim(server,action[3:]) if action.startswith('Aim') else V.step(server,action)
            if not ok:raise RuntimeError(action)
            result['steps'].append(dict(action=action,seconds=now-record_start if record_start else 0));phase='wait';start=now;return
        if phase=='wait' and elapsed>=jobs[index][1]:
            if jobs[index][0]=='Prepare':record_start=now
            index+=1
            if index==len(jobs):finish();return
            phase='act'
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
