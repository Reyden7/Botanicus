import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;(OUT/'captures').mkdir(exist_ok=True)
result={'checks':[],'samples':[],'errors':[],'peaks':{},'harness':'Rendered listen host + 3 clients, native simulation, isolated copied saves'}
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',640),('ClientWindowHeight',360)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();job=0;baseline=[];variant='Simple';pending=None
jobs=[('Prepare',2)]
for kind in ['Simple','Multi','Illegal']:
    jobs += [('Setup'+kind,2),('Plant',2)]
    for threshold in [.3,.7,1.]:jobs += [('Restore'+str(threshold-(.0002 if threshold==1 else .00001)),1.8),('Grow'+str(threshold),1.8)]
    jobs += [('Restore1.0',1.8),('HarvestRefused',1.8),('Harvest',1.8),('Restore0.01',1.8),('FastGrow',1.8),('DuplicateLifecycle',1),('DestroyPot',2)]
jobs += [('NoLeaks',2)]
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def step(w,a):return unreal.BotanicusJuiceValidation.step(w,a)
def snap(ws):
    rows=[]
    for w in ws:
        j=unreal.BotanicusJuiceValidation.get_juice(w);target=unreal.BotanicusJuiceValidation.get_target(w);meshes=[];progress=None
        if target:
            progress=list(unreal.BotanicusJuiceValidation.read_growth(w))
            for c in target.get_components_by_class(unreal.StaticMeshComponent):
                if any(t in c.get_name() for t in ['Foliage','MultiFlower','PlantVisual']):
                    mesh=c.get_editor_property('static_mesh')
                    meshes.append(dict(name=c.get_name(),mesh=mesh.get_path_name() if mesh else None,materials=[m.get_path_name() if m else None for m in c.get_materials()],visible=c.is_visible()))
        rows.append(dict(world=w.get_path_name(),bursts=j.get_lifecycle_burst_count(),last=j.get_last_lifecycle_event(),active=j.get_active_burst_count(),particles=j.get_particle_count(),planting=j.get_plant_burst_count(),progress=progress,meshes=meshes))
    return rows
def check(name,condition):
    result['checks'].append(dict(name=name,passed=bool(condition)))
    if not condition:result['errors'].append(name)
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'validation_growth.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,job,baseline,variant,pending
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if phase=='start':
            if len(ws)==4 and elapsed>8:
                check('One authority and three clients',sum(bool(unreal.GameplayStatics.get_game_mode(w)) for w in ws)==1);phase='act'
            elif elapsed>100:raise RuntimeError('Startup timeout')
            return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w));action,delay=jobs[job]
        if phase=='act':
            baseline=snap(ws)
            if action=='Prepare':
                step(server,'Quiesce');host=unreal.GameplayStatics.get_player_pawn(server,0);base=host.get_actor_location()
                for i,p in enumerate(unreal.GameplayStatics.get_all_actors_of_class(server,unreal.BotanicusCharacter)):
                    if p!=host:p.set_actor_location(base+unreal.Vector(-150,100*(i+1),0),False,True)
                unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0))
            elif action.startswith('Restore'):
                check(variant+' '+action+' native restore',unreal.BotanicusJuiceValidation.restore_growth(server,float(action[7:])))
            elif action.startswith('Grow'):
                check(variant+' '+action+' native tick',step(server,'Grow'));pending=variant.lower()+'_'+action.lower()+'.png'
            elif action!='NoLeaks':
                if action.startswith('Setup'):
                    variant=action[5:]
                    unreal.SystemLibrary.execute_console_command(server,'SetTime '+('22' if variant=='Illegal' else '12'),unreal.GameplayStatics.get_player_controller(server,0))
                check(variant+' native '+action,step(server,action))
                if action=='Harvest':pending=variant.lower()+'_harvest.png'
            phase='wait';start=time.monotonic()
        else:
            rows=snap(ws)
            for row in rows:
                key=variant+' '+action+' '+row['world'];result['peaks'][key]=max(result['peaks'].get(key,0),row['particles'])
            if pending and elapsed>.08 and any(r['particles']>0 for r in rows):
                unreal.BotanicusJuiceValidation.capture(server,str(OUT/'captures'/pending));pending=None
            if elapsed<delay:return
            after=rows;result['samples'].append(dict(variant=variant,action=action,before=baseline,after=after))
            delta=[a['bursts']-b['bursts'] for a,b in zip(after,baseline)]
            if action.startswith('Restore'):check(variant+' '+action+' no replay on restore / replication',delta==[0]*4)
            if action.startswith('Grow') or action=='Harvest':
                check(variant+' '+action+' once on every peer',delta==[1]*4)
                check(variant+' '+action+' actual particles on every peer',all(result['peaks'].get(variant+' '+action+' '+r['world'],0)>0 for r in rows))
                expected=4 if action=='Harvest' else {'.3':1,'.7':2,'1.0':3}.get(action[4:].replace('0.','.'),3)
                check(variant+' '+action+' correct event',all(r['last']==expected for r in rows))
            if action=='HarvestRefused':check(variant+' full inventory emits nothing',delta==[0]*4)
            if action=='FastGrow':check(variant+' fast simulation emits at most one event per peer',all(0<=d<=1 for d in delta))
            if action=='Plant':check(variant+' 3A planting still broadcasts once',[a['planting']-b['planting'] for a,b in zip(after,baseline)]==[1]*4)
            check(variant+' '+action+' cleanup / shared budget',all(r['active']==0 and r['particles']==0 for r in rows))
            job+=1
            if job==len(jobs):finish()
            else:phase='act'
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
