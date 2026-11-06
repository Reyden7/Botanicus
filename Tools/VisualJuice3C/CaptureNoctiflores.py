import unreal,time,json,traceback
from pathlib import Path
OUT=Path(__file__).parent;FRAMES=OUT/'captures'/'comparison_frames';FRAMES.mkdir(parents=True,exist_ok=True)
assert str(Path(unreal.Paths.project_saved_dir()).resolve()).lower().startswith(str((OUT/'isolated_user').resolve()).lower())
V=unreal.BotanicusNoctifloreValidation;J=unreal.BotanicusJuiceValidation
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem);levels.load_level('/Game/Botanicus/Maps/Lvl_BuildingTest')
s=unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.LevelEditorPlaySettings'))
for k,v in [('PlayNumberOfClients',4),('RunUnderOneProcess',True),('EnableGameSound',True),('CreateAudioDeviceForEveryPlayer',True),('ClientWindowWidth',1280),('ClientWindowHeight',720)]:s.set_editor_property(k,v)
unreal.get_default_object(unreal.load_class(None,'/Script/UnrealEd.EditorLoadingSavingSettings')).set_editor_property('bAutoSaveEnable',False)
levels.editor_request_begin_play();phase='start';start=time.monotonic();index=0;last=0
result={'frames':[],'errors':[]}
def worlds():return sorted([w for w in unreal.ObjectIterator(unreal.World) if 'UEDPIE_' in w.get_path_name() and unreal.GameplayStatics.get_game_state(w)],key=lambda w:w.get_path_name())
def clean_view(w):
    for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
        if a.actor_has_tag('BotanicusEquippedPreview'):a.set_actor_hidden_in_game(True)
        if isinstance(a,unreal.BotanicusIllegalPlanterActor):
            for cls in [unreal.WidgetComponent,unreal.TextRenderComponent]:
                for c in a.get_components_by_class(cls):c.set_visibility(False)
def finish():
    result['passed']=not result['errors'];levels.editor_request_end_play();(OUT/'capture_noctiflores.json').write_text(json.dumps(result,indent=2),encoding='utf-8');unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor()
def tick(dt):
    global phase,start,index,last
    try:
        ws=worlds();elapsed=time.monotonic()-start
        if elapsed>100:raise RuntimeError('Timeout '+phase)
        if len(ws)!=4:return
        server=next(w for w in ws if unreal.GameplayStatics.get_game_mode(w))
        if phase=='start' and elapsed>8:
            for group in ['Effects','GlobalIllumination','Reflection','Shading','Shadow','PostProcess']:
                unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality 3',unreal.GameplayStatics.get_player_controller(server,0))
            J.step(server,'Quiesce');assert V.step(server,'Setup')
            unreal.SystemLibrary.execute_console_command(server,'SetTime 22',unreal.GameplayStatics.get_player_controller(server,0))
            for w in ws:
                unreal.GameplayStatics.get_game_state(w).set_actor_tick_enabled(False)
                for a in unreal.GameplayStatics.get_all_actors_of_class(w,unreal.Actor):
                    if any(t in a.get_class().get_name() for t in ['Visitor','IllegalCustomer','IllegalTradeManager']):a.destroy_actor()
                    # Only presentation capture: hide existing labels/widgets, no game source changes.
                    if isinstance(a,unreal.BotanicusIllegalPlanterActor):
                        for cls in [unreal.WidgetComponent,unreal.TextRenderComponent]:
                            for c in a.get_components_by_class(cls):c.set_visibility(False)
            assert V.step(server,'Off');phase='before';start=time.monotonic()
        elif phase=='before' and elapsed>4:
            clean_view(server)
            phase='before_capture';start=time.monotonic()
        elif phase=='before_capture' and elapsed>.4:
            assert J.capture_frame(server,str(OUT/'captures'/'comparison_before.png'));assert V.step(server,'On');phase='settle';start=time.monotonic()
        elif phase=='settle' and elapsed>4:phase='record';start=time.monotonic()
        elif phase=='record' and time.monotonic()-last>.2:
            clean_view(server)
            assert J.capture_frame(server,str(FRAMES/('frame_%03d.png'%index)))
            result['frames'].append(dict(index=index,seconds=elapsed,worldSeconds=unreal.GameplayStatics.get_time_seconds(server)));index+=1;last=time.monotonic()
            if index>=40:phase='night22';start=time.monotonic()
        elif phase=='night22' and elapsed>1:
            clean_view(server);assert J.capture_frame(server,str(OUT/'captures'/'scene_22h.png'))
            unreal.SystemLibrary.execute_console_command(server,'SetTime 2',unreal.GameplayStatics.get_player_controller(server,0));phase='night02';start=time.monotonic()
        elif phase=='night02' and elapsed>4:
            clean_view(server);assert J.capture_frame(server,str(OUT/'captures'/'scene_02h.png'))
            unreal.SystemLibrary.execute_console_command(server,'SetTime 12',unreal.GameplayStatics.get_player_controller(server,0));phase='day12';start=time.monotonic()
        elif phase=='day12' and elapsed>5:
            clean_view(server);assert J.capture_frame(server,str(OUT/'captures'/'scene_12h.png'))
            unreal.SystemLibrary.execute_console_command(server,'SetTime 22',unreal.GameplayStatics.get_player_controller(server,0))
            for group in ['Effects','GlobalIllumination','Reflection','Shading','Shadow','PostProcess']:
                unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality 0',unreal.GameplayStatics.get_player_controller(server,0))
            phase='low';start=time.monotonic()
        elif phase=='low' and elapsed>4:
            clean_view(server);assert J.capture_frame(server,str(OUT/'captures'/'scene_low.png'))
            for group in ['Effects','Shading','Shadow','PostProcess']:
                unreal.SystemLibrary.execute_console_command(server,'sg.'+group+'Quality 3',unreal.GameplayStatics.get_player_controller(server,0))
            phase='no_lumen';start=time.monotonic()
        elif phase=='no_lumen' and elapsed>4:
            clean_view(server);assert J.capture_frame(server,str(OUT/'captures'/'scene_no_lumen.png'));finish()
    except Exception:result['errors'].append(traceback.format_exc());finish()
handle=unreal.register_slate_post_tick_callback(tick)
