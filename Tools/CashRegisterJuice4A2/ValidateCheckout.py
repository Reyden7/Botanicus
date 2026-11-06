import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
V=unreal.BotanicusCashRegisterValidation;J=unreal.BotanicusJuiceValidation
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);L.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
S=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:S.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
result={'checks':[],'samples':[],'motion':[],'errors':[],'protocol':'Real four PIE worlds, native player InputKey injection for E/left click, native server RPCs for invalid/repeated/concurrent requests. Fixture prepares customer/display and changes only runtime catalogue price. No physical mouse claim.'}
jobs=[]
def job(label,action,who=0,delay=.35,**extra):jobs.append(dict(label=label,action=action,who=who,delay=delay,**extra))
for n,price in enumerate([30,50,100,57,10007]):
    owner=n%2
    job('prepare','Prepare',price=price,delay=1)
    job('no_tool_aim','AimCustomer',delay=.45);job('no_tool_refused','Scan')
    job('take_aim','AimScanner',who=owner,delay=.5);job('take','KeyE',who=owner,delay=.6)
    job('bad_aim','AimWrong',who=owner,delay=.4);job('bad_target_refused','Scan',who=owner)
    job('scan_aim','AimCustomer',who=owner,delay=.5);job('wrong_plant_refused','WrongPlant',who=owner)
    job('scan','Click',who=owner,delay=.06);job('duplicate_scan','Scan',who=owner,delay=.8)
    job('customer_destroy','DestroyCustomer',delay=.5)
    job('coins_aim','AimCoins',who='all',delay=.6);job('remember','Remember',delay=.01)
    job('collect','ConcurrentCollect',who='all',delay=.06);job('drain','Wait',delay=3.6)
    job('replay','Replay',who='all',delay=.3)
    job('dock_aim','AimDock',who=owner,delay=.45);job('return','KeyE',who=owner,delay=.5)
job('prepare_pawn','Prepare',price=30,delay=1)
job('pawn_aim','AimScanner',who=1,delay=.5);job('pawn_take','KeyE',who=1,delay=.5)
job('destroy_pawn','DestroyCarrier',delay=.8);job('respawn','Respawn',delay=1)
job('prepare_no_fx','Prepare',price=50,delay=1);job('off','Off')
job('off_take_aim','AimScanner',delay=.5);job('off_take','KeyE',delay=.5);job('off_scan_aim','AimCustomer',delay=.5);job('off_scan','Click',delay=.8)
job('off_coins_aim','AimCoins',delay=.5);job('off_collect','Click',delay=2);job('on','On')
job('prepare_auto','Prepare',price=50,delay=1);job('auto_checkout','Auto',delay=4)
job('prepare_save','Prepare',price=57,delay=1);job('save_take_aim','AimScanner',delay=.5);job('save_take','KeyE',delay=.5)
job('save_scan_aim','AimCustomer',delay=.5);job('save_scan','Click',delay=.8);job('save_pending','Save',delay=.2)
job('restart_pending','Restart',delay=0)
index=0;phase='start';start=time.monotonic();baseline=None;before=None;pending=None;spent=None
L.editor_request_begin_play()
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def snapshots(ws):return [json.loads(V.snapshot(w)) for w in ws]
def check(label,ok):
    result['checks'].append(dict(name=label,passed=bool(ok)))
    if not ok:result['errors'].append(label)
def finish():
    result['passed']=not result['errors'];(OUT/'validation_checkout.json').write_text(json.dumps(result,indent=2),encoding='utf-8');L.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def quiesce(ws):
    for w in ws:
        J.step(w,'Quiesce')
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
            if any(x in a.get_class().get_name() for x in ['VisitorManager','IllegalTradeManager']):a.set_actor_tick_enabled(False)
def tick(dt):
    global index,phase,start,baseline,before,pending,spent
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>100:raise RuntimeError('Timeout '+phase)
        if phase=='stopped_pending' and not ws and elapsed>3:
            S.set_editor_property('PlayNumberOfClients',3);L.editor_request_begin_play();phase='restoring_pending';start=time.monotonic();return
        if phase=='stopped_spent' and not ws and elapsed>3:
            S.set_editor_property('PlayNumberOfClients',4);L.editor_request_begin_play();phase='restoring_spent';start=time.monotonic();return
        if not ws:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and len(ws)==4 and elapsed>9:quiesce(ws);phase='act'
        if phase=='restoring_pending' and len(ws)==3 and elapsed>10:
            quiesce(ws);rr=snapshots(ws);result['samples'].append(dict(action='restored_pending',peers=rr))
            check('Uncollected receipt survives PIE teardown and native restore',all(r['phase']==2 and not r['credited'] and r['transaction']==pending['transaction'] and r['amount']==pending['amount'] and r['funds']==pending['funds'] for r in rr))
            check('Restore replays no scan beam or opening audio',all(r['scanVisuals']==0 and r['ScannerBeep']==0 and r['RegisterOpen']==0 for r in rr))
            check('Request fourth real late join',J.step(server,'LateJoin'));phase='late';start=time.monotonic();return
        if phase=='late' and len(ws)==4 and elapsed>10:
            rr=snapshots(ws);result['samples'].append(dict(action='late_join',peers=rr));check('Fourth late join sees persistent exact payment without scan replay',all(r['phase']==2 and r['amount']==pending['amount'] and r['scanVisuals']==0 for r in rr))
            # Native fixture Prepare is intentionally not used after restore.
            check('Position by restored cash',V.step(server,'Position'));phase='aim_restored';start=time.monotonic();return
        if phase=='aim_restored' and elapsed>.5:
            check('Aim restored coins',V.aim(server,'Coins'));phase='collect_restored';start=time.monotonic();return
        if phase=='collect_restored' and elapsed>.6:
            check('Collect restored receipt through native input',V.step(server,'Click'));phase='save_spent';start=time.monotonic();return
        if phase=='save_spent' and elapsed>.1:
            rr=snapshots(ws);spent=rr[0];result['samples'].append(dict(action='credited_before_animation_end',peers=rr));check('Immediate restored credit exactly once',spent['credited'] and spent['funds']==pending['funds']+pending['amount'])
            check('Save during collection',V.step(server,'Save'));L.editor_request_end_play();phase='stopped_spent';start=time.monotonic();return
        if phase=='restoring_spent' and len(ws)==4 and elapsed>10:
            rr=snapshots(ws);result['samples'].append(dict(action='restored_spent',peers=rr));check('Credited receipt restores closed without another credit or motion',all(r['phase']==0 and r['funds']==spent['funds'] and r['credited'] and r['arrivals']==0 for r in rr))
            # Night changes native shop/day UI and trends. Validate its separate
            # transaction path after finishing all public cash restore checks.
            job('illegal_prepare','PrepareIllegal',delay=1);job('illegal_aim','AimIllegal',delay=.5)
            job('illegal_sale','IllegalSell',delay=.2);job('illegal_repeat','IllegalSell',delay=.3)
            index=len(jobs)-4;phase='act';start=time.monotonic();return
        if phase=='act':
            j=jobs[index];before=snapshots(ws);targets=ws if j['who']=='all' else [ws[j['who']]];action=j['action']
            if action=='Prepare':check('Prepare '+str(j['price']),V.prepare(server,j['price']))
            elif action=='Restart':pending=before[0];L.editor_request_end_play();phase='stopped_pending';start=time.monotonic();return
            elif action=='ConcurrentCollect':
                for w in targets:
                    for unused in range(3):V.step(w,'Collect')
            elif action.startswith('Aim'):
                for w in targets:check(j['label']+' issued',V.aim(w,action[3:]))
            elif action!='Wait':
                for w in targets:check(j['label']+' issued',V.step(w,action))
            phase='wait';start=time.monotonic();return
        if phase=='wait':
            j=jobs[index];rr=snapshots(ws);r=rr[0];label=j['label'];b=before[0]
            if label in ['scan','collect','drain','off_scan','off_collect']:result['motion'].append(dict(action=label,elapsed=elapsed,peers=rr))
            if elapsed<j['delay']:return
            result['samples'].append(dict(action=label,before=before,peers=rr))
            if j['action']=='Prepare':baseline=r;check(label+' native price',r.get('nativePrice',0)>0 if label=='prepare_save' else r.get('nativePrice')==j['price'])
            if label in ['no_tool_refused','bad_target_refused','wrong_plant_refused']:check(label+' no success/credit',all(x['phase']==0 and x['funds']==baseline['funds'] and x['scanVisuals']==z['scanVisuals'] for x,z in zip(rr,before)))
            if label in ['take','pawn_take']:check(label+' one replicated carrier',len(set(x['carrierPlayerId'] for x in rr))==1 and all(x['carrier'] for x in rr) and sum(x['owned'] for x in rr)==1)
            if label=='scan':check('Accepted scan freezes exact native amount and IDs',r['phase'] in [1,2] and r['amount']==baseline['nativePrice'] and r['transaction']!='0'*32 and r['plantId']!='0'*32 and r['customerId']!='0'*32 and r['funds']==baseline['funds'])
            if label=='duplicate_scan':
                check('One accepted scan only',r['scansAccepted']==baseline['scansAccepted']+1 and all(x['scanVisuals']==baseline['scanVisuals']+1 for x in rr))
                check('One beep and opening per peer',all(x['ScannerBeep']==baseline['ScannerBeep']+1 and x['RegisterOpen']==baseline['RegisterOpen']+1 for x in rr))
                check('One native review per buyer',all(x['reviews']==baseline['reviews']+1 for x in rr))
                check('Exact coin and explicit voucher representation',all(x['gold']*10+x['voucher']==r['amount'] and x['gold']==min(r['amount']//10,96) and x['renderedGold']==x['gold'] for x in rr))
            if label=='customer_destroy':check('Payment outlives customer destruction',all(x['phase']==2 and x['funds']==baseline['funds'] for x in rr))
            if label=='collect':check('Concurrent and repeated requests credit atomically once',r['credited'] and r['phase']==3 and r['funds']==baseline['funds']+r['amount'] and r['collectionsAccepted']==baseline['collectionsAccepted']+1 and r['sales']==baseline['sales']+1 and r['revenue']==baseline['revenue']+r['amount'])
            if label=='drain':check('All peers converge, all pieces arrive, drawer closes automatically',all(x['phase']==0 and x['funds']==baseline['funds']+x['amount'] and x['renderedGold']==0 and x['duration']<=2.61 for x in rr))
            if label=='replay':check('Spent transaction replay cannot credit',all(x['funds']==z['funds'] and x['sales']==z['sales'] for x,z in zip(rr,before)))
            if label=='return':check('Same scanner reused and returned on every peer',all(not x['carrier'] and x['scanner']==z['scanner'] for x,z in zip(rr,before)))
            if label=='destroy_pawn':check('Destroyed carrier returns same scanner safely',all(not x['carrier'] and x['scanner']==z['scanner'] for x,z in zip(rr,before)))
            if label=='off_collect':check('No-animation fallback still credits and closes',all(x['phase']==0 and x['funds']==baseline['funds']+50 for x in rr))
            if label=='auto_checkout':check('Native self checkout still completes sale and leaves manual receipt idle',all(x['phase']==0 and x['funds']==baseline['funds']+50 and x['sales']==baseline['sales']+1 for x in rr))
            if label=='illegal_sale':
                check('Native clandestine RPC credits original order value',r['funds']==b['funds']+b.get('illegalReward',-1) and r['illegalQuantity']==0)
                check('Clandestine sale keeps public receipt and public sales counters unchanged',all(x['phase']==z['phase'] and x['transaction']==z['transaction'] and x['sales']==z['sales'] and x['revenue']==z['revenue'] for x,z in zip(rr,before)))
                expected=min(100,b['suspicion']+unreal.get_default_object(unreal.load_class(None,'/Script/Botanicus.BotanicusIllegalTradeSettings')).get_editor_property('SuspicionPerSale'))
                check('Native clandestine suspicion preserved',r['suspicion']==expected)
            if label=='illegal_repeat':check('Clandestine repeated request pays only once',all(x['funds']==z['funds'] for x,z in zip(rr,before)))
            (OUT/'validation_progress.json').write_text(json.dumps(dict(last=label,errors=result['errors'],sample=result['samples'][-1]),indent=2),encoding='utf-8')
            index+=1
            if index==len(jobs):finish();return
            phase='act'
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
