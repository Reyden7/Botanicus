// Copyright Epic Games, Inc. All Rights Reserved.

#include "Water/BotanicusWaterSettings.h"

#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"

UBotanicusWaterSettings::UBotanicusWaterSettings()
{
	WetnessDecalMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Materials/Effects/M_BotanicusWetness.M_BotanicusWetness")));
	PuddleMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(
		TEXT("/Engine/BasicShapes/Plane.Plane")));
	PuddleMaterial = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(
		TEXT("/Game/Botanicus/Materials/Effects/M_BotanicusPuddleCartoon.M_BotanicusPuddleCartoon")));
	ImpactNiagara = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/Botanicus/VFX/Watering/NS_BotanicusWateringImpact.NS_BotanicusWateringImpact")));
	RunoffNiagara = TSoftObjectPtr<UNiagaraSystem>(FSoftObjectPath(
		TEXT("/Game/Botanicus/VFX/Watering/NS_BotanicusWaterRunoff.NS_BotanicusWaterRunoff")));
}
