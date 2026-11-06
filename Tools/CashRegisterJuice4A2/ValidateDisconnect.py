import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;V=unreal.BotanicusCashRegisterValidation;J=unreal.BotanicusJuiceValidation
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);L.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
S=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:S.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
result={'checks':[],'errors':[],'samples':[],'protocol':'Real server-side NetConnection::Close on the remote scanner carrier; check PIE world count decreases 4->3, then request real late join. Repeat disconnect during accepted coin collection.'}
phase='start';start=time.monotonic();index=0;cycle=0;baseline=None
jobs=[('Prepare',1),('AimScanner',.5),('KeyE',.6),('TakeOther',.4),('DisconnectCarrier',.2)]
collect_jobs=[('Prepare',1),('AimScanner',.5),('KeyE',.6),('AimCustomer',.5),('Click',.8),('AimCoins',.5),('Click',.1),('DisconnectCarrier',.2)]
L.editor_request_begin_play()
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w) and not unreal.SystemLibrary.is_standalone(w)],key=lambda w:w.get_path_name())
def snap(ws):return [json.loads(V.snapshot(w)) for w in ws]
def check(name,ok):
    result['checks'].append(dict(name=name,passed=bool(ok)))
    if not ok:result['errors'].append(name)
def finish():
    result['passed']=not result['errors'];(OUT/'validation_disconnect.json').write_text(json.dumps(result,indent=2),encoding='utf-8');L.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index,cycle,baseline,jobs
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>90:raise RuntimeError('Timeout '+phase)
        if not ws:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and len(ws)==4 and elapsed>10:
            for w in ws:
                J.step(w,'Quiesce')
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(x in a.get_class().get_name() for x in ['VisitorManager','IllegalTradeManager']):a.set_actor_tick_enabled(False)
            phase='act'
        if phase=='act':
            action,delay=jobs[index]
            if action=='Prepare':V.prepare(server,100 if cycle else 30)
            elif action=='DisconnectCarrier':V.step(server,action)
            elif action=='TakeOther':V.step(ws[2],'Take');V.step(ws[3],'Take')
            elif action.startswith('Aim'):V.aim(ws[1],action[3:])
            else:V.step(ws[1],action)
            phase='wait';start=time.monotonic();return
        if phase=='wait' and elapsed>=jobs[index][1]:
            action=jobs[index][0];rr=snap(ws);result['samples'].append(dict(cycle=cycle,action=action,peers=rr))
            if action=='Prepare':baseline=rr[0]
            if action=='KeyE':check('Remote carrier owns unique scanner '+str(cycle),sum(r['owned'] for r in rr)==1 and rr[1]['owned'])
            if action=='TakeOther':check('Other players cannot steal carried scanner',rr[1]['owned'] and sum(r['owned'] for r in rr)==1)
            if action=='DisconnectCarrier':phase='disconnected';start=time.monotonic();return
            index+=1;phase='act';return
        if phase=='disconnected' and len(ws)==3 and elapsed>1.5:
            rr=snap(ws);result['samples'].append(dict(cycle=cycle,action='three_worlds_after_close',peers=rr))
            check('Actual client connection and world removed '+str(cycle),len(ws)==3)
            check('Carrier disconnect returns scanner on surviving peers '+str(cycle),all(not r['carrier'] for r in rr))
            if cycle:check('Disconnect during collection preserves one exact credit and closes',all(r['phase']==0 and r['credited'] and r['funds']==baseline['funds']+100 and r['sales']==baseline['sales']+1 for r in rr))
            check('Late join requested '+str(cycle),J.step(server,'LateJoin'));phase='join';start=time.monotonic();return
        if phase=='join' and len(ws)==4 and elapsed>9:
            rr=snap(ws);result['samples'].append(dict(cycle=cycle,action='late_join',peers=rr));check('Late join sees docked shared scanner '+str(cycle),all(not r['carrier'] for r in rr))
            if cycle:
                check('Late join after spent transaction cannot create another credit',all(r['phase']==0 and r['funds']==baseline['funds']+100 and r['scanVisuals']==0 for r in rr[-1:]));finish();return
            cycle=1;index=0;jobs=collect_jobs;phase='act'
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
