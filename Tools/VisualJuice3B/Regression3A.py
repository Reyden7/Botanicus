import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
(OUT/'captures').mkdir(exist_ok=True)
result={'checks':[],'samples':[],'errors':[],'peak_particles':{},'plant_peaks':{},'perf_samples':[],'harness':'Rendered PIE listen server + 3 clients, real Botanicus GameMode, isolated saves'}
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
settings=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for key,value in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',640),('ClientWindowHeight',360)]:settings.set_editor_property(key,value)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play()
jobs=[('Prepare',2),('EffectsOff',.5),('SetupSimple',2),('CapturePlantBefore',1),('EffectsOn',.5),('Plant',2),('RejectedPlant',2),('WaterPrepare',2),('EffectsOff',.5),('InputOn',.1),('WaterBegin',2),('CaptureWaterBefore',1),('EffectsOn',.3),('WaterMaintain',2),('CaptureWaterAfter',1),('WaterEnd',2),('Wet',2),('Dry',2),('WaterPrepare',1),('InputOn',.1),('WaterBegin',1),('ToolChange',2),('WaterPrepare',1),('InputOn',.1),('WaterBegin',1),('DestroyPot',2)]
for variant in ['Multi','Illegal']:
    jobs += [('Setup'+variant,2),('Plant',2),('RejectedPlant',2),('WaterPrepare',2),('InputOn',.1),('WaterBegin',2),('WaterMaintain',2),('WaterEnd',2),('Wet',2),('DestroyPot',2)]
jobs += [('WaterFourPrepare',2),('InputAllOn',2),('WaterFourMaintain',3),('PerfOff',3),('PerfOn',3),('InputAllOff',3),('NoLeaks',2)]
phase='starting';started=time.monotonic();job=0;baseline=[];last_maintain=0;last_sample=0;variant='Simple';pending_capture=None;input_baseline=[]
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def juice(w):return unreal.BotanicusJuiceValidation.get_juice(w)
def snap(ws):
    rows=[]
    for w in ws:
        j=juice(w);a=unreal.BotanicusAudioValidation.get_audio(w)
        proxies=unreal.GameplayStatics.get_all_actors_with_tag(w,'BotanicusJuiceRemotePreview')
        rows.append(dict(world=w.get_path_name(),server=bool(unreal.GameplayStatics.get_game_mode(w)),plants=j.get_plant_burst_count(),water_starts=j.get_water_start_count(),water_active=j.get_active_water_count(),water_components=j.get_water_component_count(),bursts=j.get_active_burst_count(),particles=j.get_particle_count(),wetness=unreal.BotanicusJuiceValidation.read_wetness(w),wetness_blends=j.get_wetness_transition_count(),audio_ambience_starts=a.get_ambience_start_count(),remote_cans=len(proxies),proxies_excluded_from_save=all(p.actor_has_tag('BotanicusPlacementPreview') for p in proxies)))
    return rows
def check(name,condition):
    result['checks'].append(dict(name=name,passed=bool(condition)))
    if not condition:result['errors'].append('Check failed: '+name)
def step(w,action):return unreal.BotanicusJuiceValidation.step(w,action)
def capture(server,name):unreal.BotanicusJuiceValidation.capture(server,str(OUT/'captures'/name))
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play()
    (OUT/'pie_result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,started,job,baseline,last_maintain,last_sample,variant,pending_capture,input_baseline
    try:
        ws=worlds();elapsed=time.monotonic()-started
        if phase=='starting':
            if len(ws)==4 and elapsed>7:
                check('One authority, three clients',sum(bool(unreal.GameplayStatics.get_game_mode(w)) for w in ws)==1)
                result['initial']=snap(ws);phase='act';started=time.monotonic()
            elif elapsed>90:raise RuntimeError('PIE startup timed out: '+str(len(ws)))
            return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        action,delay=jobs[job]
        if phase=='act':
            baseline=snap(ws)
            if action=='InputOn':input_baseline=baseline
            if action=='Prepare':
                step(server,'Quiesce');host=unreal.GameplayStatics.get_player_pawn(server,0);base=host.get_actor_location()
                for i,pawn in enumerate(unreal.GameplayStatics.get_all_actors_of_class(server,unreal.BotanicusCharacter)):
                    if pawn!=host:pawn.set_actor_location(base+unreal.Vector(-150,100*(i+1),0),False,True)
                unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0))
            elif action=='CapturePlantBefore':capture(server,'planting_before.png')
            elif action=='CaptureWaterBefore':capture(server,'watering_before.png')
            elif action=='CaptureWaterAfter':capture(server,'watering_after.png')
            elif action in ['InputAllOn','InputAllOff']:
                for w in ws:step(w,'InputOn' if action=='InputAllOn' else 'InputOff')
            elif action in ['PerfOff','PerfOn']:
                step(server,'RefillFour');step(server,'EffectsOff' if action=='PerfOff' else 'EffectsOn')
                if action=='PerfOff':unreal.SystemLibrary.execute_console_command(server,'stat unit',unreal.GameplayStatics.get_player_controller(server,0))
            elif action not in ['WaterFourMaintain','NoLeaks']:
                if action.startswith('Setup'):
                    variant=action[5:]
                    if variant=='Illegal':unreal.SystemLibrary.execute_console_command(server,'SetTime 22',unreal.GameplayStatics.get_player_controller(server,0))
                check(variant+' native '+action,step(server,action))
                if action=='Plant':pending_capture='planting_'+variant.lower()+'_after.png'
            phase='wait';started=time.monotonic();last_maintain=0
        elif phase=='wait':
            if action in ['WaterBegin','WaterMaintain','CaptureWaterBefore','EffectsOn','CaptureWaterAfter'] and time.monotonic()-last_maintain>.08:
                if action!='EffectsOn' or variant=='Simple':step(server,'WaterMaintain')
                last_maintain=time.monotonic()
            if time.monotonic()-last_sample>.1:
                for row in snap(ws):
                    result['peak_particles'][row['world']]=max(result['peak_particles'].get(row['world'],0),row['particles'])
                    if action=='Plant':result['plant_peaks'][variant+' '+row['world']]=max(result['plant_peaks'].get(variant+' '+row['world'],0),row['particles'])
                if action in ['PerfOff','PerfOn'] and elapsed>.75:result['perf_samples'].append(dict(action=action,metrics=list(unreal.BotanicusJuiceValidation.read_frame_metrics(server))))
                last_sample=time.monotonic()
            if pending_capture and elapsed>.15 and juice(server).get_particle_count()>0:
                capture(server,pending_capture);pending_capture=None
            if elapsed<delay:return
            after=snap(ws);result['samples'].append(dict(variant=variant,action=action,before=baseline,after=after))
            if action=='Plant':check(variant+' one planting burst per peer',[a['plants']-b['plants'] for a,b in zip(after,baseline)]==[1]*4)
            if action=='Plant':check(variant+' planting particles simulated',all(result['plant_peaks'].get(variant+' '+p['world'],0)>0 for p in after))
            if action=='RejectedPlant':check(variant+' refused planting produces no burst',all(a['plants']==b['plants'] for a,b in zip(after,baseline)))
            if action=='WaterBegin' and variant!='Simple':check(variant+' watering starts once per peer after held input',all(a['water_starts']-b['water_starts']==1 for a,b in zip(after,input_baseline)))
            if action=='WaterMaintain':check(variant+' watering active on all peers',all(a['water_active']==1 for a in after))
            if action=='WaterMaintain':
                check(variant+' Niagara particles actually simulated',all(a['particles']>0 for a in after))
                check(variant+' continuous watering does not restart',all(a['water_starts']==b['water_starts'] for a,b in zip(after,baseline)))
            if action in ['WaterEnd','ToolChange','DestroyPot','InputAllOff','NoLeaks']:
                check(variant+' '+action+' clears water systems',all(a['water_active']==0 and a['water_components']==0 for a in after))
                check(variant+' '+action+' clears residual bursts',all(a['bursts']==0 for a in after))
            if action=='Wet':check(variant+' replicated wetness reaches wet appearance',all(a['wetness']>.98 for a in after))
            if action=='Dry':check(variant+' existing dry state restores dry appearance',all(0<=a['wetness']<.01 for a in after))
            if action=='WaterFourMaintain':check('Four simultaneous sources on every peer',all(a['water_active']==4 and a['particles']>0 for a in after))
            if action=='NoLeaks':check('No retained remote cosmetic cans',all(a['remote_cans']==0 for a in after))
            check(action+' preserves two audio ambience starts',all(a['audio_ambience_starts']==2 for a in after))
            check(action+' burst component budget',all(a['bursts']<=12 for a in after))
            check(action+' cosmetic cans excluded from existing save path',all(a['proxies_excluded_from_save'] for a in after))
            job+=1
            if job==len(jobs):finish()
            else:phase='act';started=time.monotonic()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
