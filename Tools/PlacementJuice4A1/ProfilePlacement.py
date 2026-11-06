import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
V=unreal.BotanicusPlacementValidation;J=unreal.BotanicusJuiceValidation;E=unreal.BotanicusEquipmentValidation
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();last=0;wave=0;block=-1;before=[]
conditions=[False,True,True,False,True,False,False,True]
result={'blocks':[],'samples':[],'errors':[],'protocol':'Four native PlantPot placements every second, old test pots removed at each wave; WateringCan stays selected, placements consume a non-selected slot through the native server RPC; four rendered worlds; FX3; OFF/ON/ON/OFF + ON/OFF/OFF/ON; 25s per block, 5s settle, 15s warm-up; no capture or actor/material census during measurement. This is a stress fixture, not a player input binding.'}
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def snap(ws):return [json.loads(V.snapshot(w)) for w in ws]
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'profile_placement.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def next_block(server):
    global block,phase,start
    block+=1
    if block==len(conditions):phase='drain';start=time.monotonic();return
    assert V.step(server,'On' if conditions[block] else 'Off');phase='settle';start=time.monotonic()
def tick(dt):
    global phase,start,last,wave,before
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>100:raise RuntimeError('Timeout '+phase)
        if len(ws)!=4:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and elapsed>8:
            for group in ['Effects','GlobalIllumination','Reflection','Shading','Shadow','PostProcess']:
                unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality 3',unreal.GameplayStatics.get_player_controller(server,0))
            result['quality']={key:unreal.SystemLibrary.get_console_variable_int_value(key) for key in ['sg.EffectsQuality','sg.GlobalIlluminationQuality','sg.ReflectionQuality','sg.ShadingQuality','sg.ShadowQuality','sg.PostProcessQuality','r.DynamicGlobalIlluminationMethod','r.ReflectionMethod','net.MaxRPCPerNetUpdate']}
            assert J.step(server,'Quiesce');assert V.step(server,'PrepareWave')
            unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0))
            for w in ws:
                unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(t in a.get_class().get_name() for t in ['Visitor','IllegalCustomer','IllegalTradeManager']):a.destroy_actor()
                assert E.begin(w);assert E.set_legacy(w,False)
            result['viewport_sizes']=[list(unreal.GameplayStatics.get_player_controller(w,0).get_viewport_size()) for w in ws]
            unreal.SystemLibrary.execute_console_command(server,'stat unit',unreal.GameplayStatics.get_player_controller(server,0));phase='warm';start=time.monotonic()
        elif phase=='drain' and elapsed>.8:
            result['final']=snap(ws)
            for b in result['blocks']:
                if b['enabled'] and not all(a['started']-z['started']==b['peers'][0]['started']-b['before'][0]['started'] for a,z in zip(b['peers'],b['before'])):result['errors'].append('Missing simultaneous placements in block '+str(b['block']))
            if not all(r['active']==0 and r['copies']==0 and r['created']==r['destroyed'] for r in result['final']):result['errors'].append('Temporary components remain after drain')
            finish()
        elif phase=='warm' and elapsed>15:next_block(server)
        elif phase in ['settle','measure']:
            if time.monotonic()-wave>1:
                assert V.step(server,'Wave');wave=time.monotonic()
            if phase=='settle' and elapsed>5:
                before=snap(ws)
                for w in ws:E.reset_counters(w)
                phase='measure';start=time.monotonic();last=0
            elif phase=='measure':
                if time.monotonic()-last>.1:
                    result['samples'].append(dict(block=block,seconds=elapsed,metrics=list(J.read_frame_metrics(server))));last=time.monotonic()
                if elapsed>25:
                    result['blocks'].append(dict(block=block,enabled=conditions[block],seconds=elapsed,before=before,peers=snap(ws),equipment=[dict(E.read_counters(w)) for w in ws]));unreal.log('Placement benchmark block '+str(block)+' complete');next_block(server)
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
