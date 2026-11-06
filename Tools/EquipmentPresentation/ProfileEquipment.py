import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
BASELINE=(OUT/'baseline_only.flag').exists()
CONDITIONS=[(True,False),(True,True)] if BASELINE else [(True,True),(False,True),(False,True),(True,True),(False,True),(True,True),(True,True),(False,True)]
result={'blocks':[],'samples':[],'errors':[],'baseline_only':BASELINE,'protocol':'Four rendered PIE worlds, native counters, fixed scene; no screenshots or actor enumeration while measuring. 25s windows, 5s settle; legacy/optimized ABBA + BAAB; Niagara fixed ON except initial diagnostic OFF/ON.'}
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',640),('ClientWindowHeight',360)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();block=-1;last_refill=0;last_sample=0
V=unreal.BotanicusEquipmentValidation;J=unreal.BotanicusJuiceValidation
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def finish():
    result['passed']=not result['errors']
    for w in worlds():V.finish(w)
    levels.editor_request_end_play();(OUT/('before_native.json' if BASELINE else 'profile_equipment.json')).write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def begin_block(server):
    global phase,start,block
    block+=1
    if block==len(CONDITIONS):finish();return
    legacy,niagara=CONDITIONS[block];assert V.set_legacy(server,legacy);assert J.step(server,'EffectsOn' if niagara else 'EffectsOff');assert J.step(server,'RefillFour');phase='settle';start=time.monotonic()
def tick(dt):
    global phase,start,last_refill,last_sample
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if phase=='start':
            if len(ws)<4:
                if elapsed>100:raise RuntimeError('PIE startup timeout')
                return
            if elapsed<8:return
            server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w));assert J.step(server,'Quiesce')
            unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0))
            host=unreal.GameplayStatics.get_player_pawn(server,0);base=host.get_actor_location()
            for i,pawn in enumerate(unreal.GameplayStatics.get_all_actors_of_class(server,unreal.BotanicusCharacter)):
                if pawn!=host:pawn.set_actor_location(base+unreal.Vector(-150,150*(i+1),0),False,True)
            for w in ws:
                assert V.begin(w)
                unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(t in a.get_class().get_name() for t in ['Visitor','IllegalCustomer','IllegalTradeManager']):a.destroy_actor()
            if not BASELINE:assert V.step(server,'CountersOnly')
            assert J.step(server,'WaterFourPrepare');phase='equip';start=time.monotonic();return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if time.monotonic()-last_refill>2:assert J.step(server,'RefillFour');last_refill=time.monotonic()
        if phase=='equip' and elapsed>4:
            for w in ws:assert J.step(w,'InputOn')
            unreal.SystemLibrary.execute_console_command(server,'stat unit',unreal.GameplayStatics.get_player_controller(server,0));phase='warm';start=time.monotonic()
        elif phase=='warm' and elapsed>12:begin_block(server)
        elif phase=='settle' and elapsed>5:
            for w in ws:V.reset_counters(w)
            phase='measure';start=time.monotonic()
        elif phase=='measure':
            if time.monotonic()-last_sample>.1:
                result['samples'].append(dict(block=block,elapsed=elapsed,metrics=list(J.read_frame_metrics(server))));last_sample=time.monotonic()
            if elapsed>25:
                legacy,niagara=CONDITIONS[block];rows=[]
                for w in ws:
                    juice=J.get_juice(w)
                    rows.append(dict(world=w.get_path_name(),counts=dict(V.read_counters(w)),state=json.loads(V.snapshot(w)),water_sources=juice.get_active_water_count(),particles=juice.get_particle_count(),water_starts=juice.get_water_start_count()))
                if niagara:assert all(r['water_sources']==4 for r in rows),'Missing watering sources'
                result['blocks'].append(dict(block=block,legacy=legacy,niagara=niagara,seconds=elapsed,peers=rows));begin_block(server)
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
