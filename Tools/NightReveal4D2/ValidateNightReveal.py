import unreal,json,time,traceback
from pathlib import Path
assert '/isolated_user/' in unreal.Paths.project_saved_dir().replace('\\','/')
O=Path(unreal.Paths.project_saved_dir())/'NightRevealValidation';O.mkdir(parents=True,exist_ok=True);V=unreal.BotanicusNightValidation
L=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);L.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
S=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for key,value in [('PlayNumberOfClients',2),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('SoloAudioInFirstPIEClient',False),('ClientWindowWidth',960),('ClientWindowHeight',540)]:S.set_editor_property(key,value)
result={'samples':[],'checks':[],'errors':[],'frames':[]};jobs=[]
def job(label,command=None,value=0,delay=.3):jobs.append(dict(label=label,command=command,value=value,delay=delay))
job('position','Position',delay=.7);job('day14','Time',14,5.2);job('closed_player','SweepPlayer');job('position2','Position');job('closed_npc','SweepNPC');job('position3','Position')
job('standby','Time',19.5,3.5);job('opening','Time',21,.25);job('neon_wait',delay=6.8);job('night02','Time',2,.6)
job('open_player','SweepPlayer');job('position4','Position');job('open_npc','SweepNPC');job('position5','Position')
job('midnight_a','Time',23.99,.3);job('midnight_b','Time',.02,.3);job('late_join','LateJoin',delay=7)
job('position6','Position',delay=.5);job('portal','Portal',delay=.5);job('hold_portal','Time',5,2);job('hold_inside','Inside',delay=.6)
job('leave_rear','Position',delay=5.4);job('closed_again','SweepPlayer');job('position7','Position');job('morning','Time',8,3.7)
for label,hour,delay in [('rapid21',21,.5),('rapid05',5,.25),('rapid21b',21,.3),('rapid14',14,.25),('rapid02',2,7)]:job(label,'Time',hour,delay)
job('save_night','Save',delay=.5)
index=0;phase='start';start=time.monotonic();last_sample=0;capture=False;last_frame=0;audio_recording=False;counter=0
captures=Path(unreal.Paths.project_saved_dir())/'captures';captures.mkdir(parents=True,exist_ok=True)
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def snaps(ws):return [json.loads(V.snapshot(w)) for w in ws]
def check(name,ok):
    result['checks'].append(dict(name=name,passed=bool(ok)))
    if not ok:result['errors'].append(name)
def finish():
    result['passed']=not result['errors'];(O/'test_result.json').write_text(json.dumps(result,indent=2),encoding='utf-8');L.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index,last_sample,capture,last_frame,audio_recording,counter
    now=time.monotonic();ws=worlds()
    try:
        if now-start>100:raise RuntimeError('timeout '+phase)
        if phase=='stopped':
            if ws or now-start<3:return
            S.set_editor_property('PlayNumberOfClients',3);L.editor_request_begin_play();phase='restoring';start=now;return
        if not ws:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='restoring':
            if len(ws)!=3 or now-start<7:return
            rr=snaps(ws);result['restored']=rr
            check('Restore derives stable open/night state from saved GameState',all(abs(r['hour']-result['before'][0]['hour'])<.2 and r['trade'] and r['open']>.999 and r['neon']>.99 and not r['blocked'] for r in rr))
            check('Restore replays no completed transition sound',all(r['openAudio']==0 and r['closeAudio']==0 and r['neonAudio']==0 for r in rr))
            finish();return
        if phase=='start':
            if len(ws)!=2 or now-start<6:return
            phase='act'
        if phase=='act':
            j=jobs[index];result['before']=snaps(ws)
            if j['label']=='standby':
                capture=True
                try:unreal.AudioMixerLibrary.start_recording_output(server,20);audio_recording=True
                except Exception as e:result['audio_capture_error']=str(e)
            if j['label']=='night02':
                capture=False
                if audio_recording:
                    unreal.AudioMixerLibrary.stop_recording_output(server,unreal.AudioRecordingExportType.WAV_FILE,'NightRevealAudio',str(captures));audio_recording=False
            if j['command']:check(j['label']+' queued',V.action(server,j['command'],j['value']))
            phase='wait';start=now;return
        if capture and now-last_frame>=.125:
            path=captures/f'NightReveal_{counter:05d}.bgra'
            if unreal.BotanicusCashRegisterValidation.capture(server,str(path)):
                result['frames'].append(dict(path=str(path),time=now));counter+=1
            last_frame=now
        rr=snaps(ws);label=jobs[index]['label']
        if now-last_sample>.075:result['samples'].append(dict(stage=label,elapsed=now-start,peers=rr));last_sample=now
        if now-start<jobs[index]['delay']:return
        (O/'progress.json').write_text(json.dumps(dict(stage=label,peers=rr,errors=result['errors']),indent=2),encoding='utf-8')
        near=lambda a,b,tol=.03:abs(a-b)<tol
        if label=='day14':
            check('14h closed public door and normal interior light',all(r['shop'] and not r['trade'] and r['blocked'] and r['open']<.001 and r['neon']<.001 and min(r['interiorIntensities'])>650 and r.get('hud')=='BOUTIQUE OUVERTE' for r in rr))
        if label in ['closed_player','closed_again']:check('Authoritative player sweep blocked '+label,rr[0]['playerSweepBlocked'])
        if label=='closed_npc':
            check('NPC capsule sweep blocked',rr[0]['npcSweepBlocked']);check('Dynamic navigation blocks the direct portal ray',rr[0]['navProjected'] and rr[0]['navRayBlocked'])
        if label=='standby':check('19h30 warm standby, closed portal and dark HUD',all(not r['shop'] and not r['trade'] and r['blocked'] and max(abs(v-200) for v in r['interiorIntensities'])<3 and r.get('hud')=='BOUTIQUE FERMÃƒâ€°E' for r in rr))
        if label=='opening':check('21h starts an actual sectional motion before neon',all(r['trade'] and r['motionDuration']>0 and 0<r['open']<.3 and r['neon']<.001 for r in rr))
        if label=='neon_wait':
            check('Sections retract horizontally and neon stabilizes after opening',all(r['open']>.999 and not r['blocked'] and r['neon']>.99 and abs(r['firstPanelPitch']+90)<.1 and r['lastPanelX']>300 for r in rr))
            check('One opening and one delayed electrical sound per peer',all(r['openAudio']==1 and r['neonAudio']==1 for r in rr))
        if label=='night02':check('02h clandestine HUD and lavender accent',all(r.get('hud')=='BOUTIQUE CLANDESTINE OUVERTE' and r['neon']>.99 and r['accentIntensity']>300 for r in rr))
        if label=='open_player':check('Player traverses open passage',not rr[0]['playerSweepBlocked'])
        if label=='open_npc':
            check('NPC traverses open passage',not rr[0]['npcSweepBlocked']);check('Navigation permits direct open portal ray',rr[0]['navProjected'] and not rr[0]['navRayBlocked'])
        if label=='midnight_b':check('Midnight does not replay opening or ignition',all(r['openAudio']==b['openAudio'] and r['neonAudio']==b['neonAudio'] and r['open']>.99 for r,b in zip(rr,result['before'])))
        if label=='late_join':
            check('Actual third late join sees current phase',len(ws)==3 and all(r['open']>.99 and r['neon']>.99 and not r['blocked'] and r.get('hud')=='BOUTIQUE CLANDESTINE OUVERTE' for r in rr))
            check('Late join does not replay completed sounds',len(rr)==3 and rr[-1]['openAudio']==0 and rr[-1]['neonAudio']==0)
        if label in ['hold_portal','hold_inside']:check(label+' extinguishes neon but preserves safe exit',all(r['safetyHold'] and r['open']>.99 and not r['blocked'] and r['neon']<.01 for r in rr))
        if label=='leave_rear':check('05h closes only after rear and portal are clear',all(r['open']<.001 and r['blocked'] and not r['safetyHold'] and r['neon']<.001 for r in rr))
        if label=='morning':check('08h restores public light and green status',all(r['shop'] and r['publicWeight']>.99 and min(r['interiorIntensities'])>650 and r['blocked'] and r.get('hud')=='BOUTIQUE OUVERTE' for r in rr))
        if label=='rapid02':check('Rapid time changes converge to one consistent night state',all(r['open']>.999 and not r['blocked'] and r['neon']>.99 and r['trade'] and not r['shop'] for r in rr))
        index+=1
        if index==len(jobs):L.editor_request_end_play();phase='stopped';start=now;return
        phase='act'
    except Exception:result['errors'].append(traceback.format_exc());finish()
L.editor_request_begin_play();handle=unreal.register_slate_post_tick_callback(tick)
