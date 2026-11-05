#include "Audio/BotanicusAudioCatalog.h"
float FBotanicusAudioRegion::WeightAt(const FVector& Position) const
{
	const FVector Outside = (Position-Centre).GetAbs()-Extent.GetAbs();
	const float Distance=static_cast<float>(FMath::Max3(0.0,Outside.X,FMath::Max(Outside.Y,Outside.Z)));
	return 1.0f-FMath::SmoothStep(0.0f,FMath::Max(1.0f,BlendDistance),Distance);
}
UBotanicusAudioSettings::UBotanicusAudioSettings()
{
	Catalog=TSoftObjectPtr<UBotanicusAudioCatalog>(FSoftObjectPath(TEXT("/Game/Botanicus/Audio/DA_BotanicusAudio.DA_BotanicusAudio")));
}
