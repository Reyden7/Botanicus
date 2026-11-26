#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "RobotActionTest.generated.h"

class APlayerController;
class ABotanicusCharacter;
class ABotanicusBotanistBookActor;
class ABotanicusDeliveryParcelActor;
class ABotanicusGardeningRobot;
class ABotanicusPlantPotActor;

UCLASS()
class URobotActionTest : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="Validation") static bool InstallActionSlot();
	UFUNCTION(BlueprintCallable, Category="Validation") static bool VerifyActionSlot();
	UFUNCTION(BlueprintCallable, Category="Validation") static bool PrepareHotbar(ABotanicusCharacter* Pawn, bool bBook);
	UFUNCTION(BlueprintCallable, Category="Validation") static void Select(ABotanicusCharacter* Pawn, int32 Slot);
	UFUNCTION(BlueprintCallable, Category="Validation") static AActor* GetActiveItem(ABotanicusCharacter* Pawn);
	UFUNCTION(BlueprintCallable, Category="Validation") static int32 GetToolMode(ABotanicusCharacter* Pawn);
	UFUNCTION(BlueprintCallable, Category="Validation") static FVector GetActionTarget(ABotanicusCharacter* Pawn, int32 Index);
	UFUNCTION(BlueprintCallable, Category="Validation") static ABotanicusPlantPotActor* SpawnPot(APlayerController* PC, FVector Position);
	UFUNCTION(BlueprintCallable, Category="Validation") static ABotanicusGardeningRobot* SpawnGardener(APlayerController* PC, FVector Position);
	UFUNCTION(BlueprintCallable, Category="Validation") static void Mature(ABotanicusPlantPotActor* Pot);
	UFUNCTION(BlueprintCallable, Category="Validation") static FVector SoilPoint(ABotanicusPlantPotActor* Pot);
	UFUNCTION(BlueprintCallable, Category="Validation") static float SoilLevel(ABotanicusPlantPotActor* Pot);
	UFUNCTION(BlueprintCallable, Category="Validation") static bool HasSoil(ABotanicusPlantPotActor* Pot);
	UFUNCTION(BlueprintCallable, Category="Validation") static FName Planted(ABotanicusPlantPotActor* Pot);
	UFUNCTION(BlueprintCallable, Category="Validation") static float WaterLevel(ABotanicusPlantPotActor* Pot);
	UFUNCTION(BlueprintCallable, Category="Validation") static bool StageCheckout(APlayerController* PC, FVector Position);
	UFUNCTION(BlueprintCallable, Category="Validation") static void InteractPot(ABotanicusPlantPotActor* Pot, ABotanicusCharacter* Pawn, bool bStart);
	UFUNCTION(BlueprintCallable, Category="Validation") static void Read(ABotanicusBotanistBookActor* Book, ABotanicusCharacter* Pawn, bool bOpen);
	UFUNCTION(BlueprintCallable, Category="Validation") static ABotanicusDeliveryParcelActor* SpawnTestParcel(APlayerController* Controller, bool bRectangular, FVector HalfExtent, FVector Location, FName Tag);
	UFUNCTION(BlueprintCallable, Category="Validation") static void QueueInput(APlayerController* Controller, FName Action, float Amount = 0.f);
	UFUNCTION(BlueprintCallable, Category="Validation") static ABotanicusDeliveryParcelActor* GetTestCarriedParcel(ABotanicusCharacter* Character);
	UFUNCTION(BlueprintCallable, Category="Validation") static bool IsTestPlacementPreview(ABotanicusDeliveryParcelActor* Parcel);
	UFUNCTION(BlueprintCallable, Category="Validation") static int32 Coverage(ABotanicusDeliveryParcelActor* Parcel);
	UFUNCTION(BlueprintCallable, Category="Validation") static FString TapeTrace(APlayerController* PC, ABotanicusDeliveryParcelActor* Parcel);
	UFUNCTION(BlueprintCallable, Category="Validation") static bool IsCutting(ABotanicusDeliveryParcelActor* Parcel, APlayerController* PC);
};

