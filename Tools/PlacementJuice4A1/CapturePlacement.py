import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;FRAMES=OUT/'captures'/'placement_frames';FRAMES.mkdir(parents=True,exist_ok=True)
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
V=unreal.BotanicusPlacementValidation;J=unreal.BotanicusJuiceValidation
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',1),('RunUnderOneProcess',True),('EnableGameSound',True),('ClientWindowWidth',1280),('ClientWindowHeight',720)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();index=0;last=0;job=0;origin=0
jobs=[('Preview',1.1),('BlockPreview',.65),('Unblock',.65),('Rotate',.65),('ConfirmPreview',1.5)]
result={'frames':[],'events':[],'errors':[],'note':'Real rendered PIE viewport. One player for presentation only; multiplayer is validated separately. Frame time is recorded in world seconds; no animation time dilation.'}
def worlds():return [w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)]
def clean(w):
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        if a.actor_has_tag('BotanicusEquippedPreview'):a.set_actor_hidden_in_game(True)
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'capture_placement.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index,last,job,origin
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>100:raise RuntimeError('Timeout '+phase)
        if not ws:return
        server=ws[0]
        if phase=='start' and elapsed>8:
            J.step(server,'Quiesce');assert V.prepare(server,'PlantPot')
            unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0))
            for a in unreal.GameplayStatics.get_all_actors_of_class(server,unreal.Actor):
                if any(t in a.get_class().get_name() for t in ['Visitor','IllegalCustomer','IllegalTradeManager']):a.destroy_actor()
            unreal.GameplayStatics.get_game_state(server).set_actor_tick_enabled(False)
            unreal.SystemLibrary.execute_console_command(server,'showhud',unreal.GameplayStatics.get_player_controller(server,0))
            for group in ['Effects','GlobalIllumination','Reflection','Shading','Shadow','PostProcess']:
                unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality 3',unreal.GameplayStatics.get_player_controller(server,0))
            clean(server);phase='settle';start=time.monotonic()
        elif phase=='settle' and elapsed>3:
            phase='record';start=time.monotonic();origin=unreal.GameplayStatics.get_time_seconds(server)
            action,delay=jobs[0];assert V.step(server,action);result['events'].append(dict(action=action,seconds=0))
        elif phase=='record':
            clean(server)
            now=unreal.GameplayStatics.get_time_seconds(server)-origin
            if time.monotonic()-last>1/30:
                name='frame_%04d.bgra'%index;assert V.capture_raw_frame(server,str(FRAMES/name))
                result['frames'].append(dict(index=index,file=name,worldSeconds=now,phase=jobs[job][0],snapshot=json.loads(V.snapshot(server))));index+=1;last=time.monotonic()
            if elapsed>jobs[job][1]:
                job+=1
                if job==len(jobs):finish();return
                action,delay=jobs[job]
                assert V.prepare(server,action[7:]) if action.startswith('Prepare') else V.step(server,action)
                result['events'].append(dict(action=action,seconds=now));start=time.monotonic()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
