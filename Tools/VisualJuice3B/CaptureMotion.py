import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;FRAMES=OUT/'captures'/'motion_frames';FRAMES.mkdir(parents=True,exist_ok=True)
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
result={'frames':[],'errors':[]}
collection=unreal.load_asset('/Game/Botanicus/Materials/Juice/Growth/MPC_PlantMotion')
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
s.set_editor_property('PlayNumberOfClients',4);s.set_editor_property('RunUnderOneProcess',True)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();index=0;last=0
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def step(w,a):return unreal.BotanicusJuiceValidation.step(w,a)
def amplitudes(ws):return [unreal.MaterialLibrary.get_scalar_parameter_value(w,collection,'BotanicusMotionAmplitude') for w in ws]
def state(w):
    target=unreal.BotanicusJuiceValidation.get_target(w)
    rows=[]
    for c in target.get_components_by_class(unreal.StaticMeshComponent):
        loc=c.get_editor_property('relative_location');rot=c.get_editor_property('relative_rotation');scale=c.get_editor_property('relative_scale3d')
        rows.append(dict(name=c.get_name(),location=[loc.x,loc.y,loc.z],rotation=[rot.pitch,rot.yaw,rot.roll],scale=[scale.x,scale.y,scale.z],collision=str(c.get_collision_enabled())))
    return rows
def finish():
    levels.editor_request_end_play();(OUT/'capture_motion.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index,last
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>100:raise RuntimeError('Timeout '+phase)
        if len(ws)!=4:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and elapsed>8:
            step(server,'Quiesce');step(server,'SetupSimple');unreal.BotanicusJuiceValidation.restore_growth(server,.7)
            unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0))
            for w in ws:
                unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(t in a.get_class().get_name() for t in ['Visitor','IllegalCustomer','IllegalTradeManager']):a.destroy_actor()
            step(server,'LifecycleOff');phase='before';start=time.monotonic()
        elif phase=='before' and elapsed>3:
            result['before']=state(server);result['amplitudes_off']=amplitudes(ws);assert unreal.BotanicusJuiceValidation.capture_frame(server,str(OUT/'captures'/'organic_before.png'));phase='enable';start=time.monotonic()
        elif phase=='enable' and elapsed>1:
            step(server,'LifecycleOn');phase='record';start=time.monotonic();last=0
        elif phase=='record' and elapsed>.5 and time.monotonic()-last>.2:
            assert unreal.BotanicusJuiceValidation.capture_frame(server,str(FRAMES/('frame_%03d.png'%index)))
            result['frames'].append(dict(index=index,elapsed=elapsed));last=time.monotonic();index+=1
            if index>=32:phase='done';start=time.monotonic()
        elif phase=='done' and elapsed>1:
            result['after']=state(server);result['transforms_and_collision_unchanged']=result['before']==result['after']
            result['active_bursts']=unreal.BotanicusJuiceValidation.get_juice(server).get_active_burst_count();result['amplitudes_on']=amplitudes(ws)
            unreal.SystemLibrary.execute_console_command(server,'sg.EffectsQuality 0',unreal.GameplayStatics.get_player_controller(server,0));phase='low';start=time.monotonic()
        elif phase=='low' and elapsed>3:
            result['amplitudes_low']=amplitudes(ws);result['low_quality_disables_motion']=all(a==0 for a in result['amplitudes_low']);finish()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
