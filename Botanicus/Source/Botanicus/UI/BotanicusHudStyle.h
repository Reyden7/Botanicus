// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"

namespace BotanicusHudStyle
{
inline UTexture2D* LoadTexture(const TCHAR* AssetName)
{
	return LoadObject<UTexture2D>(
		nullptr,
		*FString::Printf(
			TEXT("/Game/Botanicus/UI/HUD/Textures/%s.%s"),
			AssetName,
			AssetName));
}

inline const FLinearColor& PrimaryText()
{
	static const FLinearColor Color(0.94f, 0.93f, 0.84f, 1.0f);
	return Color;
}

inline const FLinearColor& CompletedText()
{
	static const FLinearColor Color(0.62f, 0.91f, 0.35f, 1.0f);
	return Color;
}
}
