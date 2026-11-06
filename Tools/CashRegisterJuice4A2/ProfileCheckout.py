import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
V=unreal.BotanicusCashRegisterValidation;J=unreal.BotanicusJuiceValidation
E=unreal.BotanicusEquipmentValidation
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);L.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
S=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:S.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
conditions=[False,True,True,False,True,False,False,True]
result={'blocks':[],'samples':[],'errors':[],'protocol':'Four rendered PIE worlds, fixed cash/positions, runtime native 1000cr transaction every 6s. Same finance/scan/audio path with animation OFF or ON; 15s warmup; 6s settle +24s measurement per block; OFF ON ON OFF ON OFF OFF ON; metrics sampled at 10Hz, no image capture or object census during measurement. Fixture work included equally in both conditions; not a shipping benchmark or pre-refactor comparison.'}
phase='start';start=time.monotonic();block=-1;last_sample=0;wave_start=0;wave_step=0;before=[];L.editor_request_begin_play()
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def snap(ws):return [json.loads(V.snapshot(w)) for w in ws]
def finish():
    result['passed']=not result['errors'];(OUT/'profile_checkout.json').write_text(json.dumps(result,indent=2),encoding='utf-8');L.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def next_block(server):
    global block,phase,start
    block+=1;start=time.monotonic()
    if block==len(conditions):phase='drain';return
    assert V.step(server,'On' if conditions[block] else 'Off');phase='settle'
def tick(dt):
    global phase,start,block,last_sample,wave_start,wave_step,before
    try:
        ws=worlds();now=time.monotonic();elapsed=now-start
        if elapsed>100:raise RuntimeError('Timeout '+phase)
        if len(ws)!=4:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and elapsed>10:
            for group in ['Effects','GlobalIllumination','Reflection','Shading','Shadow','PostProcess']:
                unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality 3',unreal.GameplayStatics.get_player_controller(server,0))
            unreal.SystemLibrary.execute_console_command(server,'stat unit',unreal.GameplayStatics.get_player_controller(server,0))
            for w in ws:
                J.step(w,'Quiesce')
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(x in a.get_class().get_name() for x in ['VisitorManager','IllegalTradeManager']):a.set_actor_tick_enabled(False)
                assert E.begin(w)
            assert E.step(server,'SetupAll')
            result['viewports']=[list(unreal.GameplayStatics.get_player_controller(w,0).get_viewport_size()) for w in ws]
            phase='warm';start=now
        if phase in ['warm','settle','measure']:
            t=now-wave_start
            if t>=6:
                assert V.prepare(server,1000);wave_start=now;wave_step=0
            elif wave_step==0 and t>.45:V.aim(server,'Scanner');wave_step+=1
            elif wave_step==1 and t>.7:V.step(server,'KeyE');wave_step+=1
            elif wave_step==2 and t>.95:V.aim(server,'Customer');wave_step+=1
            elif wave_step==3 and t>1.2:V.step(server,'Click');wave_step+=1
            elif wave_step==4 and t>1.85:V.aim(server,'Coins');wave_step+=1
            elif wave_step==5 and t>2.1:V.step(server,'Click');wave_step+=1
            if phase=='warm' and elapsed>15:next_block(server)
            elif phase=='settle' and elapsed>6:
                before=snap(ws)
                for w in ws:E.reset_counters(w)
                phase='measure';start=now;last_sample=0
            elif phase=='measure':
                if now-last_sample>=.1:result['samples'].append(dict(block=block,seconds=elapsed,metrics=list(J.read_frame_metrics(server))));last_sample=now
                if elapsed>=24:
                    result['blocks'].append(dict(index=block,enabled=conditions[block],before=before,peers=snap(ws),equipment=[dict(E.read_counters(w)) for w in ws],seconds=elapsed));unreal.log('Cash benchmark block '+str(block)+' complete');next_block(server)
        elif phase=='drain' and elapsed>6:
            result['final']=snap(ws)
            for b in result['blocks']:
                r=b['peers'][0];z=b['before'][0]
                if r['collectionsAccepted']-z['collectionsAccepted']!=4:result['errors'].append('Unexpected throughput block '+str(b['index']))
            finish()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
