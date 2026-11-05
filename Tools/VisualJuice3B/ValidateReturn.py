import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
result={'checks':[],'errors':[],'samples':[]}
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',3),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',640),('ClientWindowHeight',360)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();old_worlds=[]
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def step(w,a):return unreal.BotanicusJuiceValidation.step(w,a)
def check(name,ok):
    result['checks'].append(dict(name=name,passed=bool(ok)))
    if not ok:result['errors'].append(name)
def snap(ws):return [dict(world=w.get_path_name(),growth=list(unreal.BotanicusJuiceValidation.read_growth(w)),bursts=unreal.BotanicusJuiceValidation.get_juice(w).get_lifecycle_burst_count(),active=unreal.BotanicusJuiceValidation.get_juice(w).get_active_burst_count()) for w in ws]
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'validation_return.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,old_worlds
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>100:raise RuntimeError('Phase timeout '+phase)
        if phase=='start' and len(ws)==3 and elapsed>8:
            server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w));step(server,'Quiesce');check('Setup illegal',step(server,'SetupIllegal'));check('Restore near maturity',unreal.BotanicusJuiceValidation.restore_growth(server,.9998));step(server,'Grow');phase='grown';start=time.monotonic()
        elif phase=='grown' and elapsed>3:
            server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w));result['samples'].append(dict(phase=phase,peers=snap(ws)))
            check('Maturity once on original three peers',all(r['bursts']==1 and r['growth'][0]==1 for r in snap(ws)))
            old_worlds=[w.get_path_name() for w in ws];check('Request real PIE late join',step(server,'LateJoin'));phase='joined';start=time.monotonic()
        elif phase=='joined' and len(ws)==4 and elapsed>10:
            server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w));rows=snap(ws);result['samples'].append(dict(phase=phase,peers=rows));late=[r for r in rows if r['world'] not in old_worlds]
            check('Late client receives mature state without pulse',len(late)==1 and late[0]['growth'][0]==1 and late[0]['bursts']==0 and late[0]['active']==0)
            check('Host and earlier peers do not replay',all(r['bursts']==1 for r in rows if r['world'] in old_worlds))
            step(server,'RememberTarget');check('Save isolated world',unreal.GameplayStatics.get_game_mode(server).botanicus_save_now());levels.editor_request_end_play();phase='stop';start=time.monotonic()
        elif phase=='stop' and not ws and elapsed>3:
            s.set_editor_property('PlayNumberOfClients',4);levels.editor_request_begin_play();phase='return';start=time.monotonic()
        elif phase=='return' and len(ws)==4 and elapsed>10:
            server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w));step(server,'Quiesce');check('Find saved planter at original position',step(server,'FindRemembered'));rows=snap(ws);result['samples'].append(dict(phase=phase,peers=rows))
            check('Saved maturity restored on all four peers',all(r['growth'] and r['growth'][0]==1 for r in rows));check('Return / load emits no lifecycle burst',all(r['bursts']==0 and r['active']==0 for r in rows));finish()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
