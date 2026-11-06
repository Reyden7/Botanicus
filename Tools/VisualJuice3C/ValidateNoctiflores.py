import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;(OUT/'captures').mkdir(exist_ok=True)
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
V=unreal.BotanicusNoctifloreValidation;J=unreal.BotanicusJuiceValidation
settings=unreal.get_default_object(unreal.load_class(None,'/Script/Botanicus.BotanicusNoctifloreSettings'))
collection=unreal.load_asset('/Game/Botanicus/Materials/Juice/Noctiflore/MPC_Noctiflore')
result={'checks':[],'errors':[],'samples':[],'peakParticles':0,'peakComponents':0}
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',3),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',960),('ClientWindowHeight',540)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();index=0;old_worlds=[];saved=[];before=[];last_probe=0
jobs=[('Setup',4),('Day',4),('Young',2),('Developed',2),('Large',2),('Mature',2),('Ordinary',2),('Night22',5),('Night02',4),('TradeOff',4),('Night22Again',4),('Off',2),('On',3),('NoLumen',3),('Lumen',3),('Low',2),('High',3),('StressBudget',7),('DefaultBudget',4),('Clear',2),('Setup',4),('Destroy',2),('Setup',4),('Mature',2),('Save',1),('LateJoin',0)]
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def rows(ws):
    return [dict(world=w.get_path_name(),**json.loads(V.snapshot(w)),glow=unreal.MaterialLibrary.get_scalar_parameter_value(w,collection,'NoctGlowIntensity')) for w in ws]
def check(name,ok):
    result['checks'].append(dict(name=name,passed=bool(ok)))
    if not ok:result['errors'].append(name)
def freeze(ws):
    for w in ws:
        unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
        for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
            if any(t in a.get_class().get_name() for t in ['Visitor','IllegalCustomer','IllegalTradeManager']):a.destroy_actor()
def time_at(server,h):unreal.SystemLibrary.execute_console_command(server,'SetTime '+str(h),unreal.GameplayStatics.get_player_controller(server,0))
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'validation_noctiflores.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index,before,saved,old_worlds,last_probe
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>120:raise RuntimeError('Timeout '+phase)
        if phase=='stopped' and not ws and elapsed>3:
            s.set_editor_property('PlayNumberOfClients',4);levels.editor_request_begin_play();phase='return';start=time.monotonic();return
        if not ws:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and len(ws)==3 and elapsed>8:
            J.step(server,'Quiesce');freeze(ws);phase='act';return
        if phase=='act':
            action,delay=jobs[index];before=rows(ws)
            if action=='Day':time_at(server,12)
            elif action=='Night22':time_at(server,22)
            elif action=='Night02':time_at(server,2)
            elif action=='TradeOff':time_at(server,5.1)
            elif action=='Night22Again':time_at(server,22)
            elif action in ['Low','High']:
                for group in ['Effects','GlobalIllumination','Reflection','Shading','Shadow','PostProcess']:
                    unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality '+('0' if action=='Low' else '3'),unreal.GameplayStatics.get_player_controller(server,0))
            elif action in ['NoLumen','Lumen']:
                for group in ['GlobalIllumination','Reflection']:
                    unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality '+('0' if action=='NoLumen' else '3'),unreal.GameplayStatics.get_player_controller(server,0))
            elif action=='Ordinary':
                check('Create ordinary control',J.step(server,'SetupSimple'));check('Ordinary restore',J.restore_growth(server,.7))
            elif action=='Save':
                saved=rows(ws);check('Save isolated native world',unreal.GameplayStatics.get_game_mode(server).botanicus_save_now())
            elif action=='LateJoin':
                old_worlds=[w.get_path_name() for w in ws];check('Request real late join',J.step(server,'LateJoin'));phase='join';start=time.monotonic();return
            else:check('Native fixture '+action,V.step(server,action))
            phase='wait';start=time.monotonic();return
        if phase=='wait':
            action,delay=jobs[index]
            if time.monotonic()-last_probe>.4:
                for w in ws:
                    juice=J.get_juice(w);result['peakParticles']=max(result['peakParticles'],juice.get_spore_particle_count());result['peakComponents']=max(result['peakComponents'],juice.get_spore_component_count())
                    if juice.get_spore_reserved_particles()>12 or juice.get_spore_component_count()>2:raise RuntimeError('Spore budget exceeded')
                last_probe=time.monotonic()
            if elapsed<delay:return
            rr=rows(ws);result['samples'].append(dict(action=action,peers=rr))
            check(action+' / audio no restart',all(r['audioStarts']==2 for r in rr))
            check(action+' / bounded spores',all(r['components']<=2 and r['reserved']<=12 and r['particles']<=12 for r in rr))
            if action in ['Setup','Young','Developed','Large','Mature']:
                check(action+' / 16 Noctiflores each peer',all(len(r['plants'])==16 and r['candidates']==16 for r in rr))
                check(action+' / only cosmetic materials and collision unchanged',all(all('MI_NoctGlow' in p.get('parent','') and p['collision']==0 for p in r['plants']) for r in rr))
            if action in ['Young','Developed','Large','Mature']:
                stage={'Young':'/1/1.1','Developed':'/2/2.2','Large':'/3/3.3','Mature':'/3/3.3'}[action]
                check(action+' / existing mesh stage',all(all(stage in p['mesh'] for p in r['plants']) for r in rr))
            if action=='Day':check('Day / no trade, no spores, original light inactive',all(not r['trade'] and r['components']==0 and all(v<.1 for v in r['lights']) for r in rr))
            if action in ['Night22','Night02']:check(action+' / GameState, lights and glow correct',all(r['trade'] and r['glow']>.6 and all(330<v<370 for v in r['lights']) for r in rr))
            if action in ['Night22','Night02']:check(action+' / existing regional audio mix and edge fade',rr[0]['nightVolume']>.05 and all(.01<r['nightVolume']<.056 for r in rr))
            if action=='TradeOff':check('Existing schedule / light fades out and spores stop',all(not r['trade'] and r['components']==0 and all(v<1 for v in r['lights']) for r in rr))
            if action=='Night22Again':check('Existing schedule / lights return',all(r['trade'] and all(330<v<370 for v in r['lights']) for r in rr))
            if action=='Ordinary':
                for w in ws:
                    target=J.get_target(w)
                    parents=[str(c.get_material(0).get_editor_property('parent').get_path_name()) for c in target.get_components_by_class(unreal.StaticMeshComponent) if 'Foliage' in c.get_name() and c.get_material(0)]
                    check('Ordinary plant uses living materials only / '+w.get_path_name(),bool(parents) and all('NoctGlow' not in p for p in parents))
            if action in ['Off','Low']:check(action+' / glow and spores disabled',all(r['glow']==0 and r['components']==0 for r in rr))
            if action in ['Off','On','Low','High','NoLumen','Lumen']:
                check(action+' / cultivation data unchanged',[[[p['growth'],p['water']] for p in r['plants']] for r in before]==[[[p['growth'],p['water']] for p in r['plants']] for r in rr])
            if action in ['On','High']:check(action+' / restored glow without transform changes',all(r['glow']>.6 for r in rr) and [[p['transform'] for p in r['plants']] for r in before]==[[p['transform'] for p in r['plants']] for r in rr])
            if action in ['Clear','Destroy']:check(action+' / no orphan spore or candidate',all(r['components']==0 and r['candidates']==0 for r in rr))
            if action in ['Day','Night22','Night02','Off','On','NoLumen','Low']:
                check(action+' / screenshot',J.capture_frame(server,str(OUT/'captures'/(action.lower()+'.png'))))
            index+=1;phase='act';start=time.monotonic();return
        if phase=='join' and len(ws)==4 and elapsed>10:
            freeze(ws);rr=rows(ws);result['samples'].append(dict(action='LateJoin',peers=rr))
            check('Late join / receives all 16 Noctiflores, shader and night state',all(len(r['plants'])==16 and r['candidates']==16 and r['trade'] and r['glow']>.6 for r in rr))
            check('Listen server / global budget not doubled',all(r['components']<=2 and r['particles']<=12 for r in rr))
            levels.editor_request_end_play();phase='stopped';start=time.monotonic();return
        if phase=='return' and len(ws)==4 and elapsed>10:
            freeze(ws);check('Find and freeze restored fixtures',V.step(server,'Freeze'));rr=rows(ws);result['samples'].append(dict(action='Return',peers=rr))
            check('Restore / saved mesh stages and transforms',all(sorted(p['mesh']+p['transform'] for p in r['plants'])==sorted(p['mesh']+p['transform'] for p in saved[0]['plants']) for r in rr))
            check('Restore / 16 glow bindings on four peers',all(len(r['plants'])==16 and r['glow']>.6 for r in rr))
            check('Restore / water and growth unchanged',all(sorted((p['growth'],p['water']) for p in r['plants'])==sorted((p['growth'],p['water']) for p in saved[0]['plants']) for r in rr))
            # Native harvest and planting regressions are run separately using the 3B harness.
            check('Maximum observed particles within cap',0<result['peakParticles']<=12);finish()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
