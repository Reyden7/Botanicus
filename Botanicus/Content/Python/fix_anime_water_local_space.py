import unreal

ok = unreal.BotanicusCommandWidgetBuilder.make_anime_water_local_space()
unreal.log("BOTANICUS_ANIME_WATER_LOCAL_SPACE=" + str(ok))
if not ok:
    raise RuntimeError("Unable to update NS_AnimeWater emitters")
