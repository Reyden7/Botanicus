#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BotanicusEmoteCatalog.generated.h"

class UAnimSequence;
class UAnimSequenceBase;
class USoundAttenuation;
class USoundWave;
class USkeleton;
class UTexture2D;

USTRUCT(BlueprintType)
struct FBotanicusEmoteCategory
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Id;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;
};

USTRUCT(BlueprintType)
struct FBotanicusEmoteEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName EmoteId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Icon;

	/** Temporary emblem displayed when the Icon field is empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText PlaceholderGlyph;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UAnimSequenceBase> Animation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio")
	TObjectPtr<USoundWave> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0", ClampMax="2"))
	float SoundVolume = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0.125", ClampMax="4"))
	float SoundPitch = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0", Units="s"))
	float SoundDelay = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio", meta=(ClampMin="0.15", ClampMax="0.25", Units="s"))
	float SoundFadeOut = .20f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0.05"))
	float PlayRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="s"))
	float BlendInDuration = .12f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", Units="s"))
	float BlendOutDuration = .10f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInterruptOnMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	bool bInterruptOnJump = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	int32 SortOrder = 0;

	float GetEffectivePlayRate() const;
	bool IsPlayable(const USkeleton* RobotSkeleton) const;
};

UCLASS(BlueprintType)
class BOTANICUS_API UBotanicusEmoteCatalog : public UDataAsset
{
	GENERATED_BODY()

public:
	static const FName FullBodySlot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Emotes")
	TArray<FBotanicusEmoteCategory> Categories;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Emotes", meta=(TitleProperty="DisplayName"))
	TArray<FBotanicusEmoteEntry> Emotes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Emotes|Audio")
	TObjectPtr<USoundAttenuation> EmoteAttenuation;

	const FBotanicusEmoteEntry* Find(FName EmoteId) const;
	const UAnimSequence* GetMotionSequence(const FBotanicusEmoteEntry& Entry);

private:
	TMap<TWeakObjectPtr<const UAnimSequence>, bool> MovingSequences;
};

USTRUCT()
struct FBotanicusEmoteState
{
	GENERATED_BODY()

	UPROPERTY()
	FName EmoteId;

	UPROPERTY()
	double StartedAt = 0.0;

	UPROPERTY()
	uint16 Revision = 0;

	UPROPERTY()
	float InitialYaw = 0.f;
};
