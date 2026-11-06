#pragma once
#include "CoreMinimal.h"
#include "BotanicusCheckoutTypes.generated.h"

UENUM(BlueprintType)
enum class EBotanicusCheckoutPhase : uint8 { Idle, Opening, AwaitingCollection, Collecting, Closing };

/** The receipt is authoritative; rendered coins never carry economic value. */
USTRUCT(BlueprintType)
struct BOTANICUS_API FBotanicusCheckoutReceipt
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FGuid TransactionId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FGuid PlantInstanceId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FGuid CustomerId;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) FName PlantItemKey;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) int32 Amount=0;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) EBotanicusCheckoutPhase Phase=EBotanicusCheckoutPhase::Idle;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly) bool bCredited=false;
    UPROPERTY() double PhaseStartedAt=0;
    UPROPERTY() double OpenedAt=0;
    UPROPERTY() double CollectedAt=0;
    UPROPERTY() float OpenSeconds=.45f;
    UPROPERTY() float CloseSeconds=.35f;
    UPROPERTY() float DepositSeconds=.25f;
    UPROPERTY() float ClosePauseSeconds=.08f;
    UPROPERTY() float CoinFlightSeconds=.26f;
    UPROPERTY() float CoinIntervalSeconds=.08f;
    UPROPERTY() float CollectionSeconds=.26f;
    UPROPERTY() int32 VisibleGoldCoins=0;
    UPROPERTY() int32 VoucherCredits=0;
    UPROPERTY() FVector ScanOrigin=FVector::ZeroVector;
    UPROPERTY() FVector ScanTarget=FVector::ZeroVector;
    UPROPERTY() FVector CustomerHand=FVector::ZeroVector;
    int32 VisualCount() const { return VisibleGoldCoins+(VoucherCredits>0 ? 1 : 0); }
};

USTRUCT()
struct BOTANICUS_API FBotanicusSavedCashRegister
{
    GENERATED_BODY()
    UPROPERTY() FName ActorName;
    UPROPERTY() FSoftClassPath ActorClass;
    UPROPERTY() FTransform Transform;
    UPROPERTY() FGuid RegisterId;
    UPROPERTY() FBotanicusCheckoutReceipt Receipt;
};
