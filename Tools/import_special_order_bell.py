"""Run with UnrealEditor-Cmd -run=pythonscript -script=<this file>."""
from pathlib import Path
import unreal

source = Path(__file__).resolve().parents[1] / "SourceArt/Audio/freesound_community-bell-98033.mp3"
destination = "/Game/Botanicus/Audio"
sound_path = destination + "/S_SpecialOrderBell"
attenuation_path = destination + "/ATT_SpecialOrderBell"
assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

if not source.is_file():
    raise RuntimeError(f"Missing source audio: {source}")
sound = assets.load_asset(sound_path) if assets.does_asset_exist(sound_path) else None
if sound is None:
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", str(source))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", "S_SpecialOrderBell")
    task.set_editor_property("automated", True)
    task.set_editor_property("save", True)
    task.set_editor_property("factory", unreal.SoundFactory())
    tools.import_asset_tasks([task])
    sound = assets.load_asset(sound_path)
if not isinstance(sound, unreal.SoundWave):
    raise RuntimeError("Bell import did not produce a SoundWave")

attenuation = assets.load_asset(attenuation_path) if assets.does_asset_exist(attenuation_path) else None
if attenuation is None:
    attenuation = tools.create_asset("ATT_SpecialOrderBell", destination,
                                    unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = unreal.SoundAttenuationSettings()
    settings.set_editor_property("attenuate", True)
    settings.set_editor_property("spatialize", True)
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(700.0, 0.0, 0.0))
    settings.set_editor_property("falloff_distance", 2300.0)
    attenuation.set_editor_property("attenuation", settings)
    if not assets.save_loaded_asset(attenuation):
        raise RuntimeError("Could not save bell attenuation")
sound.set_editor_property("looping", False)
sound.set_editor_property("attenuation_settings", attenuation)
if not assets.save_loaded_asset(sound):
    raise RuntimeError("Could not save imported bell")
duration = sound.get_editor_property("duration")
if duration <= 0:
    raise RuntimeError("Imported bell has no audio duration")
unreal.log(f"SPECIAL_ORDER_BELL_IMPORTED {sound.get_path_name()} duration={duration:.3f}s")
