import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
V=unreal.BotanicusNoctifloreValidation;J=unreal.BotanicusJuiceValidation
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
settings=unreal.get_default_object(unreal.load_class(None,'/Script/Botanicus.BotanicusNoctifloreSettings'));settings.set_editor_property('bEnabled',False)
collection=unreal.load_asset('/Game/Botanicus/Materials/Juice/Noctiflore/MPC_Noctiflore')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'));s.set_editor_property('PlayNumberOfClients',4);s.set_editor_property('RunUnderOneProcess',True)
levels.editor_request_begin_play();phase='start';start=time.monotonic();result={'checks':[],'errors':[]}
def worlds():return [w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)]
def check(name,ok):
    result['checks'].append(dict(name=name,passed=bool(ok)))
    if not ok:result['errors'].append(name)
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'visibility.json').write_text(json.dumps(result,indent=2));unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>90:raise RuntimeError('Timeout '+phase)
        if len(ws)!=4:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and elapsed>8:
            check('Disabled before initial world sample',all(unreal.MaterialLibrary.get_scalar_parameter_value(w,collection,'NoctGlowIntensity')==0 for w in ws));check('Enable',V.step(server,'On'))
            J.step(server,'Quiesce');check('Setup 16',V.step(server,'Setup'));unreal.SystemLibrary.execute_console_command(server,'SetTime 22',unreal.GameplayStatics.get_player_controller(server,0))
            for w in ws:unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
            phase='active';start=time.monotonic()
        elif phase=='active' and elapsed>5:
            check('Active near listener',J.get_juice(server).get_spore_component_count()>0);check('Hide',V.step(server,'Hide'));phase='hidden';start=time.monotonic()
        elif phase=='hidden' and elapsed>2:
            check('Hidden owner leaves no spores',all(J.get_juice(w).get_spore_component_count()==0 for w in ws));check('Show',V.step(server,'Show'));phase='shown';start=time.monotonic()
        elif phase=='shown' and elapsed>4:
            check('Shown source resumes without rebinding',J.get_juice(server).get_spore_component_count()>0)
            unreal.GameplayStatics.get_player_pawn(server,0).set_actor_location(unreal.Vector(-11000,-12000,235),False,True);phase='far';start=time.monotonic()
        elif phase=='far' and elapsed>3:
            check('Distance culls owner spores',J.get_juice(server).get_spore_component_count()==0);finish()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
