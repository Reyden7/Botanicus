import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;(OUT/'captures').mkdir(exist_ok=True)
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
V=unreal.BotanicusPlacementValidation;J=unreal.BotanicusJuiceValidation
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
result={'checks':[],'errors':[],'samples':[],'animation_samples':[]}
levels.editor_request_begin_play();phase='start';start=time.monotonic();index=0;before=[];saved=[]
jobs=[('PreparePlantPot',2),('Preview',2),('BlockPreview',.4),('Unblock',.4),('Rotate',.4),('Cancel',.5),('Reject',.5),('Place',.8),('PreparePlantPot',1),('ClientPlace',.8),('PrepareWaterReserve',1),('Place',.8),('PrepareSalesDisplay',1),('Place',.8),('PrepareStorageShelfFloorLarge',1),('LargeConfirm',.8),('PreparePreparationWorkbench',1),('LargeConfirm',.8),('PreparePlantPot',1),('Place',.8),('FillPlant',1),('AimMove',.5),('ReserveMove',.3),('Move',.8),('PreparePlantPot',1),('Place',.07),('Destroy',.8),('PreparePlantPot',1),('Off',.3),('Place',.8),('On',.3),('PreparePlantPot',1),('NoFly',.2),('Place',.8),('Fly',.2),('PreparePlantPot',1),('Place',.04),('AimMove',.035),('SelectSoil',.01),('UseSoil',.17),('EndUse',.6),('PreparePlantPot',1),('Burst',.8),('PreparePlantPot',1),('Low',.2),('Place',.8),('High',.2),('PreparePlantPot',1),('NoAnim',.2),('Place',.8),('Anim',.2),('PreparePlantPot',1),('BudgetOne',.2),('Burst',.8),('BudgetDefault',.2),('PrepareWave',1),('Wave',.8),('Save',.2)]
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def snap(ws):return [dict(world=w.get_path_name(),**json.loads(V.snapshot(w))) for w in ws]
def transform_close(a,b):
    va=[float(x) for x in a.replace('|',',').split(',')];vb=[float(x) for x in b.replace('|',',').split(',')]
    return len(va)==len(vb) and all(abs(x-y)<(1.1 if i<6 else .001) for i,(x,y) in enumerate(zip(va,vb)))

def check(name,ok):
    result['checks'].append(dict(name=name,passed=bool(ok)))
    if not ok:result['errors'].append(name)
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'validation_placement.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index,before,saved
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>100:raise RuntimeError('Timeout '+phase)
        if phase=='stopped' and not ws and elapsed>3:
            s.set_editor_property('PlayNumberOfClients',3);levels.editor_request_begin_play();phase='return';start=time.monotonic();return
        if not ws:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and len(ws)==4 and elapsed>8:
            J.step(server,'Quiesce')
            for w in ws:
                unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(k in a.get_class().get_name() for k in ['Visitor','IllegalCustomer','IllegalTradeManager']):a.destroy_actor()
            unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0));phase='act';return
        if phase=='act':
            action,delay=jobs[index];before=snap(ws)
            if action=='PrepareWave':check(action,V.step(server,action))
            elif action.startswith('Prepare'):check(action,V.prepare(server,action[7:]))
            elif action in ['Low','High']:
                unreal.SystemLibrary.execute_console_command(server,'sg.EffectsQuality '+('0' if action=='Low' else '3'),unreal.GameplayStatics.get_player_controller(server,0))
            elif action in ['NoAnim','Anim']:
                unreal.get_default_object(unreal.load_class(None,'/Script/Botanicus.BotanicusPlacementJuiceSettings')).set_editor_property('NoAnimationItems',['PlantPot'] if action=='NoAnim' else [])
            elif action=='ClientPlace':check(action,V.step(next(w for w in ws if w!=server),'Place'))
            elif action=='Burst':check('Burst first',V.step(server,'Place'));check('Burst next',V.step(server,'PlaceNext'))
            elif action=='Save':
                saved=before;check('Save native isolated world',unreal.GameplayStatics.get_game_mode(server).botanicus_save_now())
            else:check('Native '+action,V.step(server,action))
            phase='wait';start=time.monotonic();return
        if phase=='wait':
            action,delay=jobs[index];rr=snap(ws)
            if action in ['Place','ClientPlace','LargeConfirm','Move','Burst','Wave','Destroy','UseSoil','EndUse']:
                result['animation_samples'].append(dict(job=index,action=action,elapsed=elapsed,peers=rr))
            if elapsed<delay:return
            result['samples'].append(dict(action=action,before=before,after=rr))
            if action=='Preview':check('Resolver valid blue hologram',rr[0]['preview'] and rr[0]['valid'] and rr[0].get('blue',0)>rr[0].get('red',1) and rr[0].get('previewNaniteFallback') and 'M_PlacementHologram' in rr[0].get('previewParent',''))
            if action=='BlockPreview':check('Resolver invalid red hologram',rr[0]['preview'] and not rr[0]['valid'] and rr[0].get('red',0)>rr[0].get('blue',1))
            if action=='Unblock':check('Blue returns without preview reconstruction',rr[0].get('previewName')==before[0].get('previewName') and rr[0]['valid'] and rr[0].get('blue',0)>rr[0].get('red',1))
            if action=='Rotate':check('Preview rotates using native yaw',rr[0].get('previewYaw')!=before[0].get('previewYaw'))
            if action=='Cancel':check('Cancel leaves no hologram',all(r['holograms']==0 and not r['preview'] for r in rr))
            if action=='Reject':check('Server refusal creates nothing and consumes nothing',all(r['started']==b['started'] and len(r['items'])==len(b['items']) for r,b in zip(rr,before)) and rr[0]['quantity']==before[0]['quantity'])
            if action in ['Place','ClientPlace','LargeConfirm','Move'] and delay>.5:
                enabled=action!='Place' or not any(row['action'] in ['Off','Low','NoAnim'] for row in result['samples'][-2:])
                if enabled:check(action+' seen once by each player',all(r['started']==b['started']+1 for r,b in zip(rr,before)))
                if not enabled:check(action+' quality or policy fallback creates no POP',all(r['started']==b['started'] for r,b in zip(rr,before)))
                if enabled:check(action+' audio once at contact',all((r['potSounds']+r['equipmentSounds'])==(b['potSounds']+b['equipmentSounds'])+1 for r,b in zip(rr,before)))
                check(action+' leaves no copies',all(r['active']==0 and r['copies']==0 and r['created']==r['destroyed'] for r in rr))
                check(action+' gameplay actors remain replicated, visible and collidable',all(all(p['actorCollision'] and p['replicated'] and not p['hidden'] and p.get('mainPass',True) for p in r['items']) for r in rr))
            if action=='EndUse':check('Native soil interaction succeeds while POP is running',any(p.get('soil',0)>0 for p in rr[0]['items']))
            if action=='Wave':
                check('Four simultaneous server successes reach every player',all(r['started']==b['started']+4 for r,b in zip(rr,before)))
                check('Simultaneous copies are all destroyed',all(r['active']==0 and r['created']==r['destroyed'] for r in rr))
            if action=='Off':check('Disable restores classic placement',all(r['active']==0 and r['holograms']==0 for r in rr))
            if action=='Destroy':check('Destroyed source leaves no temporary copy',all(r['active']==0 and r['copies']==0 for r in rr))
            if action=='Burst':
                check('Two rapid placements have bounded components',all(r['active']==0 and r['copies']==0 for r in rr))
                if result['samples'][-2]['action']=='BudgetOne':check('Global one-animation budget is respected',all(r['started']==b['started']+1 for r,b in zip(rr,before)))
            index+=1
            if index==len(jobs):levels.editor_request_end_play();phase='stopped';start=time.monotonic();return
            phase='act';return
        if phase=='return' and len(ws)==3 and elapsed>10:
            rr=snap(ws);result['samples'].append(dict(action='Return',after=rr))
            check('Restore does not replay POP',all(r['started']==0 and r['copies']==0 for r in rr))
            check('Restore keeps saved transforms',all(all(any(transform_close(p['transform'],q['transform']) for q in r['items']) for p in saved[0]['items']) for r in rr))
            check('Request real fourth late join',J.step(server,'LateJoin'));phase='join';start=time.monotonic();return
        if phase=='join' and len(ws)==4 and elapsed>10:
            rr=snap(ws);result['samples'].append(dict(action='LateJoin',after=rr))
            check('Late join sees final objects without animation',all(r['started']==0 and r['copies']==0 for r in rr))
            active=[r for frame in result['animation_samples'] for r in frame['peers'] if r['active']>0]
            check('Copies never replicate or collide and gameplay roots never animate',bool(active) and all(r['copiesSafe'] and r['rootsStable'] for r in active))
            check('Observed fast scale overshoot',any(r['scale']>1.035 for r in active))
            check('Observed positive bounce height',any(r['height']>.3 for r in active))
            check('No global actor hiding or collision removal during animation',bool(active) and all(all(not p['hidden'] and p['actorCollision'] for p in r['items']) for r in active))
            finish()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
