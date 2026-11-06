import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
result={'checks':[],'samples':[],'errors':[],'protocol':'Real native inventory/RPCs and four rendered PIE worlds; isolated saves; optimized presentation.'}
V=unreal.BotanicusEquipmentValidation;J=unreal.BotanicusJuiceValidation
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',640),('ClientWindowHeight',360)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();job=0;before=[];saved=[];last_refill=0
jobs=[('SetupAll',2),('UnrelatedAll',2),('ConsumeAll',2),('RechargeAll',2),('Select2',2),('Select0',2),('InputOn',.3),('Continuous',4),('InputOff',2),('RechargeAll',2),('InputOn',.3),('Continuous',2),('InputOff',2),('EmptyAll',2),('InputOn',.3),('EmptySpray',2),('InputOff',2),('Reserve',2),('RefillHost',1.2),('RefillClient',1.2),('RemoveReserve',1),('Select3',2),('QuantityAll',2)]
for i in range(8):jobs.append(('Select'+str(i%2),.35))
jobs += [('Select1',2),('ChangeDefinition',2),('RestoreDefinition',2),('ChangeRenderedMesh',2),('DestroyPreview',2),('Select4',2),('PotStateAll',2),('Select0',2),('FillAll',2),('PickupFull',2),('SetupAll',2),('Pickup',2),('DropSelected',2),('PhysicalPickup',2),('PhysicalDrop',2),('SetupAll',2),('SaveReturn',0)]
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def snapshot(ws):return [dict(world=w.get_path_name(),state=json.loads(V.snapshot(w)),counts=dict(V.read_counters(w))) for w in ws]
def check(name,ok):
    result['checks'].append(dict(name=name,passed=bool(ok)))
    if not ok:result['errors'].append(name)
def same_actors(a,b):return all(x['state'].get('equipped')==y['state'].get('equipped') for x,y in zip(a,b))
def counter_delta(a,b,key):return [x['counts'].get(key,0)-y['counts'].get(key,0) for x,y in zip(a,b)]
def finish():
    result['passed']=not result['errors']
    for w in worlds():V.finish(w)
    levels.editor_request_end_play();(OUT/'validation_equipment.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,job,before,saved,last_refill
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>120:raise RuntimeError('Timeout '+phase)
        if phase=='start':
            if len(ws)==4 and elapsed>8:
                server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w));check('Native setup tracing on four worlds',all(V.begin(w) for w in ws));V.set_legacy(server,False);J.step(server,'Quiesce')
                host=unreal.GameplayStatics.get_player_pawn(server,0);base=host.get_actor_location()
                for i,pawn in enumerate(unreal.GameplayStatics.get_all_actors_of_class(server,unreal.BotanicusCharacter)):
                    if pawn!=host:pawn.set_actor_location(base-host.get_actor_forward_vector()*50+host.get_actor_right_vector()*(35*(i+1)),False,True)
                unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0));phase='act'
            return
        if phase=='restart_stop':
            if not ws and elapsed>3:levels.editor_request_begin_play();phase='restart';start=time.monotonic()
            return
        if not ws:
            if phase=='disconnect':
                result['disconnect_note']='PIE ended as a whole after native connection close; independent client teardown not isolated.';finish()
            return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='restart':
            if len(ws)==4 and elapsed>10:
                for w in ws:V.begin(w)
                J.step(server,'Quiesce');after=snapshot(ws);result['samples'].append(dict(action='Return',before=saved,after=after))
                check('Save selected instance, key, quantity and reservoir restored on four owners',all(all(a['state'].get(k)==b['state'].get(k) for k in ['slot_item','instance','slot','quantity']) and abs(a['state'].get('water',-1)-b['state'].get('water',-2))<.00001 for a,b in zip(after,saved)))
                check('Equipped actor rebuilt once from restored state, valid locally',all(a['state'].get('item')=='WateringCan' and abs(a['state']['preview_water']-a['state']['water'])<.00001 for a in after))
                before=after
                for w in ws:V.step(w,'InputOn')
                phase='before_destroy';start=time.monotonic()
            return
        if phase=='before_destroy' and elapsed>2:
            check('Native remote Pawn destruction',V.step(server,'DestroyRemotePawn'));phase='destroyed';start=time.monotonic();return
        if phase=='destroyed' and elapsed>3:
            after=snapshot(ws);result['samples'].append(dict(action='DestroyedPawn',before=before,after=after))
            lost=[r for r in after if not r['state'].get('pawn')]
            check('Owner representation removed after Pawn destruction',len(lost)==1 and not lost[0]['state'].get('equipped') and lost[0]['state']['local_cans']==0)
            check('Other player representations remain valid',all(r['state'].get('item')=='WateringCan' for r in after if r['state'].get('pawn')))
            check('No orphan fourth water source',all(r['state'].get('water_sources',0)<=3 for r in after))
            for w in ws:V.step(w,'InputOff')
            check('Native player respawn',V.step(server,'Respawn'));phase='respawn';start=time.monotonic();return
        if phase=='respawn' and elapsed>3:
            check('Equip after new Pawn / QuickBar source',V.step(server,'SetupAll'));phase='resourced';start=time.monotonic();return
        if phase=='resourced' and elapsed>3:
            after=snapshot(ws);result['samples'].append(dict(action='RespawnSource',after=after))
            check('All four sources rebind to the new correct local representation',len(after)==4 and all(r['state'].get('item')=='WateringCan' and r['state'].get('local_cans')==1 for r in after))
            check('Native remote connection close',V.step(server,'Disconnect'));phase='disconnect';start=time.monotonic();return
        if phase=='disconnect' and elapsed>6:
            after=snapshot(ws);result['samples'].append(dict(action='Disconnect',after=after));check('Server observes disconnected controller removal',json.loads(V.snapshot(server)).get('players')==3);finish();return
        if phase not in ['act','wait']:return
        action,delay=jobs[job]
        if phase=='act':
            before=snapshot(ws)
            if action.startswith('Select'):check(action+' native owner selection',all(V.select(w,int(action[6:])) for w in ws))
            elif action in ['InputOn','InputOff','ChangeRenderedMesh','DestroyPreview']:check(action+' native local action',all(V.step(w,action) for w in ws))
            elif action=='SaveReturn':
                saved=before;check('Native isolated save',unreal.GameplayStatics.get_game_mode(server).botanicus_save_now())
                for w in ws:V.finish(w)
                levels.editor_request_end_play();phase='restart_stop';start=time.monotonic();return
            elif action not in ['Continuous','EmptySpray','RefillHost','RefillClient']:check(action+' native action',V.step(server,action))
            if action=='Reserve':check('Refill fixture stays within native range without Pawn occlusion',unreal.BotanicusPlacementValidation.step(server,'PrepareRefillRange'))
            phase='wait';start=time.monotonic();last_refill=0
        else:
            if action in ['RefillHost','RefillClient'] and time.monotonic()-last_refill>.15:
                target=server if action=='RefillHost' else next(w for w in ws if w!=server)
                check(action+' real reserve RPC',V.step(target,'RefillPulse'));last_refill=time.monotonic()
            if elapsed<delay:return
            after=snapshot(ws);result['samples'].append(dict(action=action,before=before,after=after))
            if action in ['UnrelatedAll','ConsumeAll','RechargeAll','Select2','QuantityAll','FillAll','EmptyAll'] or (action=='Select0' and all(r['state'].get('item')=='WateringCan' for r in before)):
                check(action+' reuses actor on all owners',same_actors(after,before))
                check(action+' no local actor allocations',counter_delta(after,before,'LocalCreated')==[0]*4 and counter_delta(after,before,'LocalDestroyRequested')==[0]*4)
            if action in ['ConsumeAll','RechargeAll','Select2','Select0','EmptyAll']:
                check(action+' reservoir matches authoritative owner state',all(abs(r['state']['water']-r['state']['preview_water'])<.00001 for r in after))
            if action=='Continuous':
                check('Continuous water has stable owner actor',same_actors(after,before))
                check('Continuous water consumes reservoir on four owners',all(a['state']['water']<b['state']['water'] for a,b in zip(after,before)))
                check('Four sources and audio voices visible to each peer',all(r['state']['water_sources']==4 and r['state']['audio_water']==4 and r['state']['remote_cans']==3 for r in after))
            if action in ['InputOff','EmptySpray']:check(action+' clears water rendering and audio',all(r['state']['water_sources']==0 and r['state']['audio_water']==0 and r['state']['remote_cans']==0 for r in after))
            if action in ['RefillHost','RefillClient']:
                target=server if action=='RefillHost' else next(w for w in ws if w!=server);V.step(target,'RefillEnd');ix=ws.index(target)
                check(action+' refills without replacing representation',after[ix]['state']['water']>before[ix]['state']['water'] and after[ix]['state']['equipped']==before[ix]['state']['equipped'])
            if action.startswith('Select'):
                expected={0:'WateringCan',1:'GardenTrowel',2:'WateringCan',3:'PottingSoil',4:'PlantPot'}[int(action[6:])]
                check(action+' current equipment follows selection without stale tool',all(r['state']['item']==expected and r['state']['slot']==int(action[6:]) for r in after))
            if action in ['ChangeDefinition','RestoreDefinition']:check(action+' rebuilds legitimate changed definition on host',after[ws.index(server)]['state']['equipped']!=before[ws.index(server)]['state']['equipped'])
            if action in ['ChangeRenderedMesh','DestroyPreview']:check(action+' reconstructs invalid representation on all owners',all(a['state']['equipped'] and a['state']['equipped']!=b['state']['equipped'] for a,b in zip(after,before)))
            if action=='PotStateAll':check('Pot state changes preserve visible growth',all(a['state']['equipped']!=b['state']['equipped'] and .7<=a['state'].get('pot_growth',0)<.75 and a['state'].get('pot_plant') not in ['', 'None'] for a,b in zip(after,before)))
            if action=='QuantityAll':check('Remaining stack quantity updated; held presentation still one item',all(r['state']['quantity']==4 for r in after))
            if action=='PickupFull':check('Refused pickup leaves current presentation unchanged',same_actors(after,before))
            if action=='Pickup':check('Actual native pickup selects collected item',after[ws.index(server)]['state']['slot_item']=='GardenTrowel')
            if action=='DropSelected':check('Actual native floor placement consumes the selected item',after[ws.index(server)]['state']['quantity']<before[ws.index(server)]['state']['quantity'])
            if action=='PhysicalPickup':check('Replicated physical can attached and visible in all four views',all(any(c['carrier'] and c['attached']==c['carrier'] and c['visible'] and c['replicated'] for c in r['state']['physical']) for r in after))
            if action=='PhysicalDrop':check('Physical can detaches on all four peers',all(not any(c['carrier'] for c in r['state']['physical']) for r in after))
            check(action+' held preview remains local and collisionless',all(not r['state']['preview_replicated'] and not r['state']['preview_collision'] for r in after))
            job+=1
            if job==len(jobs):finish()
            else:phase='act'
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
