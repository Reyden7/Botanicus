#pragma once
#include "CoreMinimal.h"
#include "Delivery/BotanicusPlaceableItemActor.h"
#include "Sales/BotanicusSalesShelfTypes.h"
#include "BotanicusSalesShelfActor.generated.h"
class ABotanicusSalesDisplayActor;
class APlayerController;
/** Modular wall furniture. Every product slot is the original sales-display actor. */
UCLASS()
class BOTANICUS_API ABotanicusSalesShelfActor : public ABotanicusPlaceableItemActor
{
    GENERATED_BODY()
public:
    ABotanicusSalesShelfActor();
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
    static int32 CapacityForKey(FName Key);
    static float LengthForCapacity(int32 Count) {return 48.f*FMath::Clamp(Count,1,6)+8.f;}
    static bool ResolveWallPlacement(APlayerController* PC,FName Key,FTransform& Out,const AActor* Ignored=nullptr);
    int32 GetCapacity() const {return Capacity;}
    ABotanicusSalesDisplayActor* GetSlot(int32 Index) const;
    ABotanicusSalesDisplayActor* FindSlotAtPoint(const FVector& WorldPoint) const;
    FVector GetSlotLocalLocation(int32 Index) const;
    bool HasContents() const;
    TArray<FBotanicusSavedSalesShelfSlot> CaptureSlots() const;
    void RestoreSlots(const TArray<FBotanicusSavedSalesShelfSlot>& Saved);
    UPROPERTY(EditAnywhere,BlueprintReadOnly,ReplicatedUsing=OnRep_Capacity,Category="Sales Shelf",meta=(ClampMin="1",ClampMax="6")) int32 Capacity=1;
    UPROPERTY(VisibleAnywhere,BlueprintReadOnly,Category="Sales Shelf") TArray<TObjectPtr<UStaticMeshComponent>> Brackets;
protected:
    virtual void ApplyItemDefinition() override;
private:
    UFUNCTION() void OnRep_Capacity();
    void RefreshLayout();
    void EnsureSlots();
    UPROPERTY(Replicated) TArray<TObjectPtr<ABotanicusSalesDisplayActor>> Slots;
};
