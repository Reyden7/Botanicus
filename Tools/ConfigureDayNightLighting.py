"""Install/update lighting using supported Unreal Editor APIs. Saves ONLY this map.

Run after compiling BotanicusEditor, with the editor closed for commandlet use:
UnrealEditor-Cmd.exe Botanicus.uproject -run=pythonscript -script=<this file> -unattended -nullrhi -nosound
This intentionally does not save Engine assets, Blueprints, meshes or materials.
"""
import json
from pathlib import Path
import unreal

MAP = '/Game/Botanicus/Maps/Lvl_BuildingTest'
DIRECTOR_LABEL = 'BotanicusAmbienceDirector'
# Coordinates are level-specific. No collision, mesh or gameplay volume is added.
LIGHTS = [
    ('Ambience_Interior_Sales', (-9107.0,-8468.0,360.0), 650.0, True),
    ('Ambience_Interior_Checkout', (-9331.0,-7262.0,360.0), 500.0, True),
    ('Ambience_Interior_Preparation', (-8000.0,-8200.0,360.0), 600.0, True),
    ('Ambience_Clandestine', (-6400.0,-7000.0,330.0), 450.0, False),
]

def main():
    level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    assert level.load_level(MAP), 'Could not load target map'
    all_actors = actors.get_all_level_actors()
    by_name = {a.get_name(): a for a in all_actors}
    by_label = {a.get_actor_label(): a for a in all_actors}
    cls = unreal.load_class(None, '/Script/Botanicus.BotanicusAmbienceDirector')
    assert cls, 'Compile BotanicusEditor before running this script'
    cube = unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap')
    assert isinstance(cube, unreal.TextureCube), 'Expected existing engine cubemap'
    required = ['GlobalLight','SkyLight_0','SkySphere','ExponentialHeightFog','GlobalPostProcess']
    assert all(name in by_name for name in required), 'Environment differs: inspect before installing'
    before = {a.get_path_name(): a.get_class().get_path_name() for a in all_actors}
    with unreal.ScopedEditorTransaction('Configure Botanicus day/night lighting'):
        director = by_label.get(DIRECTOR_LABEL)
        if director:
            assert isinstance(director, unreal.BotanicusAmbienceDirector), 'Label belongs to a different actor'
        else:
            director = actors.spawn_actor_from_class(cls, unreal.Vector(-8500,-8200,500))
            director.set_actor_label(DIRECTOR_LABEL)
            director.set_folder_path('Render/BotanicusAmbience')
        for field,name in [('sun','GlobalLight'),('sky_light','SkyLight_0'),('legacy_sky_sphere','SkySphere'),('height_fog','ExponentialHeightFog'),('exposure_volume','GlobalPostProcess')]:
            director.set_editor_property(field, by_name[name])

        # Stable ambient source: convolution at load, no runtime scene recapture.
        sky = by_name['SkyLight_0'].get_component_by_class(unreal.SkyLightComponent)
        sky.set_mobility(unreal.ComponentMobility.MOVABLE)
        sky.set_editor_property('source_type', unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        sky.set_editor_property('cubemap',cube)
        sky.set_editor_property('real_time_capture',False)
        sky.set_editor_property('cast_shadows',True)
        interiors=[]; clandestine=[]
        for label,pos,radius,shadows in LIGHTS:
            light=by_label.get(label)
            if light:
                assert isinstance(light,unreal.RectLight), 'Ambience light label conflict: '+label
            else:
                light=actors.spawn_actor_from_class(unreal.RectLight,unreal.Vector(*pos),unreal.Rotator(pitch=-90.0,yaw=0.0,roll=0.0))
                light.set_actor_label(label)
                light.set_folder_path('Render/BotanicusAmbience')
            component=light.get_component_by_class(unreal.RectLightComponent)
            component.set_mobility(unreal.ComponentMobility.MOVABLE)
            component.set_editor_property('intensity_units',unreal.LightUnits.LUMENS)
            component.set_editor_property('attenuation_radius',radius)
            component.set_editor_property('source_width',120.0)
            component.set_editor_property('source_height',100.0)
            component.set_editor_property('cast_shadows',shadows)
            component.set_editor_property('max_draw_distance',2200.0)
            component.set_editor_property('max_distance_fade_range',400.0)
            (clandestine if label=='Ambience_Clandestine' else interiors).append(light)
        director.set_editor_property('interior_lights',interiors)
        director.set_editor_property('clandestine_lights',clandestine)
        settings=by_name['GlobalPostProcess'].get_editor_property('settings')
        settings.set_editor_property('indirect_lighting_intensity',1.0)
        by_name['GlobalPostProcess'].set_editor_property('settings',settings)
        director.preview_at_hour(12.0,1,False)
        assert level.save_current_level(), 'Level save failed'
    after=actors.get_all_level_actors()
    after_by_path={a.get_path_name():a for a in after}
    assert all(p in after_by_path and after_by_path[p].get_class().get_path_name()==c for p,c in before.items()), 'Existing actor identity changed'
    output=Path(unreal.Paths.project_saved_dir())/'Lighting'
    output.mkdir(parents=True,exist_ok=True)
    (output/'installation.json').write_text(json.dumps(dict(map=MAP,director=director.get_path_name(),ambient_cubemap=cube.get_path_name(),actor_count_before=len(before),actor_count_after=len(after),added=[a.get_path_name() for a in after if a.get_path_name() not in before]),indent=2),encoding='utf-8')
    unreal.log('BOTANICUS_LIGHTING_INSTALL_COMPLETE')

if __name__=='__main__':
    main()
