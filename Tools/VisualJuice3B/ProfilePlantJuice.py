import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
result={'samples':[],'blocks':[],'errors':[],'note':'Four rendered PIE worlds; same static rig, existing ticks frozen; 32 extra garden plants, four spraying players; 3A enabled in both conditions, only 3B motion/lifecycle toggled; ABBA 20s blocks'}
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',640),('ClientWindowHeight',360)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();last_sample=0;last_refill=0;block=-1
conditions=[False,True,True,False]
seen={};spawned={}
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def step(w,a):return unreal.BotanicusJuiceValidation.step(w,a)
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'profile_3b.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def begin_block(server):
    global block,phase,start
    block+=1
    if block==len(conditions):finish();return
    step(server,'LifecycleOn' if conditions[block] else 'LifecycleOff');step(server,'RefillFour')
    phase='settle';start=time.monotonic()
def tick(dt):
    global phase,start,last_sample,last_refill,block
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if phase=='start':
            if len(ws)<4:
                if elapsed>90:raise RuntimeError('PIE startup timeout')
                return
            if elapsed<8:return
            server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
            unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0));step(server,'Quiesce')
            host=unreal.GameplayStatics.get_player_pawn(server,0);base=host.get_actor_location()
            for i,pawn in enumerate(unreal.GameplayStatics.get_all_actors_of_class(server,unreal.BotanicusCharacter)):
                if pawn!=host:pawn.set_actor_location(base+unreal.Vector(-150,150*(i+1),0),False,True)
            for w in ws:
                for actor in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    name=actor.get_class().get_name()
                    if any(t in name for t in ['Visitor','IllegalCustomer','IllegalTradeManager']):actor.destroy_actor()
                unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
            assert step(server,'SetupSimple');assert unreal.BotanicusJuiceValidation.restore_growth(server,.7);assert step(server,'Garden');assert len(unreal.GameplayStatics.get_all_actors_with_tag(server,'JuiceGarden'))==32;assert step(server,'WaterFourPrepare')
            phase='equip';start=time.monotonic();return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='equip' and elapsed>4:
            for w in ws:step(w,'InputOn')
            unreal.SystemLibrary.execute_console_command(server,'stat unit',unreal.GameplayStatics.get_player_controller(server,0))
            phase='warm';start=time.monotonic()
        if time.monotonic()-last_refill>2:step(server,'RefillFour');last_refill=time.monotonic()
        if phase=='warm' and elapsed>10:begin_block(server)
        elif phase=='settle' and elapsed>5:
            phase='measure';start=time.monotonic()
            result['blocks'].append(dict(block=block,enabled=conditions[block],seconds=20))
        elif phase=='measure':
            if time.monotonic()-last_sample>.25:
                rows=[]
                for w in ws:
                    name=w.get_path_name();cans=unreal.GameplayStatics.get_all_actors_of_class(w,unreal.BotanicusWateringCanActor)
                    names={a.get_name() for a in cans};fresh=names-seen.get(name,set());spawned[name]=spawned.get(name,0)+len(fresh);seen.setdefault(name,set()).update(names)
                    j=unreal.BotanicusJuiceValidation.get_juice(w)
                    rows.append(dict(world=name,water=j.get_active_water_count(),particles=j.get_particle_count(),bursts=j.get_active_burst_count(),cans=len(cans),new_can_names=len(fresh),total_observed_can_names=spawned[name],actors=len(unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor))))
                result['samples'].append(dict(block=block,enabled=conditions[block],elapsed=elapsed,metrics=list(unreal.BotanicusJuiceValidation.read_frame_metrics(server)),peers=rows))
                last_sample=time.monotonic()
            if elapsed>20:begin_block(server)
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
