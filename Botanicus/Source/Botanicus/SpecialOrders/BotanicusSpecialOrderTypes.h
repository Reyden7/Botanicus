#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Growing/BotanicusPlantCatalog.h"
#include "BotanicusSpecialOrderTypes.generated.h"

class ABotanicusVisitorCharacter;

UENUM(BlueprintType)
enum class EBotanicusSpecialOrderStatus : uint8
{
	Travelling, Offered, Active, Completed, Expired, Cancelled
};

/** Criteria are combined with AND. An unset tag does not restrict the plant. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusSpecialOrderRequirement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PlantKey = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bFilterElement = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EBotanicusPlantElement Element = EBotanicusPlantElement::Normal;
	/** 0 = standard or better, 1 = beautiful or better, 2 = exceptional. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", ClampMax="2")) int32 MinimumQuality = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RarityTag = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName MutationTag = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ColorTag = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRequirePreparedPot = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1", ClampMax="10")) int32 Quantity = 1;
	UPROPERTY(BlueprintReadOnly) int32 Delivered = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Description;
};

/** Optional metadata for actual harvest variants; absent mutations are never invented. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusSpecialOrderPlantTraits
{
	GENERATED_BODY()
	/** Override for a new harvest variant whose key does not follow the native naming. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PlantKey = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RarityTag = TEXT("Common");
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FName> MutationTags;
};

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusSpecialOrderTemplate
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FText Title;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) TArray<FBotanicusSpecialOrderRequirement> Requirements;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 MinimumShopLevel = 1;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="30.0")) float TimeLimitSeconds = 480.0f;
};

USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusSpecialOrder
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) FGuid OrderId;
	UPROPERTY(BlueprintReadOnly) int32 Number = 0;
	UPROPERTY(BlueprintReadOnly) FText Title;
	UPROPERTY(BlueprintReadOnly) TArray<FBotanicusSpecialOrderRequirement> Requirements;
	UPROPERTY(BlueprintReadOnly) EBotanicusSpecialOrderStatus Status = EBotanicusSpecialOrderStatus::Travelling;
	UPROPERTY(BlueprintReadOnly) float DeadlineServerTime = 0.0f;
	UPROPERTY(BlueprintReadOnly) float TimeLimitSeconds = 480.0f;
	UPROPERTY(BlueprintReadOnly) int32 RewardCredits = 0;
	UPROPERTY(BlueprintReadOnly) int32 ResultReputationDelta = 0;
	UPROPERTY(BlueprintReadOnly) bool bWasAccepted = false;
	/** Replicated for nearby interactions; deliberately excluded from disk saves. */
	UPROPERTY(Transient, BlueprintReadOnly) TObjectPtr<ABotanicusVisitorCharacter> Customer;

	bool IsPending() const;
	bool IsComplete() const;
};

USTRUCT()
struct BOTANICUS_API FBotanicusSpecialOrderSaveData
{
	GENERATED_BODY()
	UPROPERTY() FBotanicusSpecialOrder Order;
	UPROPERTY() float RemainingSeconds = 0.0f;
};

/** Resolved from authoritative inventory and catalogues, never from a client request. */
struct BOTANICUS_API FBotanicusSpecialOrderPlant
{
	FName PlantKey;
	EBotanicusPlantElement Element = EBotanicusPlantElement::Normal;
	int32 Quality = 0;
	FName RarityTag = TEXT("Common");
	TArray<FName> MutationTags;
	FName ColorTag;
	bool bPreparedPot = false;
};

BOTANICUS_API bool MatchesBotanicusSpecialOrder(
	const FBotanicusSpecialOrderRequirement& Requirement,
	const FBotanicusSpecialOrderPlant& Plant);
BOTANICUS_API int32 FindBotanicusSpecialOrderDeliveryLine(
	const TArray<FBotanicusSpecialOrderRequirement>& Requirements,
	const FBotanicusSpecialOrderPlant& Plant);
BOTANICUS_API FString GetBotanicusSpecialOrderElementLabel(EBotanicusPlantElement Element);

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Botanicus Special Orders"))
class BOTANICUS_API UBotanicusSpecialOrderSettings : public UDeveloperSettings
{
	GENERATED_BODY()
public:
	virtual FName GetCategoryName() const override { return TEXT("Game"); }
	UPROPERTY(Config, EditAnywhere, Category="Frequency") bool bEnabled = true;
	UPROPERTY(Config, EditAnywhere, Category="Frequency", meta=(ClampMin="0.0", ClampMax="1.0")) float VisitorChance = 0.25f;
	UPROPERTY(Config, EditAnywhere, Category="Frequency", meta=(ClampMin="0.0")) float OfferIntervalSeconds = 120.0f;
	UPROPERTY(Config, EditAnywhere, Category="Frequency", meta=(ClampMin="1", ClampMax="4")) int32 MaximumConcurrentOrders = 2;
	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="10.0")) float AcceptanceWaitSeconds = 120.0f;
	UPROPERTY(Config, EditAnywhere, Category="Timing", meta=(ClampMin="30.0")) float MinimumProductionSeconds = 360.0f;
	UPROPERTY(Config, EditAnywhere, Category="Rewards", meta=(ClampMin="1.0", ClampMax="5.0")) float RewardMultiplier = 1.5f;
	/** Only configure variants that can actually be obtained in the game. */
	UPROPERTY(Config, EditAnywhere, Category="Plant variants") TMap<FName, FBotanicusSpecialOrderPlantTraits> ItemTraits;
	/** Optional requests; the generator also checks that every criterion has a matching item. */
	UPROPERTY(Config, EditAnywhere, Category="Requests") TArray<FBotanicusSpecialOrderTemplate> Templates;
};
